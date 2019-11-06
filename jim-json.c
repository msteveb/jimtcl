/*
 * Copyright (c) 2015 - 2016 Svyatoslav Mishyn <juef@openmailbox.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <jim.h>

#include "jsmn/jsmn.h"

struct jmsn_state {
	Jim_Obj *nullObj;
	jsmntok_t *tok;
	int need_subst;
};

static void json_decode_dump_value(Jim_Interp *interp, struct jmsn_state *state, Jim_Obj *list, const char *json);

/**
 * Returns the current object (state->tok) as a Tcl list.
 *
 * state->tok is incremented to just past the object that was dumped.
 */
static Jim_Obj *
json_decode_dump_container(Jim_Interp *interp, struct jmsn_state *state, const char *json)
{
	int i;

	Jim_Obj *list = Jim_NewListObj(interp, NULL, 0);
	int size = state->tok->size;
	int type = state->tok->type;

	state->tok++;

	for (i = 0; i < size; i++) {
		if (type == JSMN_OBJECT) {
			/* Dump the object key */
			json_decode_dump_value(interp, state, list, json);
		}
		/* Dump the array or object value */
		json_decode_dump_value(interp, state, list, json);
	}
	return list;
}

/**
 * Appends the value at state->tok to 'list' and increments state->tok to just
 * past that token.
 */
static void
json_decode_dump_value(Jim_Interp *interp, struct jmsn_state *state, Jim_Obj *list, const char *json)
{
	Jim_Obj		*newList, *elem;
	unsigned char	 c;
	int		 len;
	const jsmntok_t *t = state->tok;

	if (t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE) {
		assert(t->start && t->end);
		len = t->end - t->start;
		c = (unsigned char)json[t->start];

		if (t->type == JSMN_STRING) {
			/* Do we need to process backslash escapes? */
			if (state->need_subst == 0 && memchr(json + t->start, '\\', len) != NULL) {
				state->need_subst = 1;
			}
			elem = Jim_NewStringObj(interp, json + t->start, len);
		} else if (c == 'n') {	/* null */
			elem = state->nullObj;
		} else if (c == 'I') {
			elem = Jim_NewStringObj(interp, "Inf", -1);
		} else if (c == '-' && json[t->start + 1] == 'I') {
			elem = Jim_NewStringObj(interp, "-Inf", -1);
		} else {		/* number, true or false */
			elem = Jim_NewStringObj(interp, json + t->start, len);
		}

		Jim_ListAppendElement(interp, list, elem);
		state->tok++;
	}
	else {
		newList = json_decode_dump_container(interp, state, json);
		Jim_ListAppendElement(interp, list, newList);
	}
}

/* Parses the options ?-null string? *state.
 * Any options not present are not set.
 *
 * Returns JIM_OK or JIM_ERR and sets an error result.
 */
static int parse_json_decode_options(Jim_Interp *interp, int argc, Jim_Obj *const argv[], struct jmsn_state *state)
{
	static const char * const options[] = { "-null", NULL };
	enum { OPT_NULL, };
	int i;

	for (i = 1; i < argc - 1; i++) {
		int option;
		if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
			return JIM_ERR;
		}
		switch (option) {
			case OPT_NULL:
				i++;
				Jim_IncrRefCount(argv[i]);
				Jim_DecrRefCount(interp, state->nullObj);
				state->nullObj = argv[i];
				break;
		}
	}

	if (i != argc - 1) {
		Jim_WrongNumArgs(interp, 1, argv,
			"?-null nullvalue? json");
		return JIM_ERR;
	}

	return JIM_OK;
}

static jsmntok_t *
json_decode_tokenize(Jim_Interp *interp, const char *json, size_t len)
{
	jsmntok_t	*t;
	jsmn_parser	 parser;
	int n;

	jsmn_init(&parser);
	n = jsmn_parse(&parser, json, len, NULL, 0);

error:
	switch (n) {
		case JSMN_ERROR_INVAL:
			Jim_SetResultString(interp, "invalid JSON string", -1);
			return NULL;

		case JSMN_ERROR_PART:
			Jim_SetResultString(interp, "truncated JSON string", -1);
			return NULL;

		case 0:
			Jim_SetResultString(interp, "root element must be an object or an array", -1);
			return NULL;

		default:
			break;
	}

	if (n < 0) {
		return NULL;
	}

	t = Jim_Alloc(n * sizeof(*t));

	jsmn_init(&parser);
	n = jsmn_parse(&parser, json, len, t, n);
	if (t->type != JSMN_OBJECT && t->type != JSMN_ARRAY) {
		n = 0;
	}
	if (n <= 0) {
		Jim_Free(t);
		goto error;
	}

	return t;
}

static int
json_decode(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
	Jim_Obj		*list;
	jsmntok_t	*tokens;
	const char	*json;
	int		 len;
	int ret = JIM_ERR;

	struct jmsn_state state;

	state.need_subst = 0;
	state.nullObj = Jim_NewStringObj(interp, "null", -1);
	Jim_IncrRefCount(state.nullObj);

	if (parse_json_decode_options(interp, argc, argv, &state) != JIM_OK) {
		goto done;
	}

	json = Jim_GetString(argv[argc - 1], &len);

	if (!len) {
		Jim_SetResultString(interp, "empty JSON string", -1);
		goto done;
	}
	if ((tokens = json_decode_tokenize(interp, json, len)) == NULL) {
		goto done;
	}
	state.tok = tokens;

	list = json_decode_dump_container(interp, &state, json);
	Jim_Free(tokens);
	ret = JIM_OK;

	if (state.need_subst) {
		/* Subsitute backslashes in the returned dictionary.
		 * Need to be careful of refcounts.
		 * Note that Jim_SubstObj() supports a few more escapes than
		 * JSON requires, but should give the same result for all legal escapes.
		 */
		Jim_Obj *newList;
		Jim_IncrRefCount(list);
		Jim_SubstObj(interp, list, &newList, JIM_SUBST_FLAG | JIM_SUBST_NOCMD | JIM_SUBST_NOVAR);
		Jim_SetResult(interp, newList);
		Jim_DecrRefCount(interp, list);
	}
	else {
		Jim_SetResult(interp, list);
	}

done:
	Jim_DecrRefCount(interp, state.nullObj);

	return ret;
}

int
Jim_jsonInit(Jim_Interp *interp)
{
	if (Jim_PackageProvide(interp, "json", "1.0", JIM_ERRMSG) != JIM_OK) {
		return JIM_ERR;
	}

	Jim_CreateCommand(interp, "json::decode", json_decode, NULL, NULL);
	/* Load the Tcl implementation of the json encoder if possible */
	Jim_PackageRequire(interp, "jsonencode", 0);
	return JIM_OK;
}
