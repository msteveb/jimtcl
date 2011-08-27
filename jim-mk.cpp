#include <string.h>
#include <new>
#include <mk4.h>

#include "jim.h"
#include "jimautoconf.h"
#include "jim-subcmd.h"

extern "C" { /* The whole file is essentially C */

#define MK_PROPERTY_BINARY  'B'
#define MK_PROPERTY_INT     'I'
#define MK_PROPERTY_LONG    'L'
#define MK_PROPERTY_FLOAT   'F'
#define MK_PROPERTY_DOUBLE  'D'
#define MK_PROPERTY_STRING  'S'
#define MK_PROPERTY_VIEW    'V'

#define MK_MODE_ORIGINAL    -1
#define MK_MODE_READONLY    0
#define MK_MODE_READWRITE   1
#define MK_MODE_EXTEND      2

#define MK_CMD_LEN 32
#define JIM_CURSOR_SPACE (35+JIM_REFERENCE_TAGLEN + 1 + 20)

#define isnamech(c) ( !strchr(":,[^]!-", (c)) && (c) != 0 )

#define JIM_MKFLAG_INMEMORY 0x0001
#define JIM_MKFLAG_READONLY 0x0002
#define JIM_MKFLAG_EXTEND   0x0004
#define JIM_MKFLAG_NOCOMMIT 0x0008

#define JIM_CMDFLAG_NODESTROY 0x0100 /* Do not destroy a one-shot view after this command */

/* utilities */
static int JimCheckMkName(Jim_Interp *interp, Jim_Obj *name, const char *type);
static const char *MkPropertyTypeName(c4_Property p);
static Jim_Obj *JimGetMkValue(Jim_Interp *interp, c4_Cursor cur, const c4_Property &prop);
static int JimSetMkValue(Jim_Interp *interp, c4_Cursor cur, const c4_Property &prop, Jim_Obj *obj);

/* property object */
static Jim_Obj *JimNewPropertyObj (Jim_Interp *interp, c4_Property prop);
static int JimGetProperty (Jim_Interp *interp, Jim_Obj *obj,
    c4_View view, const char *what, const c4_Property **propPtr);
static int JimGetPropertyTyped (Jim_Interp *interp, Jim_Obj *obj,
    char type, const c4_Property **propPtr);
static int JimGetNewProperty (Jim_Interp *interp, Jim_Obj *obj,
    c4_View view, char type, const c4_Property **propPtr);
static int JimGetProperties (Jim_Interp *interp, int objc, Jim_Obj *const *objv,
    c4_View view, c4_View *propsPtr);
static Jim_Obj *JimViewPropertiesList (Jim_Interp *interp, c4_View view);

/* cursor object */
static int JimGetCursor (Jim_Interp *interp, Jim_Obj *obj,
    const c4_Cursor **curPtr, Jim_Obj **viewObjPtr);
static int JimIncrCursor (Jim_Interp *interp, Jim_Obj *obj, int offset);
static int JimSeekCursor (Jim_Interp *interp, Jim_Obj *obj, int position);

/* Also accepts JIM_ERRMSG */
#define JIM_CURSOR_GET      (1 << JIM_PRIV_FLAG_SHIFT)
#define JIM_CURSOR_SET      (2 << JIM_PRIV_FLAG_SHIFT)
#define JIM_CURSOR_INSERT   (4 << JIM_PRIV_FLAG_SHIFT)

static int JimCheckCursor (Jim_Interp *interp, Jim_Obj *curObj, int flags);

/* view object */
static Jim_Obj *JimNewViewObj (Jim_Interp *interp, c4_View view);
static int JimGetView (Jim_Interp *interp, Jim_Obj *obj, c4_View *viewPtr);
static void JimPinView (Jim_Interp *interp, Jim_Obj *obj);

/* -------------------------------------------------------------------------
 * Utilities
 * ------------------------------------------------------------------------- */

static int JimCheckMkName(Jim_Interp *interp, Jim_Obj *name, const char *type)
{
    const char *s;
    int i, len;

    s = Jim_GetString(name, &len);
    for (i = 0; i < len; i++) {
        if (!isnamech(s[i])) {
            Jim_SetResultFormatted(interp, "expected %s name but got \"%#s\"", type ? type : "property", name);
            return JIM_ERR;
        }
    }

    return JIM_OK;
}

static const char *const jim_mktype_options[] = {
    "-integer",
    "-long",
    "-float",
    "-double",
    "-string",
    "-subview",
    /* FIXME "-binary", */
    0
};

static const char jim_mktype_types[] = {
    MK_PROPERTY_INT,
    MK_PROPERTY_LONG,
    MK_PROPERTY_FLOAT,
    MK_PROPERTY_DOUBLE,
    MK_PROPERTY_STRING,
    MK_PROPERTY_VIEW,
    /* MK_PROPERTY_BINARY, */
};

#define JIM_MKTYPES ((int)(sizeof(jim_mktype_types) / sizeof(jim_mktype_types[0])))

static const char *MkPropertyTypeName(c4_Property p)
{
    int i;

    for (i = 0; i < JIM_MKTYPES; i++) {
        if (p.Type() == jim_mktype_types[i])
            /* Name without the dash */
            return jim_mktype_options[i] + 1;
    }
    return "(unknown type)";
}

static Jim_Obj *JimGetMkValue(Jim_Interp *interp, c4_Cursor cur, const c4_Property &prop)
{
    switch (prop.Type()) {
        case MK_PROPERTY_INT:
            return Jim_NewIntObj(interp, ((c4_IntProp &)prop).Get(*cur));
        case MK_PROPERTY_LONG:
            return Jim_NewIntObj(interp, ((c4_LongProp &)prop).Get(*cur));
        case MK_PROPERTY_FLOAT:
            return Jim_NewDoubleObj(interp, ((c4_FloatProp &)prop).Get(*cur));
        case MK_PROPERTY_DOUBLE:
            return Jim_NewDoubleObj(interp, ((c4_DoubleProp &)prop).Get(*cur));
        case MK_PROPERTY_STRING:
            return Jim_NewStringObj(interp, ((c4_StringProp &)prop).Get(*cur), -1);
        case MK_PROPERTY_VIEW:
            return JimNewViewObj(interp, ((c4_ViewProp &)prop).Get(*cur));

        case MK_PROPERTY_BINARY:
            /* FIXME */
        default:
            /* FIXME Something more meaningful here? */
            return Jim_NewEmptyStringObj(interp);
    }
}

static int JimSetMkValue(Jim_Interp *interp, c4_Cursor cur, const c4_Property &prop, Jim_Obj *obj)
{
    switch (prop.Type()) {
        case MK_PROPERTY_INT: {
            jim_wide value;

            if (Jim_GetWide(interp, obj, &value) != JIM_OK)
                return JIM_ERR;

            ((c4_IntProp &)prop).Set(*cur, value);
            return JIM_OK;
        }
        case MK_PROPERTY_LONG: {
            jim_wide value;

            if (Jim_GetWide(interp, obj, &value) != JIM_OK)
                return JIM_ERR;

            ((c4_LongProp &)prop).Set(*cur, value);
            return JIM_OK;
        }
        case MK_PROPERTY_FLOAT: {
            double value;

            if (Jim_GetDouble(interp, obj, &value) != JIM_OK)
                return JIM_ERR;

            ((c4_FloatProp &)prop).Set(*cur, value);
            return JIM_OK;
        }
        case MK_PROPERTY_DOUBLE: {
            double value;

            if (Jim_GetDouble(interp, obj, &value) != JIM_OK)
                return JIM_ERR;

            ((c4_DoubleProp &)prop).Set(*cur, value);
            return JIM_OK;
        }
        case MK_PROPERTY_STRING: {
            int len;
            const char *rep;

            rep = Jim_GetString(obj, &len);
            if (len != (int)strlen(rep)) {
                Jim_SetResultString(interp, "null characters are not allowed in Metakit strings", -1);
                return JIM_ERR;
            }

            ((c4_StringProp &)prop).Set(*cur, rep);
            return JIM_OK;
        }
        case MK_PROPERTY_VIEW: {
            c4_View value;

            if (JimGetView(interp, obj, &value) != JIM_OK)
                return JIM_ERR;

            ((c4_ViewProp &)prop).Set(*cur, value);
        }
        case MK_PROPERTY_BINARY:
            /* FIXME */
        default:
            Jim_SetResultString(interp, "unsupported Metakit type", -1);
            return JIM_ERR;
    }
}

/* -------------------------------------------------------------------------
 * Property object
 * ------------------------------------------------------------------------- */

#define JimPropertyValue(o) ((c4_Property *)((o)->internalRep.ptr))

static void FreePropertyInternalRep(Jim_Interp *interp, Jim_Obj *obj)
{
    delete JimPropertyValue(obj);
}

static void DupPropertyInternalRep(Jim_Interp *interp, Jim_Obj *oldObj, Jim_Obj *newObj)
{
    newObj->internalRep.ptr = new c4_Property(*JimPropertyValue(oldObj));
    newObj->typePtr = oldObj->typePtr;
}

static void UpdateStringOfProperty(Jim_Obj* obj)
{
    const char *name = JimPropertyValue(obj)->Name();
    int len = strlen(name);

    obj->bytes = (char *) Jim_Alloc(len + 1);
    memcpy(obj->bytes, name, len + 1);
    obj->length = len;
}

static Jim_ObjType propertyObjType = {
    "mk.property",
    FreePropertyInternalRep,
    DupPropertyInternalRep,
    UpdateStringOfProperty,
    JIM_TYPE_NONE
};

static int JimGetProperty(Jim_Interp *interp, Jim_Obj *obj, c4_View view, const char *name, const c4_Property **propPtr)
{
    int index;

    if (obj->typePtr == &propertyObjType) {
        index = view.FindProperty(JimPropertyValue(obj)->GetId());
    } else {
        if (JimCheckMkName(interp, obj, name) != JIM_OK)
            return JIM_ERR;
        index = view.FindPropIndexByName(Jim_String(obj));
    }

    if (index != -1) {
        *propPtr = &view.NthProperty(index);
        return JIM_OK;
    }
    else {
        Jim_SetResultFormatted(interp, "%s \"%#s\" does not exist",
            name ? name : "property", obj);
        return JIM_ERR;
    }
}

static int JimGetPropertyTyped(Jim_Interp *interp, Jim_Obj *obj, char type, const c4_Property **propPtr)
{
    c4_Property *prop;

    if (obj->typePtr == &propertyObjType) {
        if (JimPropertyValue(obj)->Type() != type) {
            /* coerce the property type */

            prop = new c4_Property(type, JimPropertyValue(obj)->Name());
            delete JimPropertyValue(obj);
            obj->internalRep.ptr = prop;
        }
    }
    else {
        if (JimCheckMkName(interp, obj, NULL) != JIM_OK)
            return JIM_ERR;

        prop = new c4_Property(type, Jim_String(obj));

        Jim_FreeIntRep(interp, obj);
        obj->typePtr = &propertyObjType;
        obj->internalRep.ptr = (void *)prop;
    }

    *propPtr = JimPropertyValue(obj);
    return JIM_OK;
}

static int JimGetNewProperty(Jim_Interp *interp, Jim_Obj *obj, c4_View view, char type, const c4_Property **propPtr)
{
    const c4_Property *newp, *prop;

    if (JimGetPropertyTyped(interp, obj, type, &newp) != JIM_OK)
        return JIM_ERR;

    prop = &view.NthProperty(view.AddProperty(*newp));

    if (prop->Type() != newp->Type()) {
        Jim_SetResultFormatted(interp, "property \"%#s\" is %s, not %s",
            obj, MkPropertyTypeName(*prop), MkPropertyTypeName(*newp));
        return JIM_ERR;
    }

    *propPtr = prop;
    return JIM_OK;
}

static int JimGetProperties(Jim_Interp *interp, int objc, Jim_Obj *const *objv, c4_View view, c4_View *propsPtr)
{
    int i;
    const c4_Property *prop;
    c4_View props;

    for (i = 0; i < objc; i++) {
        if (JimGetProperty(interp, objv[i], view, NULL, &prop) != JIM_OK) {
            return JIM_ERR;
        }
        props.AddProperty(*prop);
    }

    *propsPtr = props;
    return JIM_OK;
}

static Jim_Obj *JimNewPropertyObj(Jim_Interp *interp, c4_Property prop)
{
    Jim_Obj *obj;

    obj = Jim_NewObj(interp);
    obj->typePtr = &propertyObjType;
    obj->bytes = NULL;
    obj->internalRep.ptr = new c4_Property(prop);
    return obj;
}

/* -------------------------------------------------------------------------
 * Cursor object
 * ------------------------------------------------------------------------- */

#define JimCursorValue(o) ((c4_Cursor *)((o)->internalRep.twoPtrValue.ptr1))
#define JimCursorView(o)  ((Jim_Obj *)((o)->internalRep.twoPtrValue.ptr2))

static void FreeCursorInternalRep(Jim_Interp *interp, Jim_Obj *obj)
{
    delete JimCursorValue(obj);
    Jim_DecrRefCount(interp, JimCursorView(obj));
}

static void DupCursorInternalRep(Jim_Interp *interp, Jim_Obj *oldObj, Jim_Obj *newObj)
{
    newObj->internalRep.twoPtrValue.ptr1 = new c4_Cursor(*JimCursorValue(oldObj));
    newObj->internalRep.twoPtrValue.ptr2 = JimCursorView(oldObj);
    Jim_IncrRefCount(JimCursorView(oldObj));

    newObj->typePtr = oldObj->typePtr;
}

static void UpdateStringOfCursor(Jim_Obj *obj)
{
    char buf[JIM_CURSOR_SPACE + 1];
    int len;

    snprintf(buf, JIM_CURSOR_SPACE + 1, "%s!%d",
        Jim_String(JimCursorView(obj)), JimCursorValue(obj)->_index);

    len = strlen(buf);
    obj->bytes = (char *) Jim_Alloc(len + 1);
    memcpy(obj->bytes, buf, len + 1);
    obj->length = len;
}

static Jim_ObjType cursorObjType = {
    "mk.cursor",
    FreeCursorInternalRep,
    DupCursorInternalRep,
    UpdateStringOfCursor,
    JIM_TYPE_REFERENCES
};

/* Functions --------------------------------------------------------------- */

static int JimGetCursor(Jim_Interp *interp, Jim_Obj *obj, const c4_Cursor **curPtr, Jim_Obj **viewObjPtr)
{
    if (obj->typePtr != &cursorObjType) {
        const char *rep, *delim;
        int len, index;
        Jim_Obj *viewObj, *indexObj;
        c4_View view;

        rep = Jim_GetString(obj, &len);
        delim = (char *) memchr(rep, '!', len);

        if (!delim) {
            Jim_SetResultFormatted(interp, "expected cursor but got \"%#s\"", obj);
            return JIM_ERR;
        }

        viewObj = Jim_NewStringObj(interp, rep, delim - rep);
        indexObj = Jim_NewStringObj(interp, delim + 1, len - (delim - rep) - 1);

        if (Jim_GetIndex(interp, indexObj, &index) != JIM_OK ||
            JimGetView(interp, viewObj, &view) != JIM_OK)
            /* prefer this order - the view is constructed last */
        {
            Jim_FreeNewObj(interp, viewObj);
            Jim_FreeNewObj(interp, indexObj);
            return JIM_ERR;
        }

        Jim_FreeIntRep(interp, obj);
        Jim_FreeNewObj(interp, indexObj);
        Jim_IncrRefCount(viewObj);

        /* Jim_GetIndex sometimes returns int limit values, but these are too
         * dangerous to manipulate. Unfortunately, we also can't handle end+N
         * values for N >=2 correctly.
         */

        if (index == INT_MAX)
            index = view.GetSize();
        else if (index <= -INT_MAX) /* yes, -INT_MAX and not INT_MIN */
            index = -1;
        else if (index < 0)
            index = view.GetSize() + index;

        obj->typePtr = &cursorObjType;
        obj->internalRep.twoPtrValue.ptr1 = new c4_Cursor(&view[index]);
        obj->internalRep.twoPtrValue.ptr2 = viewObj;
    }

    if (curPtr)
        *curPtr = JimCursorValue(obj);
    if (viewObjPtr)
        *viewObjPtr = JimCursorView(obj);

    return JIM_OK;
}

static int JimIncrCursor(Jim_Interp *interp, Jim_Obj *obj, int offset)
{
    const c4_Cursor *cur;

    /* JimPanic((Jim_IsShared(obj), "JimIncrCursor called with shared object")) */

    if (JimGetCursor(interp, obj, &cur, NULL) != JIM_OK)
        return JIM_ERR;

    Jim_InvalidateStringRep(obj);
    *((c4_Cursor *)cur) += offset;
    return JIM_OK;
}

static int JimSeekCursor(Jim_Interp *interp, Jim_Obj *obj, int index)
{
    const c4_Cursor *cur;

    /* JimPanic((Jim_IsShared(obj), "JimSeekCursor called with shared object")) */

    if (JimGetCursor(interp, obj, &cur, NULL) != JIM_OK)
        return JIM_ERR;
    Jim_InvalidateStringRep(obj);

    if (index <= -INT_MAX)
        index = -1;
    else if (index == INT_MAX)
        index = (**cur).Container().GetSize();
    else if (index < 0)
        index = (**cur).Container().GetSize() + index;

    ((c4_Cursor *)cur)->_index = index;
    return JIM_OK;
}

static int JimCheckCursor(Jim_Interp *interp, Jim_Obj *curObj, int flags)
{
    const c4_Cursor *cur;
    int size;

    if (JimGetCursor(interp, curObj, &cur, NULL) != JIM_OK)
        return JIM_ERR;
    size = (**cur).Container().GetSize();

    if ((flags & JIM_CURSOR_GET) && (cur->_index < 0 || cur->_index >= size)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp,
                "cursor \"%#s\" does not point to an existing row", curObj);
        }
        return JIM_ERR;
    }
    else if ((flags & JIM_CURSOR_SET) && cur->_index < 0) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp,
                "cursor \"%#s\" points before start of view", curObj);
        }
        return JIM_ERR;
    }
    else if ((flags & JIM_CURSOR_INSERT) && (cur->_index < 0 || cur->_index > size)) {
        if (flags & JIM_ERRMSG) {
            Jim_SetResultFormatted(interp,
                "cursor \"%#s\" does not point to a valid insert position", curObj);
        }
        return JIM_ERR;
    }

    return JIM_OK;
}

/* Records ----------------------------------------------------------------- */

static int cursor_cmd_get(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_Cursor *curPtr;
    c4_View view;

    if (JimGetCursor(interp, argv[0], &curPtr, NULL) != JIM_OK)
        return JIM_ERR;
    if (JimCheckCursor(interp, argv[0], JIM_ERRMSG | JIM_CURSOR_GET) != JIM_OK)
        return JIM_ERR;

    view = (**curPtr).Container();

    if (argc == 1) { /* Return all properties */
        int i, count;
        Jim_Obj *result;

        result = Jim_NewListObj(interp, NULL, 0);
        count = view.NumProperties();

        for (i = 0; i < count; i++) {
            c4_Property prop = view.NthProperty(i);

            Jim_ListAppendElement(interp, result,
                JimNewPropertyObj(interp, prop));
            Jim_ListAppendElement(interp, result,
                JimGetMkValue(interp, *curPtr, prop));
        }

        Jim_SetResult(interp, result);
    }
    else { /* Return a single property */
        const c4_Property *propPtr;

        if (argc == 2) {
            /* No type annotation, existing property */
            if (JimGetProperty(interp, argv[1], view, NULL, &propPtr) != JIM_OK)
                return JIM_ERR;
        } else {
            /* Explicit type annotation; the property may be new */
            int idx;

            if (Jim_GetEnum(interp, argv[1], jim_mktype_options, &idx,
                    "property type", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
                return JIM_ERR;
            if (JimGetNewProperty(interp, argv[2], view, jim_mktype_types[idx], &propPtr) != JIM_OK)
                return JIM_ERR;
        }

        Jim_SetResult(interp, JimGetMkValue(interp, *curPtr, *propPtr));
    }

    return JIM_OK;
}

static int cursor_cmd_set(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_Cursor *curPtr;
    const c4_Property *propPtr;
    c4_View view;
    int i;

    if (JimGetCursor(interp, argv[0], &curPtr, NULL) != JIM_OK)
        return JIM_ERR;
    if (JimCheckCursor(interp, argv[0], JIM_ERRMSG | JIM_CURSOR_SET) != JIM_OK)
        return JIM_ERR;

    view = (**curPtr).Container();

    if (curPtr->_index >= view.GetSize()) {
        view.SetSize(curPtr->_index + 1);
    }

    if (argc == 2) {
        /* Update everything except subviews from a dictionary in argv[1].
         * No new properties are permitted.
         */

        int objc;
        Jim_Obj **objv;

        if (Jim_DictPairs(interp, argv[1], &objv, &objc) != JIM_OK)
            return JIM_ERR;

        for (i = 0; i < objc; i += 2) {
            if (JimGetProperty(interp, objv[i], view, NULL, &propPtr) != JIM_OK ||
                JimSetMkValue(interp, *curPtr, *propPtr, objv[i+1]) != JIM_OK)
            {
                Jim_Free(objv);
                return JIM_ERR;
            }
        }
    } else {
        /* Update everything from argv[1..]. New properties are permitted if
         * explicitly typed.
         */

        for (i = 1; i < argc; i += 2) {
            if (Jim_String(argv[i])[0] == '-') {
                int idx;

                if (i + 2 >= argc) {
                    Jim_WrongNumArgs(interp, 2, argv, "?-type? prop value ?...?");
                    return JIM_ERR;
                }

                if (Jim_GetEnum(interp, argv[i], jim_mktype_options, &idx,
                        "property type", JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
                    return JIM_ERR;
                if (JimGetNewProperty(interp, argv[i+1], view, jim_mktype_types[idx], &propPtr) != JIM_OK)
                    return JIM_ERR;
                i++;
            }
            else {
                if (i + 1 >= argc) {
                    Jim_WrongNumArgs(interp, 2, argv, "?-type? prop value ?...?");
                    return JIM_ERR;
                }

                if (JimGetProperty(interp, argv[i], view, NULL, &propPtr) != JIM_OK)
                    return JIM_ERR;
            }

            if (JimSetMkValue(interp, *curPtr, *propPtr, argv[i+1]) != JIM_OK)
                return JIM_ERR;
        }
    }

    return JIM_OK;
}

static int cursor_cmd_insert(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    c4_View view;
    const c4_Cursor *curPtr;
    jim_wide count;

    if (JimGetCursor(interp, argv[0], &curPtr, NULL) != JIM_OK)
        return JIM_ERR;
    if (JimCheckCursor(interp, argv[0], JIM_ERRMSG | JIM_CURSOR_INSERT) != JIM_OK)
        return JIM_ERR;

    view = (**curPtr).Container();

    if (argc == 1) {
        count = 1;
    } else {
        if (Jim_GetWide(interp, argv[1], &count) != JIM_OK)
            return JIM_ERR;
    }

    if (count > 0) {
        c4_Row empty;
        view.InsertAt(curPtr->_index, empty, (int)count);
    }

    Jim_SetEmptyResult(interp);
    return JIM_OK;
}

static int cursor_cmd_remove(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    c4_View view;
    const c4_Cursor *curPtr;
    int pos;
    jim_wide count;

    if (JimGetCursor(interp, argv[0], &curPtr, NULL) != JIM_OK)
        return JIM_ERR;
    if (JimCheckCursor(interp, argv[0], JIM_ERRMSG | JIM_CURSOR_SET) != JIM_OK)
        return JIM_ERR;

    view = (**curPtr).Container();
    pos = curPtr->_index;

    if (argc == 1) {
        count = 1;
    } else {
        if (Jim_GetWide(interp, argv[1], &count) != JIM_OK)
            return JIM_ERR;
    }

    if (pos + count < view.GetSize())
        count = view.GetSize() - pos;

    if (pos < view.GetSize())
        view.RemoveAt(pos, (int)count);

    return JIM_OK;
}

/* Attributes -------------------------------------------------------------- */

static int cursor_cmd_view(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *viewObj;

    if (JimGetCursor(interp, argv[0], NULL, &viewObj) != JIM_OK)
        return JIM_ERR;

    JimPinView(interp, viewObj);
    Jim_SetResult(interp, viewObj);
    return JIM_OK;
}

/* Positioning ------------------------------------------------------------- */

static int cursor_cmd_tell(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_Cursor *curPtr;

    if (JimGetCursor(interp, argv[0], &curPtr, NULL) != JIM_OK)
        return JIM_ERR;
    Jim_SetResultInt(interp, curPtr->_index);
    return JIM_OK;
}

static int cursor_cmd_validfor(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char *options[] = {
        "get", "set", "insert", "remove", 0
    };
    static int optflags[] = {
        JIM_CURSOR_GET,
        JIM_CURSOR_SET,
        JIM_CURSOR_INSERT,
        JIM_CURSOR_SET
    };

    int idx;

    if (argc == 1) {
        idx = 0;
    } else {
        if (Jim_GetEnum(interp, argv[0], options, &idx, NULL,
                JIM_ERRMSG | JIM_ENUM_ABBREV) != JIM_OK)
            return JIM_ERR;
    }

    if (JimGetCursor(interp, argv[argc-1], NULL, NULL) != JIM_OK)
        return JIM_ERR;

    Jim_SetResultBool(interp, JimCheckCursor(interp, argv[argc-1], optflags[idx]) == JIM_OK);
    return JIM_OK;
}

static int cursor_cmd_seek(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *curObj;
    int index;

    if (Jim_GetIndex(interp, argv[1], &index) != JIM_OK)
        return JIM_ERR;

    curObj = Jim_GetVariable(interp, argv[0], JIM_ERRMSG | JIM_UNSHARED);
    if (curObj == NULL)
        return JIM_ERR;

    if (JimSeekCursor(interp, curObj, index) != JIM_OK)
        return JIM_ERR;

    Jim_SetResult(interp, curObj);
    return JIM_OK;
}

static int cursor_cmd_incr(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_Obj *curObj;
    jim_wide offset;

    if (argc == 1) {
        offset = 1;
    } else {
        if (Jim_GetWide(interp, argv[1], &offset) != JIM_OK)
            return JIM_ERR;
    }

    curObj = Jim_GetVariable(interp, argv[0], JIM_ERRMSG | JIM_UNSHARED);
    if (curObj == NULL)
        return JIM_ERR;

    if (JimIncrCursor(interp, curObj, (int)offset) != JIM_OK)
        return JIM_ERR;

    Jim_SetResult(interp, curObj);
    return JIM_OK;
}

/* Command table ----------------------------------------------------------- */

static const jim_subcmd_type cursor_command_table[] = {

    /* Records */

    {   "get", "cur ?-type? ?prop?",
        cursor_cmd_get,
        1, 3,
        0,
        "Get the whole record or a specific property at the cursor"
    },
    {   "set", "cur [dict | ?-type? field value ?...?]",
        cursor_cmd_set,
        1, -1,
        0,
        "Update the record at the cursor"
    },
    {   "insert", "cur ?count?",
        cursor_cmd_insert,
        1, 2,
        0,
        "Insert a specified number of empty rows at the cursor (default 1)"
    },
    {   "remove", "cur ?count?",
        cursor_cmd_remove,
        1, 2,
        0,
        "Remove a specified number of rows at the cursor (default 1)"
    },

    /* Attributes */

    {   "view", "cur",
        cursor_cmd_view,
        1, 1,
        0,
        "Get the view the cursor points into"
    },

    /* Positioning */

    {   "tell", "cur",
        cursor_cmd_tell,
        1, 1,
        0,
        "Get the position of the cursor"
    },
    {   "validfor", "?command? cur",
        cursor_cmd_validfor,
        1, 2,
        0,
        "Checks if the cursor is valid for get (default), set or insert commands"
    },
    {   "seek", "curVar index",
        cursor_cmd_seek,
        2, 2,
        0,
        "Seek to the specified index in the view"
    },
    {   "incr", "curVar ?offset?",
        cursor_cmd_incr,
        1, 2,
        0,
        "Move the cursor offset records from its current position (default 1)"
    },

    { 0 }
};

/* -------------------------------------------------------------------------
 * View object
 * ------------------------------------------------------------------------- */

/* Views aren't really Jim objects; instead, they are Tk-style commands with
 * oo.tcl-like lifetime management. Additionally, all views are initially
 * created as one-shot, meaning that they die after one command. Call
 * JimPinView to make a view object persistent.
 *
 * It is valid to rename a view in the Tcl land, but by doing this you take
 * the responsibility of destroying the object when it's no longer needed.
 * Any cursors that pointed into the view become invalid.
 */

static int JimViewSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);
static int JimOneShotViewSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv);

/* Relational view operations ---------------------------------------------- */

static int view_cmd_join(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_View *viewPtr = (const c4_View *) Jim_CmdPrivData(interp);
    c4_View other, props;
    int outer, off;

    if (JimGetView(interp, argv[0], &other) != JIM_OK)
        return JIM_ERR;

    off = 1; outer = 0;
    if (Jim_CompareStringImmediate(interp, argv[1], "-outer")) {
        off++; outer = 1;
    }

    if (JimGetProperties(interp, argc - off, argv + off, *viewPtr, &props) != JIM_OK)
        return JIM_ERR;

    Jim_SetResult(interp, JimNewViewObj(interp, viewPtr->Join(other, props, outer)));
    return JIM_OK;
}

static int view_cmd_group(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_View *viewPtr = (const c4_View *) Jim_CmdPrivData(interp);
    const c4_Property *subviewPtr;
    c4_View props;

    if (JimGetPropertyTyped(interp, argv[0], MK_PROPERTY_VIEW, &subviewPtr) != JIM_OK)
        return JIM_ERR;

    if (JimGetProperties(interp, argc - 1, argv + 1, *viewPtr, &props) != JIM_OK)
        return JIM_ERR;

    Jim_SetResult(interp, JimNewViewObj(interp, viewPtr->GroupBy(props, *(c4_ViewProp *)subviewPtr)));
    return JIM_OK;
}

static int view_cmd_flatten(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_View *viewPtr = (const c4_View *) Jim_CmdPrivData(interp);
    const c4_Property *subviewPtr;

    if (JimGetProperty(interp, argv[0], *viewPtr, NULL, &subviewPtr) != JIM_OK)
        return JIM_ERR;

    if (subviewPtr->Type() != MK_PROPERTY_VIEW) {
        Jim_SetResultFormatted(interp, "expected a subview property but got %s one",
            MkPropertyTypeName(*subviewPtr));
        return JIM_ERR;
    }

    Jim_SetResult(interp, JimNewViewObj(interp, viewPtr->JoinProp(*(c4_ViewProp *)subviewPtr)));
    return JIM_OK;
}

/* View lifetime ----------------------------------------------------------- */

static int view_cmd_return(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimPinView(interp, argv[0]);
    Jim_SetResult(interp, argv[0]);
    return JIM_OK;
}

static int view_cmd_as(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    JimPinView(interp, argv[0]);
    Jim_SetVariable(interp, argv[2], argv[0]);
    Jim_SetResult(interp, argv[0]);
    return JIM_OK;
}

static int view_cmd_destroy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_DeleteCommand(interp, Jim_String(argv[0]));
    return JIM_OK;
}

/* View queries */

static int view_cmd_properties(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_View *viewPtr = (const c4_View *) Jim_CmdPrivData(interp);
    Jim_SetResult(interp, JimViewPropertiesList(interp, *viewPtr));
    return JIM_OK;
}

static int view_cmd_size(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const c4_View *viewPtr = (const c4_View *) Jim_CmdPrivData(interp);
    Jim_SetResultInt(interp, viewPtr->GetSize());
    return JIM_OK;
}

static int view_cmd_resize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    c4_View *view = (c4_View *) Jim_CmdPrivData(interp);
    jim_wide size;

    if (Jim_GetWide(interp, argv[0], &size) != JIM_OK)
        return JIM_ERR;
    if (size < 0 || size > INT_MAX) {
        Jim_SetResultFormatted(interp,
            "view size \"%#s\" is out of range", argv[0]);
        return JIM_ERR;
    }

    view->SetSize((int)size);
    Jim_SetResult(interp, argv[0]);
    return JIM_OK;
}

/* Command table ----------------------------------------------------------- */

static const jim_subcmd_type view_command_table[] = {

    /* Relational operations */

    {   "join", "view ?-outer? prop ?prop ...?",
        view_cmd_join,
        2, -1,
        0,
        "Relational join with view on the specified properties"
    },
    {   "group", "subviewName prop ?prop ...?",
        view_cmd_group,
        1, -1,
        0,
        "Group rows with equal specified properties, move all other properties into subview"
    },
    {   "flatten", "subviewName",
        view_cmd_flatten,
        1, 1,
        0,
        "Flatten the specified subview; the inverse of group"
    },

    /* Attributes */

    {   "properties", "",
        view_cmd_properties,
        0, 0,
        0,
        "List the properties in this view"
    },
    {   "size", "",
        view_cmd_size,
        0, 0,
        0,
        "Return the number of records in the view"
    },
    {   "resize", "newSize",
        view_cmd_resize,
        1, 1,
        0,
        "Set the number of records in the view"
    },

    /* Lifetime management */

    {   "return", "",
        view_cmd_return,
        0, 0,
        JIM_MODFLAG_FULLARGV | JIM_CMDFLAG_NODESTROY,
        "Marks the view as persistent"
    },
    {   "as", "varName",
        view_cmd_as,
        1, 1,
        JIM_MODFLAG_FULLARGV | JIM_CMDFLAG_NODESTROY,
        "Marks the view as persistent and assigns it to the given variable"
    },
    {   "destroy", "",
        view_cmd_destroy,
        0, 0,
        JIM_MODFLAG_FULLARGV | JIM_CMDFLAG_NODESTROY,
        "Destroys the view explicitly"
    },

    { 0 }
};

static void JimViewDelProc(Jim_Interp *interp, void *privData)
{
    delete (c4_View *)privData;
}

static int JimViewSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, view_command_table, argc, argv), argc, argv);
}

static int JimOneShotViewSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const jim_subcmd_type *cmd = Jim_ParseSubCmd(interp, view_command_table, argc, argv);
    int result = Jim_CallSubCmd(interp, cmd, argc, argv);

    if (!cmd || !(cmd->flags & JIM_CMDFLAG_NODESTROY))
        Jim_DeleteCommand(interp, Jim_String(argv[0]));
    return result;
}

static int JimViewFinalizerProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* We won't succeed here if the user renamed the command, and this is right */
    Jim_DeleteCommand(interp, Jim_String(argv[1]));
    return JIM_OK;
}

static Jim_Obj *JimNewViewObj(Jim_Interp *interp, c4_View view) {
    Jim_Obj *tag, *ref;

    tag = Jim_NewStringObj(interp, "mk.view", -1);
    ref = Jim_NewReference(interp, tag, tag, Jim_NewStringObj(interp, "mk.view.finalizer", -1));
    Jim_CreateCommand(interp, Jim_String(ref),
        JimOneShotViewSubCmdProc, new c4_View(view), JimViewDelProc);

    return ref;
}

static int JimGetView(Jim_Interp *interp, Jim_Obj *obj, c4_View *viewPtr)
{
    Jim_Cmd *cmd = Jim_GetCommand(interp, obj, 0);

    if (cmd == NULL || cmd->isproc || cmd->u.native.delProc != JimViewDelProc) {
        Jim_SetResultFormatted(interp, "invalid view object \"%#s\"", obj);
        return JIM_ERR;
    }

    *viewPtr = *(c4_View *)cmd->u.native.privData;
    return JIM_OK;
}

/* Only call this against known view objects. */
static void JimPinView(Jim_Interp *interp, Jim_Obj *obj)
{
    Jim_Cmd *cmd = Jim_GetCommand(interp, obj, 0);
    /* JimPanic((cmd == NULL, "JimPinView called against non-view"))
       JimPanic((cmd->u.native.delProc != JimViewDelProc, "JimPinView called against non-view")) */
    cmd->u.native.cmdProc = JimViewSubCmdProc;
}

static Jim_Obj *JimViewPropertiesList(Jim_Interp *interp, c4_View view)
{
    int i, count;
    Jim_Obj *result;

    result = Jim_NewListObj(interp, NULL, 0);
    count = view.NumProperties();

    for (i = 0; i < count; i++) {
        Jim_ListAppendElement(interp, result, Jim_NewStringObj(interp,
            view.NthProperty(i).Name(), -1));
    }

    return result;
}

/* ----------------------------------------------------------------------------
 * Storage object
 * ---------------------------------------------------------------------------- */

/* These are also commands, like views, but must be managed explicitly by the
 * user. Quite like file handles, actually.
 */

typedef struct MkStorage {
    unsigned flags;
    Jim_Obj *filename;
    c4_Storage storage;
    c4_Cursor content;
} MkStorage;

/* Attributes -------------------------------------------------------------- */

static int storage_cmd_nocommit(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);

    if (argc == 1) {
        jim_wide flag;

        if (Jim_GetWide(interp, argv[0], &flag) != JIM_OK)
            return JIM_ERR;

        if (flag)
            mk->flags |= JIM_MKFLAG_NOCOMMIT;
        else
            mk->flags &= ~JIM_MKFLAG_NOCOMMIT;
        mk->storage.AutoCommit(!flag);
    }

    Jim_SetResultBool(interp, (mk->flags & JIM_MKFLAG_NOCOMMIT) != 0);
    return JIM_OK;
}

static int storage_cmd_readonly(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);

    Jim_SetResultBool(interp, (mk->flags & JIM_MKFLAG_READONLY) != 0);
    return JIM_OK;
}

/* Views ------------------------------------------------------------------- */

static int storage_cmd_views(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);

    Jim_SetResult(interp, JimViewPropertiesList(interp, mk->storage));
    return JIM_OK;
}

static int storage_cmd_view(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);
    const c4_Property *propPtr;

    if (JimGetProperty(interp, argv[0], mk->storage, "view", &propPtr) != JIM_OK)
        return JIM_ERR;

    Jim_SetResult(interp, JimGetMkValue(interp, mk->content, *propPtr));
    return JIM_OK;
}

/* Store operations -------------------------------------------------------- */

static int storage_cmd_commit(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);

    if (mk->flags & JIM_MKFLAG_INMEMORY) {
        Jim_SetResultString(interp, "cannot commit an in-memory storage", -1);
        return JIM_ERR;
    }
    else if (mk->flags & JIM_MKFLAG_READONLY) {
        Jim_SetResultString(interp, "cannot commit a read-only storage", -1);
        return JIM_ERR;
    }

    if (mk->storage.Commit(0)) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }
    else {
        Jim_SetResultString(interp, "I/O error during commit", -1);
        return JIM_ERR;
    }
}

static int storage_cmd_rollback(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk = (MkStorage *)Jim_CmdPrivData(interp);

    if (mk->flags & JIM_MKFLAG_INMEMORY) {
        Jim_SetResultString(interp, "cannot rollback an in-memory storage", -1);
        return JIM_ERR;
    }

    if (mk->storage.Rollback(0)) {
        Jim_SetEmptyResult(interp);
        return JIM_OK;
    }
    else {
        Jim_SetResultString(interp, "I/O error during rollback", -1);
        return JIM_ERR;
    }
}

static int storage_cmd_close(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_DeleteCommand(interp, Jim_String(argv[0]));
}

/* Command table ----------------------------------------------------------- */

static const jim_subcmd_type storage_command_table[] = {

    /* Options */

    {   "nocommit", "?value?",
        storage_cmd_nocommit,
        0, 1,
        0,
        "Query or modify the auto-commit option of this storage"
    },
    {   "readonly", "",
        storage_cmd_readonly,
        0, 0,
        0,
        "Returns the read-only status of this storage"
    },

    /* Views */

    {   "views", "",
        storage_cmd_views,
        0, 0,
        0,
        "Returns the list of views stored here"
    },
    {   "view", "viewName",
        storage_cmd_view,
        1, 1,
        0,
        "Retrieve the view specified by viewName"
    },

    /* Store operations */

    {   "commit", "",
        storage_cmd_commit,
        0, 0,
        0,
        "Commit the changes to disk"
    },
    {   "rollback", "",
        storage_cmd_rollback,
        0, 0,
        0,
        "Revert to the saved state"
    },
    {   "close", "",
        storage_cmd_close,
        0, 0,
        JIM_MODFLAG_FULLARGV,
        "Close this storage"
    },
    { 0 }
};

static void JimStorageDelProc(Jim_Interp *interp, void *privData)
{
    MkStorage *mk = (MkStorage *)privData;

    mk->storage.~c4_Storage();
    mk->content.~c4_Cursor();
    Jim_DecrRefCount(interp, mk->filename);
    Jim_Free(mk);
}

static int JimStorageSubCmdProc(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp,
        storage_command_table, argc, argv), argc, argv);
}

/* -------------------------------------------------------------------------
 * storage ?options? ?filename?
 *
 * Creates a new metakit storage object, optionally backed by a file.
 *
 * Options apply only when filename is given; these include:
 *
 *   -readonly   Open the file in read-only mode
 *   -original   Open the file in read-only mode, discarding possible extends
 *   -extend     Open the file in extend mode
 *   -nocommit   Do not commit the changes when the storage is closed
 * ------------------------------------------------------------------------- */

static int JimStorageCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    MkStorage *mk;
    char buf[MK_CMD_LEN];
    int i, mode;

    static const char *const options[] = {
        "-readonly",
        "-original",
        "-extend",
        "-nocommit",
        0
    };
    enum {
        OPT_READONLY,
        OPT_ORIGINAL,
        OPT_EXTEND,
        OPT_NOCOMMIT
    };
    int option;

    mk = (MkStorage *)Jim_Alloc(sizeof(MkStorage));
    mk->flags = 0;
    mode = MK_MODE_READWRITE;
    for (i = 1; i < argc - 1; i++ ) {
        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG) != JIM_OK) {
            Jim_Free(mk);
            return JIM_ERR;
        }

        switch (option) {
            case OPT_READONLY:
                if (mode != MK_MODE_READWRITE)
                    goto modeconflict;

                mode = MK_MODE_READONLY;
                mk->flags |= JIM_MKFLAG_READONLY;
                break;

            case OPT_ORIGINAL:
                if (mode != MK_MODE_READWRITE)
                    goto modeconflict;

                mode = MK_MODE_ORIGINAL;
                mk->flags |= JIM_MKFLAG_READONLY;
                break;

            case OPT_EXTEND:
                if (mode != MK_MODE_READWRITE)
                    goto modeconflict;

                mode = MK_MODE_EXTEND;
                mk->flags |= JIM_MKFLAG_EXTEND;
                break;

            case OPT_NOCOMMIT:
                mk->flags |= JIM_MKFLAG_NOCOMMIT;
                break;
        }
    }

    if (argc > 1) {
        new(&mk->storage) c4_Storage(Jim_String(argv[argc-1]), mode);

        if (!mk->storage.Strategy().IsValid()) {
            mk->storage.~c4_Storage();
            Jim_Free(mk);
            Jim_SetResultFormatted(interp, "could not open storage \"%#s\"", argv[argc-1]);
            return JIM_ERR;
        }

        mk->filename = argv[argc-1];

        if (!((mk->flags & JIM_MKFLAG_NOCOMMIT) || (mk->flags & JIM_MKFLAG_READONLY))) {
            mk->storage.AutoCommit(1);
        }
    }
    else {
        mk->flags |= JIM_MKFLAG_INMEMORY;

        new(&mk->storage) c4_Storage();
        mk->filename = Jim_NewEmptyStringObj(interp);
    }
    new(&mk->content) c4_Cursor(&mk->storage[0]);
    Jim_IncrRefCount(mk->filename);

    snprintf(buf, sizeof(buf), "mk.handle%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimStorageSubCmdProc, mk, JimStorageDelProc);
    Jim_SetResultString(interp, buf, -1);
    return JIM_OK;

  modeconflict:
    Jim_Free(mk);
    Jim_SetResultString(interp, "only one of -readonly, -original and -extend may be specified", -1);
    return JIM_ERR;
}

/* -------------------------------------------------------------------------
 * Initialization code
 * ------------------------------------------------------------------------- */

int Jim_mkInit(Jim_Interp *interp)
{
    char version[4];

    version[0] = '0' + d4_MetakitLibraryVersion / 100;
    version[1] = '0' + d4_MetakitLibraryVersion % 100 / 10;
    version[2] = '0' + d4_MetakitLibraryVersion / 10;
    version[3] = '\0';

    if (Jim_PackageProvide(interp, "mk", version, JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "storage", JimStorageCommand, NULL, NULL);
    Jim_CreateCommand(interp, "cursor", Jim_SubCmdProc, (void *)cursor_command_table, NULL);
    Jim_CreateCommand(interp, "mk.view.finalizer", JimViewFinalizerProc, NULL, NULL);

    return JIM_OK;
}

} /* extern "C" */
