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

struct json_state {
	Jim_Obj *nullObj;
	jsmntok_t *tok;
	Jim_Obj *schemaObj;
	int schema;
	int need_subst;
};

/* These are all the schema types we support */
typedef enum {
	JSON_NONE = -1,
	JSON_BOOL = 0,
	JSON_OBJ,
	JSON_LIST,
	JSON_MIXED,
	JSON_STR,
	JSON_NUM,
} json_schema_t;


static void json_decode_dump_value(Jim_Interp *interp, struct json_state *state, Jim_Obj *list, const char *json);

/**
 * Start a new subschema. Returns the previous schemaObj.
 * Does nothing and returns NULL if -schema is not enabled.
 */
static Jim_Obj *json_decode_schema_push(Jim_Interp *interp, struct json_state *state)
{
	Jim_Obj *prevSchemaObj = NULL;
	if (state->schema) {
		prevSchemaObj = state->schemaObj;
		state->schemaObj = Jim_NewListObj(interp, NULL, 0);
		Jim_IncrRefCount(state->schemaObj);
	}
	return prevSchemaObj;
}

/**
 * Combines the current schema with the previous schema, prevSchemaObj
 * returned by json_decode_schema_push().
 * Does nothing if -schema is not enabled.
 */
static void json_decode_schema_pop(Jim_Interp *interp, struct json_state *state, Jim_Obj *prevSchemaObj)
{
	if (state->schema) {
		Jim_ListAppendElement(interp, prevSchemaObj, state->schemaObj);
		Jim_DecrRefCount(interp, state->schemaObj);
		state->schemaObj = prevSchemaObj;
	}
}

/**
 * Appends the schema type to schemaObj based on 'type'
 */
static void json_decode_add_schema_type(Jim_Interp *interp, Jim_Obj *schemaObj, json_schema_t type)
{
	static const char * const schema_names[] = {
		"bool",
		"obj",
		"list",
		"mixed",
		"str",
		"num",
	};
	assert(type < sizeof(schema_names) / sizeof(*schema_names));

	/* XXX: Could optimise storage of these strings with reference counting*/
	Jim_ListAppendElement(interp, schemaObj, Jim_NewStringObj(interp, schema_names[type], -1));
}

static json_schema_t json_decode_get_type(const jsmntok_t *tok, const char *json)
{
	switch (tok->type) {
		case JSMN_PRIMITIVE:
			assert(json);
			if (json[tok->start] == 't' || json[tok->start] == 'f') {
				return JSON_BOOL;
			}
			return JSON_NUM;
		case JSMN_STRING:
			return JSON_STR;
		case JSMN_OBJECT:
			return JSON_OBJ;
		case JSMN_ARRAY:
			/* Return mixed by default - need other checks to determine list instead */
			return JSON_MIXED;
		default:
			fprintf(stderr, "tok->type=%d, token=%s\n", tok->type, json + tok->start);
			assert(0);
	}
}

/**
 * Returns the current object (state->tok) as a Tcl list.
 *
 * state->tok is incremented to just past the object that was dumped.
 */
static Jim_Obj *
json_decode_dump_container(Jim_Interp *interp, struct json_state *state, const char *json)
{
	int i;

	Jim_Obj *list = Jim_NewListObj(interp, NULL, 0);
	int size = state->tok->size;
	int type = state->tok->type;
	int subtypes = 1;

	if (state->schemaObj) {
		/* Figure out the type to use for the container - obj, list or mixed, and whether to include subtypes */
		if (type == JSMN_ARRAY) {
			/* If every element of the array is of the same primitive schema type (str, bool or num),
			 * we can use "list", otherwise need to use "mixed"
			 */
			subtypes = 0;
			if (size == 0) {
				/* Special case "[]" */
				json_decode_add_schema_type(interp, state->schemaObj, JSON_LIST);
			}
			else {
				json_schema_t list_type = json_decode_get_type(&state->tok[1], json);

				if (list_type == JSON_BOOL || list_type == JSON_STR || list_type == JSON_NUM) {
					for (i = 2; i <= size; i++) {
						if (json_decode_get_type(state->tok + i, json) != list_type) {
							/* Can't use list */
							subtypes = 1;
							break;
						}
					}
					if (subtypes == 0) {
						/* We can use list, so don't need subtypes */
						json_decode_add_schema_type(interp, state->schemaObj, JSON_LIST);
						json_decode_add_schema_type(interp, state->schemaObj, list_type);
					}
				}
			}
		}
		if (subtypes) {
			json_decode_add_schema_type(interp, state->schemaObj, json_decode_get_type(state->tok, json));
		}
	}

	state->tok++;

	for (i = 0; i < size; i++) {
		if (type == JSMN_OBJECT) {
			/* Dump the object key */
			if (state->schema) {
				Jim_ListAppendElement(interp, state->schemaObj, Jim_NewStringObj(interp, json + state->tok->start, state->tok->end - state->tok->start));
			}
			json_decode_dump_value(interp, state, list, json);
		}

		if (state->schemaObj && subtypes) {
			if (state->tok->type == JSMN_STRING || state->tok->type == JSMN_PRIMITIVE) {
				json_decode_add_schema_type(interp, state->schemaObj, json_decode_get_type(state->tok, json));
			}
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
json_decode_dump_value(Jim_Interp *interp, struct json_state *state, Jim_Obj *list, const char *json)
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
		Jim_Obj *prevSchemaObj = json_decode_schema_push(interp, state);
		newList = json_decode_dump_container(interp, state, json);
		Jim_ListAppendElement(interp, list, newList);
		json_decode_schema_pop(interp, state, prevSchemaObj);
	}
}

/* Parses the options ?-null string? ?-schema? *state.
 * Any options not present are not set.
 *
 * Returns JIM_OK or JIM_ERR and sets an error result.
 */
static int parse_json_decode_options(Jim_Interp *interp, int argc, Jim_Obj *const argv[], struct json_state *state)
{
	static const char * const options[] = { "-null", "-schema", NULL };
	enum { OPT_NULL, OPT_SCHEMA, };
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

			case OPT_SCHEMA:
				state->schema = 1;
				break;
		}
	}

	if (i != argc - 1) {
		Jim_WrongNumArgs(interp, 1, argv,
			"?-null nullvalue? ?-schema? json");
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

/**
 * json::decode returns the decoded data structure.
 *
 * If -schema is specified, returns a list of {data schema}
 */
static int
json_decode(Jim_Interp *interp, int argc, Jim_Obj *const argv[])
{
	Jim_Obj		*list;
	jsmntok_t	*tokens;
	const char	*json;
	int		 len;
	int ret = JIM_ERR;

	struct json_state state;

	state.need_subst = 0;
	state.schema = 0;
	state.schemaObj = NULL;
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
	json_decode_schema_push(interp, &state);

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

	if (state.schemaObj) {
		Jim_Obj *resultObj = Jim_NewListObj(interp, NULL, 0);
		Jim_ListAppendElement(interp, resultObj, Jim_GetResult(interp));
		Jim_ListAppendElement(interp, resultObj, state.schemaObj);
		Jim_SetResult(interp, resultObj);
		Jim_DecrRefCount(interp, state.schemaObj);
	}

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
