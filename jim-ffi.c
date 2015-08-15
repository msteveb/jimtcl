/*
 * Jim - libffi bindings
 *
 * Copyright 2015 Dima Krasner <dima@dimakrasner.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE JIM TCL PROJECT ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * JIM TCL PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the Jim Tcl Project.
 */

#include <inttypes.h>
#include <limits.h>
#include <dlfcn.h>
#include <string.h>

#include <ffi.h>

#include <jim.h>

struct ffi_var {
    union {
        void *vp;
        unsigned long ul;
        long l;
        unsigned int ui;
        int i;
        unsigned short us;
        short s;
        unsigned char uc;
        char c;
    } val;
    ffi_type *type;
    void (*to_str)(Jim_Interp *, const struct ffi_var *);
    void *addr;
};

struct ffi_struct {
    ffi_type type;
    int size;
    int nmemb;
    unsigned char *buf;
};

struct ffi_func {
    ffi_cif cif;
    ffi_type *rtype;
    ffi_type **atypes;
    void *p;
};

/* variable methods */

static void JimVarDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_var *var = privData;

    JIM_NOTUSED(interp);
    Jim_Free(var);
}

static int JimVarHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    static const char * const options[] = {
        "value", "address", "size", "raw", NULL
    };
    struct ffi_var *var = Jim_CmdPrivData(interp);
    int option;
    enum { OPT_VALUE, OPT_ADDRESS, OPT_SIZE, OPT_RAW };

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "ffi variable method", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    switch (option) {
    case OPT_VALUE:
        var->to_str(interp, var);
        return JIM_OK;

    case OPT_ADDRESS:
        sprintf(buf, "%p", var->addr);
        Jim_SetResultString(interp, buf, -1);
        return JIM_OK;

    case OPT_SIZE:
        if (var->type->size > INT_MAX) {
            Jim_SetResultFormatted(interp, "bad variable size for: %#s", argv[1]);
            return JIM_ERR;
        }
        Jim_SetResultInt(interp, (int) var->type->size);
        return JIM_OK;

    case OPT_RAW:
        if (var->type->size > INT_MAX) {
            Jim_SetResultFormatted(interp, "bad variable size for: %#s", argv[1]);
            return JIM_ERR;
        }
        Jim_SetResultString(interp, (char *) var->addr, (int) var->type->size);
        return JIM_OK;
    }

    return JIM_ERR;
}

/* common int methods */

static struct ffi_var *Jim_NewIntBase(Jim_Interp *interp, const char *name)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    sprintf(buf, "ffi.%s%ld", name, Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVarHandlerCommand, var, JimVarDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return var;
}

/* short methods */

static void Jim_ShortToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.s);
}

static void Jim_NewShort(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "short");

    var->val.s = (short) val;
    var->type = &ffi_type_sshort;
    var->to_str = Jim_ShortToStr;
    var->addr = &var->val.s;
}

static void Jim_UshortToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.us);
}

static void Jim_NewUshort(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "ushort");

    var->val.us = (unsigned short) val;
    var->type = &ffi_type_ushort;
    var->to_str = Jim_UshortToStr;
    var->addr = &var->val.us;
}

/* int methods */

static void Jim_IntToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.i);
}

static void Jim_NewInt(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int");

    var->val.i = (int) val;
    var->type = &ffi_type_sint;
    var->to_str = Jim_IntToStr;
    var->addr = &var->val.i;
}

static void Jim_UintToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.ui);
}

static void Jim_NewUint(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint");

    var->val.ui = (unsigned int) val;
    var->type = &ffi_type_uint;
    var->to_str = Jim_UintToStr;
    var->addr = &var->val.ui;
}

/* long methods */

static void Jim_LongToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.l);
}

static void Jim_NewLong(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "long");

    var->val.l = (long) val;
    var->type = &ffi_type_slong;
    var->to_str = Jim_LongToStr;
    var->addr = &var->val.l;
}

static void Jim_UlongToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide) var->val.ul);
}

static void Jim_NewUlong(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "ulong");

    var->val.ul = (unsigned long) val;
    var->type = &ffi_type_ulong;
    var->to_str = Jim_UlongToStr;
    var->addr = &var->val.ul;
}

/* pointer methods */

static void Jim_PointerToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    char buf[32];

    sprintf(buf, "%p", var->val.vp);
    Jim_SetResultString(interp, buf, -1);
}

static void Jim_NewPointer(Jim_Interp *interp, void *p)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    sprintf(buf, "ffi.pointer%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVarHandlerCommand, var, JimVarDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    var->val.vp = p;
    var->type = &ffi_type_pointer;
    var->to_str = Jim_PointerToStr;
    var->addr = &var->val.vp;
}

/* buffer methods */

static void JimBufferDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_var *var = privData;

    JIM_NOTUSED(interp);
    Jim_Free(var->val.vp);
    JimVarDelProc(interp, privData);
}

static void Jim_BufferToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultString(interp, var->val.vp, -1);
}

static void Jim_NewBuffer(Jim_Interp *interp, void *p)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    sprintf(buf, "ffi.buffer%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVarHandlerCommand, var, JimBufferDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    var->val.vp = p;
    var->type = &ffi_type_pointer;
    var->to_str = Jim_BufferToStr;
    var->addr = &var->val.vp;
}

/* int commands */

static int JimIntBaseCmd(Jim_Interp *interp,
                         int argc,
                         Jim_Obj *const *argv,
                         const long min,
                         const long max,
                         void (*new_obj)(Jim_Interp *, const jim_wide))
{
    jim_wide val;

    switch (argc) {
    case 1:
        break;

    case 2:
        if (Jim_GetWide(interp, argv[1], &val) != JIM_OK) {
            return JIM_ERR;
        }
        if ((val < min) || (val > max)) {
            Jim_SetResultFormatted(interp, "bad integer value: %#s", argv[1]);
            return JIM_ERR;
        }
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "val");
        return JIM_ERR;
    }

    new_obj(interp, val);
    return JIM_OK;
}

static int JimUintBaseCmd(Jim_Interp *interp,
                          int argc,
                          Jim_Obj *const *argv,
                          const long max,
                          void (*new_obj)(Jim_Interp *, const jim_wide))
{
    return JimIntBaseCmd(interp, argc, argv, 0, max, new_obj);
}

static int JimShortCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, SHRT_MIN, SHRT_MAX, Jim_NewShort);
}

static int JimUshortCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, USHRT_MAX, Jim_NewUshort);
}

static int JimIntCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, INT_MIN, INT_MAX, Jim_NewInt);
}

static int JimUintCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UINT_MAX, Jim_NewUint);
}

static int JimLongCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, LONG_MIN, LONG_MAX, Jim_NewLong);
}

static int JimUlongCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, ULONG_MAX, Jim_NewUlong);
}

/* pointer commands */

static int JimPointerCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide val;

    switch (argc) {
        case 1:
            break;

        case 2:
            if (Jim_GetWide(interp, argv[1], &val) != JIM_OK) {
                return JIM_ERR;
            }
            break;

        case 3:
            Jim_WrongNumArgs(interp, 1, argv, "address");
            return JIM_ERR;
    }

    Jim_NewPointer(interp, (void *) (intptr_t) val);

    return JIM_OK;
}

static int JimStringCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = { "at", "copy", NULL };
    const char *s;
    jim_wide addr;
    long size;
    int option, len;
    enum { OPT_AT, OPT_COPY };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "at|copy str|addr");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "ffi string method", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (option == OPT_COPY) {
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 1, argv, "copy str");
            return JIM_ERR;
        }

        s = Jim_GetString(argv[2], &len);
        Jim_NewBuffer(interp, (void *) Jim_StrDupLen(s, len));
    } else {
        if ((argc != 3) && (argc != 4)) {
            Jim_WrongNumArgs(interp, 1, argv, "at addr ?size?");
            return JIM_ERR;
        }

        if (Jim_GetWide(interp, argv[2], &addr) != JIM_OK) {
            Jim_SetResultFormatted(interp, "bad address: %#s", argv[2]);
            return JIM_ERR;
        }

        if (argc == 3) {
            Jim_SetResultString(interp, (char *) (uintptr_t) addr, -1);
        } else {
            if (Jim_GetLong(interp, argv[3], &size) != JIM_OK) {
                Jim_SetResultFormatted(interp, "invalid size: %#s", argv[3]);
                return JIM_ERR;
            }

            if ((size < 0) || (size > INT_MAX)) {
                Jim_SetResultFormatted(interp, "bad size: %#s", argv[3]);
                return JIM_ERR;
            }

            Jim_SetResultString(interp, (char *) (uintptr_t) addr, (int) size);
        }
    }

    return JIM_OK;
}

static int JimBufferCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long len;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "len");
        return JIM_ERR;
    }

    if (Jim_GetLong(interp, argv[1], &len) != JIM_OK) {
        Jim_SetResultFormatted(interp, "invalid length: %#s", argv[1]);
        return JIM_ERR;
    }

    if ((len <= 0) || (len > INT_MAX)) {
        Jim_SetResultFormatted(interp, "bad length: %#s", argv[1]);
        return JIM_ERR;
    }

    Jim_NewBuffer(interp, Jim_Alloc((int) len));
    return JIM_OK;
}

/* misc. commands */
static void Jim_VoidToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResult(interp, Jim_NewEmptyStringObj(interp));
}

static int JimVoidCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    struct ffi_var *var;

    if (argc != 1) {
        Jim_WrongNumArgs(interp, 1, argv, "");
        return JIM_ERR;
    }

    var = Jim_Alloc(sizeof(*var));
    var->type = &ffi_type_void;
    var->to_str = Jim_VoidToStr;
    var->addr = NULL;

    sprintf(buf, "ffi.void%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVarHandlerCommand, var, JimVarDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

/* struct commands */

static void JimStructDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_struct *s = privData;

    JIM_NOTUSED(interp);
    Jim_Free(s->buf);
    Jim_Free(s->type.elements);
    Jim_Free(s);
}

static int JimStructHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    static const char * const options[] = { "member", "address", "size", NULL };
    struct ffi_struct *s = Jim_CmdPrivData(interp);
    size_t off;
    long memb;
    void *p;
    int option, i;
    enum { OPT_MEMBER, OPT_ADDRESS, OPT_SIZE };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "ffi struct method", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    switch (option) {
    case OPT_MEMBER:
        if (argc != 3) {
            Jim_WrongNumArgs(interp, 1, argv, "index");
            return JIM_ERR;
        }

        if (Jim_GetLong(interp, argv[2], &memb) != JIM_OK) {
            Jim_SetResultFormatted(interp, "invalid member index: %#s", argv[2]);
            return JIM_ERR;
        }
        if ((memb < 0) || ((int) memb >= s->nmemb)) {
            Jim_SetResultFormatted(interp, "bad member index: %#s", argv[2]);
            return JIM_ERR;
        }

        off = 0;
        for (i = 0; i < (int) memb; ++i) {
            off += s->type.elements[i]->size;
        }

        p = s->buf + off;

        if (s->type.elements[memb] == &ffi_type_sshort) {
            Jim_NewShort(interp, (jim_wide) (*((short *) p)));
        } else if (s->type.elements[memb] == &ffi_type_ushort) {
            Jim_NewUshort(interp, (jim_wide) (*((unsigned short *) p)));
        } if (s->type.elements[memb] == &ffi_type_sint) {
            Jim_NewInt(interp, (jim_wide) (*((int *) p)));
        } else if (s->type.elements[memb] == &ffi_type_uint) {
            Jim_NewUint(interp, (jim_wide) (*((unsigned int *) p)));
        } else if (s->type.elements[memb] == &ffi_type_slong) {
            Jim_NewLong(interp, (jim_wide) (*((long *) p)));
        } else if (s->type.elements[memb] == &ffi_type_ulong) {
            Jim_NewUlong(interp, (jim_wide) (*((unsigned long *) p)));
        } else if (s->type.elements[memb] == &ffi_type_pointer) {
            Jim_NewPointer(interp, p);
        }

        return JIM_OK;

    case OPT_ADDRESS:
        sprintf(buf, "%p", s->buf);
        Jim_SetResultString(interp, buf, -1);
        return JIM_OK;

    case OPT_SIZE:
        Jim_SetResultInt(interp, s->size);
        return JIM_OK;
    }

    return JIM_ERR;
}

static int JimStructCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const type_names[] = {
        "short", "ushort", "int", "uint", "long", "ulong", "pointer",  NULL
    };
    ffi_type *types[] = {
        &ffi_type_sshort, &ffi_type_ushort, &ffi_type_sint, &ffi_type_uint,
        &ffi_type_slong, &ffi_type_ulong, &ffi_type_pointer
    };
    char buf[32];
    struct ffi_struct *s;
    const char *raw;
    int i, j, len;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "raw member ?member ...?");
        return JIM_ERR;
    }

    raw = Jim_GetString(argv[1], &len);

    s = Jim_Alloc(sizeof(*s));
    s->nmemb = argc - 1;
    s->type.elements = Jim_Alloc(sizeof(ffi_type *) * (1 + s->nmemb));
    s->size = 0;

    for (i = 2; i < argc; ++i) {
        if (Jim_GetEnum(interp, argv[i], type_names, &j, "ffi type", JIM_ERRMSG) != JIM_OK) {
            Jim_Free(s->type.elements);
            Jim_Free(s);
            return JIM_ERR;
        }
        s->type.elements[i - 2] = types[j];

        if (s->size >= (INT_MAX - types[j]->size)) {
            Jim_SetResultString(interp, "bad struct size", -1);
            Jim_Free(s->type.elements);
            Jim_Free(s);
            return JIM_ERR;
        }
        s->size += types[j]->size;
    }

    if ((0 != len) && (size_t) len != s->size) {
        Jim_SetResultFormatted(interp, "bad struct initializer: %#s", argv[1]);
        Jim_Free(s->type.elements);
        Jim_Free(s);
        return JIM_ERR;
    }

    s->buf = Jim_Alloc(s->size);
    if (0 != len) {
        memcpy(s->buf, raw, (size_t) len);
    }
    s->type.size = 0;
    s->type.alignment = 0;
    s->type.type = FFI_TYPE_STRUCT;
    s->type.elements[s->nmemb] = NULL;

    sprintf(buf, "ffi.struct%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimStructHandlerCommand, s, JimStructDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

/* function commands */

static void JimFunctionDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_func *f = privData;

    JIM_NOTUSED(interp);
    Jim_Free(f->atypes);
    Jim_Free(f);
}

static int JimFunctionHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_func *f = Jim_CmdPrivData(interp);
    void **args, *ret;
    int nargs, i;
    const char *s;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "ret ?args ...?");
        return JIM_ERR;
    }

    nargs = Jim_ListLength(interp, argv[2]);
    args = Jim_Alloc(sizeof(void *) * nargs);

    s = Jim_String(argv[1]);
    if (sscanf(s, "%p", &ret) != 1) {
        Jim_SetResultFormatted(interp, "bad pointer: %s", s);
        return JIM_ERR;
    }

    for (i = 0; i < nargs; ++i) {
        s = Jim_String(Jim_ListGetIndex(interp, argv[2], i));
        if (sscanf(s, "%p", &args[i]) != 1) {
            Jim_Free(args);
            Jim_SetResultFormatted(interp, "bad pointer: %s", s);
            return JIM_ERR;
        }
    }

    ffi_call(&f->cif, f->p, ret, args);

    Jim_Free(args);

    return JIM_OK;
}

/* library commands */

static void JimLibraryDelProc(Jim_Interp *interp, void *privData)
{
    JIM_NOTUSED(interp);
    dlclose(privData);
}

static int JimLibraryHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    static const char * const type_names[] = {
        "short", "ushort", "int", "uint", "long", "ulong", "pointer", "void",
        NULL
    };
    ffi_type *types[] = {
        &ffi_type_sshort, &ffi_type_ushort, &ffi_type_sint, &ffi_type_uint,
        &ffi_type_slong, &ffi_type_ulong, &ffi_type_pointer, &ffi_type_void
    };
    struct ffi_func *f;
    void *h = Jim_CmdPrivData(interp);
    const char *sym;
    int i, j, nargs;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "rtype name ?argtypes ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], type_names, &i, "ffi type", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    f = Jim_Alloc(sizeof(*f));

    sym = Jim_String(argv[2]);
    f->p = dlsym(h, sym);
    if (f->p == NULL) {
        Jim_SetResultFormatted(interp, "failed to resolve %s", sym);
        Jim_Free(f);
        return JIM_ERR;
    }

    f->rtype = types[i];
    nargs = argc - 3;

    f->atypes = Jim_Alloc(sizeof(char *) * nargs);
    for (i = 3; i < argc; ++i) {
        if (Jim_GetEnum(interp, argv[i], type_names, &j, "ffi type", JIM_ERRMSG) != JIM_OK) {
            Jim_Free(f->atypes);
            Jim_Free(f);
            return JIM_ERR;
        }

        if (types[j] == &ffi_type_void) {
            Jim_SetResultString(interp, "the type of a function argument cannot be void", -1);
            Jim_Free(f->atypes);
            Jim_Free(f);
            return JIM_ERR;
        }

        f->atypes[i - 3] = types[j];
    }

    if (ffi_prep_cif(&f->cif, FFI_DEFAULT_ABI, nargs, f->rtype, f->atypes) != FFI_OK) {
        Jim_Free(f->atypes);
        Jim_Free(f);
        return JIM_ERR;
    }

    sprintf(buf, "ffi.function%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimFunctionHandlerCommand, f, JimFunctionDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

static int JimDlopenCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    const char *path;
    void *h;
    int len;

    if (argc != 2) {
        Jim_WrongNumArgs(interp, 1, argv, "path");
        return JIM_ERR;
    }

    path = Jim_GetString(argv[1], &len);
    if (len == 0) {
        path = NULL;
    }

    h = dlopen(path, RTLD_LAZY);
    if (h == NULL) {
        Jim_SetResultFormatted(interp, "failed to load %s", path);
        return JIM_ERR;
    }

    sprintf(buf, "ffi.library%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimLibraryHandlerCommand, h, JimLibraryDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

int Jim_ffiInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "ffi", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "ffi.short", JimShortCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.ushort", JimUshortCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.int", JimIntCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.uint", JimUintCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.long", JimLongCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.ulong", JimUlongCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi.pointer", JimPointerCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.string", JimStringCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi.buffer", JimBufferCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi.void", JimVoidCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi.struct", JimStructCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi.dlopen", JimDlopenCmd, 0, 0);

    return JIM_OK;
}
