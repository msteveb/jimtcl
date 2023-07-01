/*
 * Copyright (c) 2015 - 2016 Svyatoslav Mishyn <juef@openmailbox.org>
 * Copyright (c) 2019 Steve Bennett <steveb@workware.net.au>
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

/* These are all the schema types we support */
typedef enum {
	JSON_BOOL,
	JSON_OBJ,
	JSON_LIST,
	JSON_MIXED,
	JSON_STR,
	JSON_NUM,
	JSON_MAX_TYPE,
} json_schema_t;

struct json_state {
	Jim_Obj *fileNameObj;
	int line;
	Jim_Obj *nullObj;
	const char *json;
	jsmntok_t *tok;
	int need_subst;
	/* The following are used for -schema */
	int enable_schema;
	int enable_index;
	Jim_Obj *schemaObj;
	Jim_Obj *schemaTypeObj[JSON_MAX_TYPE];
};

static void json_decode_dump_value(Jim_Interp *interp, struct json_state *state, Jim_Obj *list);

/**
 * Start a new subschema. Returns the previous schemaObj.
 * Does nothing and returns NULL if -schema is not enabled.
 */
static Jim_Obj *json_decode_schema_push(Jim_Interp *interp, struct json_state *state)
{
	Jim_Obj *prevSchemaObj = NULL;
	if (state->enable_schema) {
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
	if (state->enable_schema) {
		Jim_ListAppendElement(interp, prevSchemaObj, state->schemaObj);
		Jim_DecrRefCount(interp, state->schemaObj);
		state->schemaObj = prevSchemaObj;
	}
}

/**
 * Appends the schema type to state->schemaObj based on 'type'
 */
static void json_decode_add_schema_type(Jim_Interp *interp, struct json_state *state, json_schema_t type)
{
	static const char * const schema_names[] = {
		"bool",
		"obj",
		"list",
		"mixed",
		"str",
		"num",
	};
	assert(type >= 0 && type < JSON_MAX_TYPE);
	/* Share multiple instances of the same type */
	if (state->schemaTypeObj[type] == NULL) {
		state->schemaTypeObj[type] = Jim_NewStringObj(interp, schema_names[type], -1);
	}
	Jim_ListAppendElement(interp, state->schemaObj, state->schemaTypeObj[type]);
}

/**
 * Returns the schema type for the given token.
 * There is a one-to-one correspondence except for JSMN_PRIMITIVE
 * which will return JSON_BOOL for true, false and JSON_NUM otherise.
 */
static json_schema_t json_decode_get_type(const jsmntok_t *tok, const char *json)
{
	switch (tok->type) {
		case JSMN_PRIMITIVE:
			assert(json);
			if (json[tok->start] == 't' || json[tok->start] == 'f') {
				return JSON_BOOL;
			}
			return JSON_NUM;
		case JSMN_OBJECT:
			return JSON_OBJ;
		case JSMN_ARRAY:
			/* Return mixed by default - need other checks to select list instead */
			return JSON_MIXED;
		case JSMN_STRING:
		default:
			return JSON_STR;
	}
}

/**
 * Returns the current object (state->tok) as a Tcl list.
 *
 * state->tok is incremented to just past the object that was dumped.
 */
static Jim_Obj *
json_decode_dump_container(Jim_Interp *interp, struct json_state *state)
{
	int i;
	Jim_Obj *list = Jim_NewListObj(interp, NULL, 0);
	int size = state->tok->size;
	int type = state->tok->type;
	json_schema_t container_type = JSON_OBJ; /* JSON_LIST, JSON_MIXED or JSON_OBJ */

	if (state->schemaObj) {
		/* Don't strictly need to initialise this, but some compilers can't figure out it is always
		 * assigned a value below.
		 */
		json_schema_t list_type = JSON_STR;
		/* Figure out the type to use for the container */
		if (type == JSMN_ARRAY) {
			/* If every element of the array is of the same primitive schema type (str, bool or num),
			 * we can use "list", otherwise need to use "mixed"
			 */
			container_type = JSON_LIST;
			if (size) {
				list_type = json_decode_get_type(&state->tok[1], state->json);

				if (list_type == JSON_BOOL || list_type == JSON_STR || list_type == JSON_NUM) {
					for (i = 2; i <= size; i++) {
						if (json_decode_get_type(state->tok + i, state->json) != list_type) {
							/* Can't use list */
							container_type = JSON_MIXED;
							break;
						}
					}
				}
				else {
					container_type = JSON_MIXED;
				}
			}
		}
		json_decode_add_schema_type(interp, state, container_type);
		if (container_type == JSON_LIST && size) {
			json_decode_add_schema_type(interp, state, list_type);
		}
	}

	state->tok++;

	for (i = 0; i < size; i++) {
		if (type == JSMN_OBJECT) {
			/* Dump the object key */
			if (state->enable_schema) {
				const char *p = state->json + state->tok->start;
				int len = state->tok->end - state->tok->start;
				Jim_ListAppendElement(interp, state->schemaObj, Jim_NewStringObj(interp, p, len));
			}
			json_decode_dump_value(interp, state, list);
		}

		if (state->enable_index && type == JSMN_ARRAY) {
			Jim_ListAppendElement(interp, list, Jim_NewIntObj(interp, i));
		}

		if (state->schemaObj && container_type != JSON_LIST) {
			if (state->tok->type == JSMN_STRING || state->tok->type == JSMN_PRIMITIVE) {
				json_decode_add_schema_type(interp, state, json_decode_get_type(state->tok, state->json));
			}
		}

		/* Dump the array or object value */
		json_decode_dump_value(interp, state, list);
	}

	return list;
}

/**
 * Appends the value at state->tok to 'list' and increments state->tok to just
 * past that token.
 *
 * Also appends to the schema if state->enable_schema is set.
 */
static void
json_decode_dump_value(Jim_Interp *interp, struct json_state *state, Jim_Obj *list)
{
	const jsmntok_t *t = state->tok;

	if (t->type == JSMN_STRING || t->type == JSMN_PRIMITIVE) {
		Jim_Obj	*elem;
		int len = t->end - t->start;
		const char *p = state->json + t->start;
		int set_source = 1;
		if (t->type == JSMN_STRING) {
			/* Do we need to process backslash escapes? */
			if (state->need_subst == 0 && memchr(p, '\\', len) != NULL) {
				state->need_subst = 1;
			}
			elem = Jim_NewStringObj(interp, p, len);
		} else if (p[0] == 'n') {	/* null */
			elem = state->nullObj;
			set_source = 0;
		} else if (p[0] == 'I') {
			elem = Jim_NewStringObj(interp, "Inf", -1);
		} else if (p[0] == '-' && p[1] == 'I') {
			elem = Jim_NewStringObj(interp, "-Inf", -1);
		} else {		/* number, true or false */
			elem = Jim_NewStringObj(interp, p, len);
		}
		if (set_source) {
			/* Note we need to subtract 1 because both are 1-based values */
			Jim_SetSourceInfo(interp, elem, state->fileNameObj, state->line + t->line - 1);
		}

		Jim_ListAppendElement(interp, list, elem);
		state->tok++;
	}
	else {
		Jim_Obj *prevSchemaObj = json_decode_schema_push(interp, state);
		Jim_Obj *newList = json_decode_dump_container(interp, state);
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
	static const char * const options[] = { "-index", "-null", "-schema", NULL };
	enum { OPT_INDEX, OPT_NULL, OPT_SCHEMA, };
	int i;

	for (i = 1; i < argc - 1; i++) {
		int option;
		if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK) {
			return JIM_ERR;
		}
		switch (option) {
			case OPT_INDEX:
				state->enable_index = 1;
				break;

			case OPT_NULL:
				i++;
				Jim_IncrRefCount(argv[i]);
				Jim_DecrRefCount(interp, state->nullObj);
				state->nullObj = argv[i];
				break;

			case OPT_SCHEMA:
				state->enable_schema = 1;
				break;
		}
	}

	if (i != argc - 1) {
		return JIM_USAGE;
	}

	return JIM_OK;
}

/**
 * Use jsmn to tokenise the JSON string 'json' of length 'len'
 *
 * Returns an allocated array of tokens or NULL on error (and sets an error result)
 */
static jsmntok_t *
json_decode_tokenize(Jim_Interp *interp, const char *json, size_t len)
{
	jsmntok_t	*t;
	jsmn_parser	 parser;
	int n;

	/* Parse once just to find the number of tokens */
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
	Jim_Obj *list;
	jsmntok_t *tokens;
	int len;
	int ret = JIM_ERR;
	struct json_state state;

	memset(&state, 0, sizeof(state));

	state.nullObj = Jim_NewStringObj(interp, "null", -1);
	Jim_IncrRefCount(state.nullObj);

	if ((ret = parse_json_decode_options(interp, argc, argv, &state)) != JIM_OK) {
		goto done;
	}

	state.json = Jim_GetString(argv[argc - 1], &len);

	if (!len) {
		Jim_SetResultString(interp, "empty JSON string", -1);
		goto done;
	}

	/* Save any source information from the original string */
	state.fileNameObj = Jim_GetSourceInfo(interp, argv[argc - 1], &state.line);

	if ((tokens = json_decode_tokenize(interp, state.json, len)) == NULL) {
		goto done;
	}
	state.tok = tokens;
	json_decode_schema_push(interp, &state);

	list = json_decode_dump_container(interp, &state);
	Jim_Free(tokens);
	ret = JIM_OK;

	/* Make sure the refcount doesn't go to 0 during Jim_SubstObj() */
	Jim_IncrRefCount(list);

	if (state.need_subst) {
		/* Subsitute backslashes in the returned dictionary.
		 * Need to be careful of refcounts.
		 * Note that Jim_SubstObj() supports a few more escapes than
		 * JSON requires, but should give the same result for all legal escapes.
		 */
		Jim_Obj *newList;
		Jim_SubstObj(interp, list, &newList, JIM_SUBST_FLAG | JIM_SUBST_NOCMD | JIM_SUBST_NOVAR);
		Jim_IncrRefCount(newList);
		Jim_DecrRefCount(interp, list);
		list = newList;
	}

	if (state.schemaObj) {
		Jim_Obj *resultObj = Jim_NewListObj(interp, NULL, 0);
		Jim_ListAppendElement(interp, resultObj, list);
		Jim_ListAppendElement(interp, resultObj, state.schemaObj);
		Jim_SetResult(interp, resultObj);
		Jim_DecrRefCount(interp, state.schemaObj);
	}
	else {
		Jim_SetResult(interp, list);
	}
	Jim_DecrRefCount(interp, list);

done:
	Jim_DecrRefCount(interp, state.nullObj);

	return ret;
}

int
Jim_jsonInit(Jim_Interp *interp)
{
	Jim_PackageProvideCheck(interp, "json");
	Jim_RegisterSimpleCmd(interp, "json::decode", "?-index? ?-null nullvalue? ?-schema? json", 1, 5, json_decode);
	/* Load the Tcl implementation of the json encoder if possible */
	Jim_PackageRequire(interp, "jsonencode", 0);
	return JIM_OK;
}
