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
#include <stdint.h>
#include <limits.h>
#include <dlfcn.h>
#include <string.h>

#include <ffi.h>

#include <jim.h>
#include <jim-subcmd.h>

#if INT64_MAX <= JIM_WIDE_MAX
#define TYPE_NAMES "char", "uchar", "short", "ushort", "int", "uint", "long", \
                   "ulong", "pointer", "double", "float", "int8", "uint8", \
                   "int16", "uint16", "int32", "uint32", "int64", "uint64"
#else
#define TYPE_NAMES "char", "uchar", "short", "ushort", "int", "uint", "long", \
                   "ulong", "pointer", "double", "float", "int8", "uint8", \
                   "int16", "uint16", "int32", "uint32"
#endif

struct ffi_var {
    /* may be different than the libffi type size - i.e the size of a buffer
     * object is its actual size, not the size of a pointer */
    size_t size;
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

        uint64_t ui64;
        int64_t i64;
        uint32_t ui32;
        int32_t i32;
        uint16_t ui16;
        int16_t i16;
        uint8_t ui8;
        int8_t i8;

        float f;
        double d;
    } val;
    ffi_type *type;
    void (*to_str)(Jim_Interp *, const struct ffi_var *);
    void *addr; /* points to the val member that holds the value */
};

struct ffi_struct {
    ffi_type type;
    int size;
    int nmemb;
    int *offs;
    unsigned char *buf;
};

struct ffi_func {
    ffi_cif cif;
    ffi_type *rtype;
    ffi_type **atypes;
    void *p;
    int nargs; /* used to verify of the number of arguments, when called */
};

/* constants */

static ffi_type *type_structs[] = {
#if INT64_MAX <= JIM_WIDE_MAX
    &ffi_type_schar, &ffi_type_uchar, &ffi_type_sshort, &ffi_type_ushort,
    &ffi_type_sint, &ffi_type_uint, &ffi_type_slong, &ffi_type_ulong,
    &ffi_type_pointer, &ffi_type_double, &ffi_type_float, &ffi_type_sint8,
    &ffi_type_uint8, &ffi_type_sint16, &ffi_type_uint16, &ffi_type_sint32,
    &ffi_type_uint32, &ffi_type_sint64, &ffi_type_uint64, &ffi_type_void
#else
    &ffi_type_schar, &ffi_type_uchar, &ffi_type_sshort, &ffi_type_ushort,
    &ffi_type_sint, &ffi_type_uint, &ffi_type_slong, &ffi_type_ulong,
    &ffi_type_pointer, &ffi_type_double, &ffi_type_float, &ffi_type_sint8,
    &ffi_type_uint8, &ffi_type_sint16, &ffi_type_uint16, &ffi_type_sint32,
    &ffi_type_uint32, &ffi_type_void
#endif
};

static struct ffi_var null_var;
static struct ffi_var zero_var;
static struct ffi_var one_var;
static Jim_Obj *main_obj;

/* common functions */

static int JimAddress(Jim_Interp *interp, const void *p)
{
    char buf[32];

    sprintf(buf, "%p", p);
    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

/* variable methods */

static void JimVariableDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_var *var = privData;

    JIM_NOTUSED(interp);
    Jim_Free(var);
}

static int JimVariableValue(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_var *var = Jim_CmdPrivData(interp);

    var->to_str(interp, var);

    return JIM_OK;
}

static int JimVariableAddress(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_var *var = Jim_CmdPrivData(interp);

    return JimAddress(interp, var->addr);
}

static int JimVariableSize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_var *var = Jim_CmdPrivData(interp);

    if (var->size > INT_MAX) {
        Jim_SetResultFormatted(interp, "bad variable size for: %#s", argv[0]);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, (int)var->size);

    return JIM_OK;
}

static int JimVariableRaw(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_var *var = Jim_CmdPrivData(interp);

    if (var->size > INT_MAX) {
        Jim_SetResultFormatted(interp, "bad variable size for: %#s", argv[0]);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, (char *)var->addr, (int)var->size);

    return JIM_OK;
}

static const jim_subcmd_type variable_command_table[] = {
    {   "value",
        NULL,
        JimVariableValue,
        0,
        0,
        /* Description: Retrieves the value of a variable */
    },
    {   "address",
        NULL,
        JimVariableAddress,
        0,
        0,
        /* Description: Returns the address of a variable */
    },
    {   "size",
        NULL,
        JimVariableSize,
        0,
        0,
        JIM_MODFLAG_FULLARGV
        /* Description: Returns the size of a variable */
    },
    {   "raw",
        NULL,
        JimVariableRaw,
        0,
        0,
        JIM_MODFLAG_FULLARGV
        /* Description: Returns the raw value of a variable */
    }
};

static int JimVariableHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, variable_command_table, argc, argv), argc, argv);
}

/* common int methods */

static struct ffi_var *Jim_NewIntBase(Jim_Interp *interp, const char *name)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    sprintf(buf, "ffi.%s%ld", name, Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVariableHandlerCommand, var, JimVariableDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return var;
}

/* {u,}int8 methods */

static void Jim_Int8ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.i8);
}

static void Jim_NewInt8(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int8");

    var->val.i8 = (int8_t)val;
    var->type = &ffi_type_sint8;
    var->to_str = Jim_Int8ToStr;
    var->addr = &var->val.i8;
    var->size = sizeof(var->val.i8);
}

static void Jim_Uint8ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ui8);
}

static void Jim_NewUint8(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint8");

    var->val.ui8 = (uint8_t)val;
    var->type = &ffi_type_uint8;
    var->to_str = Jim_Uint8ToStr;
    var->addr = &var->val.ui8;
    var->size = sizeof(var->val.ui8);
}

/* {u,}int16 methods */

static void Jim_Int16ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.i16);
}

static void Jim_NewInt16(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int16");

    var->val.i16 = (int16_t)val;
    var->type = &ffi_type_sint16;
    var->to_str = Jim_Int16ToStr;
    var->addr = &var->val.i16;
    var->size = sizeof(var->val.i16);
}

static void Jim_Uint16ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ui16);
}

static void Jim_NewUint16(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint16");

    var->val.ui16 = (uint16_t)val;
    var->type = &ffi_type_uint16;
    var->to_str = Jim_Uint16ToStr;
    var->addr = &var->val.ui16;
    var->size = sizeof(var->val.ui16);
}

/* {u,}int32 methods */

static void Jim_Int32ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.i32);
}

static void Jim_NewInt32(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int32");

    var->val.i32 = (int32_t)val;
    var->type = &ffi_type_sint32;
    var->to_str = Jim_Int32ToStr;
    var->addr = &var->val.i32;
    var->size = sizeof(var->val.i32);
}

static void Jim_Uint32ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ui32);
}

static void Jim_NewUint32(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint32");

    var->val.ui32 = (uint32_t)val;
    var->type = &ffi_type_uint32;
    var->to_str = Jim_Uint32ToStr;
    var->addr = &var->val.ui32;
    var->size = sizeof(var->val.ui32);
}

/* {u,}int64 methods */

#if INT64_MAX <= JIM_WIDE_MAX

static void Jim_Int64ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.i64);
}

static void Jim_NewInt64(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int64");

    var->val.i64 = (int64_t)val;
    var->type = &ffi_type_sint64;
    var->to_str = Jim_Int64ToStr;
    var->addr = &var->val.i64;
    var->size = sizeof(var->val.i64);
}

static void Jim_Uint64ToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ui64);
}

static void Jim_NewUint64(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint64");

    var->val.ui64 = (uint64_t)val;
    var->type = &ffi_type_uint64;
    var->to_str = Jim_Uint64ToStr;
    var->addr = &var->val.ui64;
    var->size = sizeof(var->val.ui64);
}

#endif

/* char methods */

static void Jim_CharToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    char buf[2];

    /* char objects are represented as Tcl strings of length 1 */
    buf[0] = var->val.c;
    buf[1] = '\0';
    Jim_SetResultString(interp, buf, 1);
}

static void Jim_NewChar(Jim_Interp *interp, const char val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "char");

    var->val.c = val;
    var->type = &ffi_type_schar;
    var->to_str = Jim_CharToStr;
    var->addr = &var->val.c;
    var->size = sizeof(var->val.c);
}

static void Jim_UcharToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    /* uchar objects are represented as Tcl integers */
    Jim_SetResultInt(interp, (jim_wide)var->val.uc);
}

static void Jim_NewUchar(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uchar");

    var->val.uc = (unsigned char)val;
    var->type = &ffi_type_uchar;
    var->to_str = Jim_UcharToStr;
    var->addr = &var->val.uc;
    var->size = sizeof(var->val.uc);
}

/* short methods */

static void Jim_ShortToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.s);
}

static void Jim_NewShort(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "short");

    var->val.s = (short)val;
    var->type = &ffi_type_sshort;
    var->to_str = Jim_ShortToStr;
    var->addr = &var->val.s;
    var->size = sizeof(var->val.s);
}

static void Jim_UshortToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.us);
}

static void Jim_NewUshort(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "ushort");

    var->val.us = (unsigned short)val;
    var->type = &ffi_type_ushort;
    var->to_str = Jim_UshortToStr;
    var->addr = &var->val.us;
    var->size = sizeof(var->val.us);
}

/* int methods */

static void Jim_IntToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.i);
}

/* needed for statically-allocated int objects - we use it for global
 * constants */
static void Jim_NewIntNoAlloc(Jim_Interp *interp,
                              struct ffi_var *var,
                              const jim_wide val,
                              char *buf)
{
    sprintf(buf, "ffi.int%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVariableHandlerCommand, var, NULL);

    var->val.i = (int)val;
    var->type = &ffi_type_sint;
    var->to_str = Jim_IntToStr;
    var->addr = &var->val.i;
    var->size = sizeof(var->val.i);
}

static void Jim_NewInt(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "int");

    var->val.i = (int)val;
    var->type = &ffi_type_sint;
    var->to_str = Jim_IntToStr;
    var->addr = &var->val.i;
    var->size = sizeof(var->val.i);
}

static void Jim_UintToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ui);
}

static void Jim_NewUint(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "uint");

    var->val.ui = (unsigned int)val;
    var->type = &ffi_type_uint;
    var->to_str = Jim_UintToStr;
    var->addr = &var->val.ui;
    var->size = sizeof(var->val.ui);
}

/* long methods */

static void Jim_LongToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.l);
}

static void Jim_NewLong(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "long");

    var->val.l = (long)val;
    var->type = &ffi_type_slong;
    var->to_str = Jim_LongToStr;
    var->addr = &var->val.l;
    var->size = sizeof(var->val.l);
}

static void Jim_UlongToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    Jim_SetResultInt(interp, (jim_wide)var->val.ul);
}

static void Jim_NewUlong(Jim_Interp *interp, const jim_wide val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "ulong");

    var->val.ul = (unsigned long)val;
    var->type = &ffi_type_ulong;
    var->to_str = Jim_UlongToStr;
    var->addr = &var->val.ul;
    var->size = sizeof(var->val.ul);
}

/* float methods */

static void Jim_FloatToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    char buf[JIM_DOUBLE_SPACE + 1];

    sprintf(buf, "%.12g", (double) var->val.f);
    Jim_SetResultString(interp, buf, -1);
}

static void Jim_NewFloat(Jim_Interp *interp, const float val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "float");

    var->val.f = val;
    var->type = &ffi_type_float;
    var->to_str = Jim_FloatToStr;
    var->addr = &var->val.f;
    var->size = sizeof(var->val.f);
}

/* double methods */

static void Jim_DoubleToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    char buf[JIM_DOUBLE_SPACE + 1];

    sprintf(buf, "%.12g", var->val.d);
    Jim_SetResultString(interp, buf, -1);
}

static void Jim_NewDouble(Jim_Interp *interp, const double val)
{
    struct ffi_var *var;

    var = Jim_NewIntBase(interp, "double");

    var->val.d = (double)val;
    var->type = &ffi_type_double;
    var->to_str = Jim_DoubleToStr;
    var->addr = &var->val.d;
    var->size = sizeof(var->val.d);
}

/* pointer methods */

static void Jim_PointerToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    char buf[32];

    /* sprintf() may represent NULL as "(nil)", which isn't a valid integer we
     * can use later (e.g during a function call) */
    if (var->val.vp == NULL) {
        Jim_SetResultString(interp, "0x0", -1);
    } else {
        sprintf(buf, "%p", var->val.vp);
        Jim_SetResultString(interp, buf, -1);
    }
}

/* like Jim_NewIntNoAlloc, this one is used for constants */
static void Jim_NewPointerNoAlloc(Jim_Interp *interp,
                                  struct ffi_var *var,
                                  void *p,
                                  char *buf,
                                  void (*del)(Jim_Interp *, void *))
{
    sprintf(buf, "ffi.pointer%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVariableHandlerCommand, var, del);

    var->val.vp = p;
    var->type = &ffi_type_pointer;
    var->to_str = Jim_PointerToStr;
    var->addr = &var->val.vp;
    var->size = sizeof(var->val.vp);
}

static void Jim_NewPointer(Jim_Interp *interp, void *p)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    Jim_NewPointerNoAlloc(interp, var, p, buf, JimVariableDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));
}

/* buffer methods */

static void JimBufferDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_var *var = privData;

    JIM_NOTUSED(interp);
    /* the buffer itself needs to be freed */
    Jim_Free(var->val.vp);
    JimVariableDelProc(interp, privData);
}

static void Jim_BufferToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    /* dangerous - we assume the string is null-terminated */
    Jim_SetResultString(interp, var->val.vp, -1);
}

static void Jim_NewBuffer(Jim_Interp *interp, void *p, const size_t size)
{
    char buf[32];
    struct ffi_var *var;

    var = Jim_Alloc(sizeof(*var));

    sprintf(buf, "ffi.buffer%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVariableHandlerCommand, var, JimBufferDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    var->val.vp = p;
    var->type = &ffi_type_pointer;
    var->to_str = Jim_BufferToStr;
    var->addr = &var->val.vp;
    /* the size of a buffer object is not determined by the value (i.e pointer)
     * size */
    var->size = size;
}

/* int commands */

static int JimIntBaseCmd(Jim_Interp *interp,
                         int argc,
                         Jim_Obj *const *argv,
                         const jim_wide min,
                         const jim_wide max,
                         void (*new_obj)(Jim_Interp *, const jim_wide))
{
    jim_wide val;

    switch (argc) {
    case 1:
        break;

    case 2:
        if (Jim_GetWide(interp, argv[1], &val) != JIM_OK) {
            Jim_SetResultFormatted(interp, "invalid integer value: %#s", argv[1]);
            return JIM_ERR;
        }
        if ((val < min) || (val > max)) {
            Jim_SetResultFormatted(interp, "bad integer value: %#s", argv[1]);
            return JIM_ERR;
        }
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "?val?");
        return JIM_ERR;
    }

    new_obj(interp, val);
    return JIM_OK;
}

static int JimUintBaseCmd(Jim_Interp *interp,
                          int argc,
                          Jim_Obj *const *argv,
                          const jim_wide max,
                          void (*new_obj)(Jim_Interp *, const jim_wide))
{
    return JimIntBaseCmd(interp, argc, argv, 0, max, new_obj);
}

static int JimInt8Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, INT8_MIN, INT8_MAX, Jim_NewInt8);
}

static int JimUint8Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UINT8_MAX, Jim_NewUint8);
}

static int JimInt16Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, INT16_MIN, INT16_MAX, Jim_NewInt16);
}

static int JimUint16Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UINT16_MAX, Jim_NewUint16);
}

static int JimInt32Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, INT32_MIN, INT32_MAX, Jim_NewInt32);
}

static int JimUint32Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UINT32_MAX, Jim_NewUint32);
}

#if INT64_MAX <= JIM_WIDE_MAX

static int JimInt64Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimIntBaseCmd(interp, argc, argv, INT64_MIN, INT64_MAX, Jim_NewInt64);
}

static int JimUint64Cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UINT64_MAX, Jim_NewUint64);
}

#endif

static int JimCharCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    const char *s;
    int len;
    char c;

    switch (argc) {
    case 1:
        break;

    case 2:
        s = Jim_GetString(argv[1], &len);
        if (len != 1) {
            Jim_SetResultFormatted(interp, "bad character: %#s", argv[1]);
            return JIM_ERR;
        }

        c = s[0];
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "?val?");
        return JIM_ERR;
    }

    Jim_NewChar(interp, c);
    return JIM_OK;
}

static int JimUcharCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return JimUintBaseCmd(interp, argc, argv, UCHAR_MAX, Jim_NewUchar);
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

/* float commands */

static int JimFloatBaseCmd(Jim_Interp *interp,
                           int argc,
                           Jim_Obj *const *argv,
                           double *val)
{
    switch (argc) {
    case 1:
        break;

    case 2:
        if (Jim_GetDouble(interp, argv[1], val) != JIM_OK) {
            Jim_SetResultFormatted(interp, "invalid double value: %#s", argv[1]);
            return JIM_ERR;
        }
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "?val?");
        return JIM_ERR;
    }

    return JIM_OK;
}

static int JimFloatCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    double val;

    if (JimFloatBaseCmd(interp, argc, argv, &val) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_NewFloat(interp, (float)val);
    return JIM_OK;
}

static int JimDoubleCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    double val;

    if (JimFloatBaseCmd(interp, argc, argv, &val) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_NewDouble(interp, val);
    return JIM_OK;
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
            Jim_WrongNumArgs(interp, 1, argv, "?address?");
            return JIM_ERR;
    }

    Jim_NewPointer(interp, (void *)(intptr_t)val);

    return JIM_OK;
}

static int JimStringAt(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide addr;
    long len;
    char *s;

    if (Jim_GetWide(interp, argv[0], &addr) != JIM_OK) {
        Jim_SetResultFormatted(interp, "bad address: %#s", argv[0]);
        return JIM_ERR;
    }

    s = (char *)(uintptr_t)addr;

    /* in "at" mode, if no length is specified, return the string and assume
     *  it's terminated */
    if (argc == 1) {
        Jim_SetResultString(interp, s, -1);
    } else {
        if (Jim_GetLong(interp, argv[1], &len) != JIM_OK) {
            Jim_SetResultFormatted(interp, "invalid size: %#s", argv[1]);
            return JIM_ERR;
        }

        if ((len < 0) || (len >= INT_MAX)) {
            Jim_SetResultFormatted(interp, "bad size: %#s", argv[1]);
            return JIM_ERR;
        }

        /* otherwise, terminate it first */
        s[len] = '\0';
        Jim_SetResultString(interp, s, (int)(len + 1));
    }

    return JIM_OK;
}

static int JimStringCopy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *s;
    int len;

    /* in "copy" mode, copy the string into a new buffer object */
    s = (char *)Jim_GetString(argv[0], &len);
    Jim_NewBuffer(interp, (void *)Jim_StrDupLen(s, len), (size_t) len);

    return JIM_OK;
}

static const jim_subcmd_type string_command_table[] = {
    {   "at",
        "address ?len?",
        JimStringAt,
        1,
        2,
        /* Description: Reads a string at a given address */
    },
    {   "copy",
        "value",
        JimStringCopy,
        1,
        1,
        /* Description: Creates a new string and initializes it with a given value */
    }
};

static int JimStringCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, string_command_table, argc, argv), argc, argv);
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

    Jim_NewBuffer(interp, Jim_Alloc((int)len), (size_t)len);
    return JIM_OK;
}

/* misc. commands */

static void Jim_VoidToStr(Jim_Interp *interp, const struct ffi_var *var)
{
    /* void objects are represented as empty strings */
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
    var->size = 0;

    sprintf(buf, "ffi.void%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimVariableHandlerCommand, var, JimVariableDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

/* struct commands */

static void JimStructDelProc(Jim_Interp *interp, void *privData)
{
    struct ffi_struct *s = privData;

    JIM_NOTUSED(interp);
    Jim_Free(s->buf);
    Jim_Free(s->offs);
    Jim_Free(s->type.elements);
    Jim_Free(s);
}

static void Jim_RawValueToObj(Jim_Interp *interp, void *p, const ffi_type *type)
{
    if (type == &ffi_type_pointer) {
        Jim_NewPointer(interp, *((void **) p));
    /* ffi_type_sint, ffi_type_slong, etc' are #defines of the fixed-width
     * integer types */
#if INT64_MAX <= JIM_WIDE_MAX
    } else if (type == &ffi_type_uint64) {
        Jim_NewUint64(interp, (jim_wide)(*((uint64_t *)p)));
    } else if (type == &ffi_type_sint64) {
        Jim_NewInt64(interp, (jim_wide)(*((int64_t *)p)));
#endif
    } else if (type == &ffi_type_uint32) {
        Jim_NewUint32(interp, (jim_wide)(*((uint32_t *)p)));
    } else if (type == &ffi_type_sint32) {
        Jim_NewInt32(interp, (jim_wide)(*((int32_t *)p)));
    } else if (type == &ffi_type_uint16) {
        Jim_NewUint16(interp, (jim_wide)(*((uint16_t *)p)));
    } else if (type == &ffi_type_sint16) {
        Jim_NewInt16(interp, (jim_wide)(*((int16_t *)p)));
    } else if (type == &ffi_type_uint8) {
        Jim_NewUint8(interp, (jim_wide)(*((uint8_t *)p)));
    } else if (type == &ffi_type_sint8) {
        Jim_NewInt8(interp, (jim_wide)(*((int8_t *)p)));
    } else if (type == &ffi_type_float) {
        Jim_NewFloat(interp, (*((float *)p)));
    } else { /* raw values cannot be of type void */
        Jim_NewDouble(interp, (*((double *)p)));
    }
}

static int JimStructMember(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long memb;
    struct ffi_struct *s = Jim_CmdPrivData(interp);

    if (Jim_GetLong(interp, argv[0], &memb) != JIM_OK) {
        Jim_SetResultFormatted(interp, "invalid member index: %#s", argv[0]);
        return JIM_ERR;
    }

    if ((memb < 0) || ((int)memb >= s->nmemb)) {
        Jim_SetResultFormatted(interp, "bad member index: %#s", argv[0]);
        return JIM_ERR;
    }

    /* create a Tcl representation of the member value */
    Jim_RawValueToObj(interp,
                      s->buf + s->offs[memb],
                      s->type.elements[memb]);

    return JIM_OK;
}

static int JimStructAddress(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_struct *s = Jim_CmdPrivData(interp);

    return JimAddress(interp, s->buf);
}

static int JimStructSize(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct ffi_struct *s = Jim_CmdPrivData(interp);

    Jim_SetResultInt(interp, s->size);

    return JIM_OK;
}

static const jim_subcmd_type struct_command_table[] = {
    {   "member",
        "index",
        JimStructMember,
        1,
        1,
        /* Description: Retrieves a struct member */
    },
    {   "address",
        NULL,
        JimStructAddress,
        0,
        0,
        /* Description: Returns the address of a struct */
    },
    {   "size",
        NULL,
        JimStructSize,
        0,
        0,
        /* Description: Returns the size of a struct */
    }
};

static int JimStructHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, struct_command_table, argc, argv), argc, argv);
}

static int JimStructCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const type_names[] = {TYPE_NAMES, NULL };
    char buf[32];
    struct ffi_struct *s;
    const char *raw;
    int i, j, len, off;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "raw member ?member ...?");
        return JIM_ERR;
    }

    raw = Jim_GetString(argv[1], &len);

    s = Jim_Alloc(sizeof(*s));
    s->nmemb = argc - 1;
    s->type.elements = Jim_Alloc(sizeof(ffi_type *) * (1 + s->nmemb));
    s->offs = Jim_Alloc(sizeof(int) * s->nmemb);

    s->size = 0;
    off = 0;
    for (i = 2; i < argc; ++i) {
        if (Jim_GetEnum(interp, argv[i], type_names, &j, "ffi type", JIM_ERRMSG) != JIM_OK) {
            Jim_Free(s->offs);
            Jim_Free(s->type.elements);
            Jim_Free(s);
            return JIM_ERR;
        }
        s->type.elements[i - 2] = type_structs[j];

        if (s->size >= (INT_MAX - type_structs[j]->size)) {
            Jim_Free(s->offs);
            Jim_Free(s->type.elements);
            Jim_Free(s);
            Jim_SetResultString(interp, "bad struct size", -1);
            return JIM_ERR;
        }
        /* cache the member offset inside the raw struct */
        s->offs[i - 2] = off;
        off += type_structs[j]->size;
    }

    s->size += off;

    /* if an initializer is specified, it must be the same size as the struct */
    if ((len != 0) && (s->size != (size_t) len)) {
        Jim_Free(s->offs);
        Jim_Free(s->type.elements);
        Jim_Free(s);
        Jim_SetResultFormatted(interp, "bad struct initializer: %#s", argv[1]);
        return JIM_ERR;
    }

    s->buf = Jim_Alloc(s->size);
    if (len != 0) {
        /* copy the initializer */
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
    int i, nargs;
    const char *s;

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "ret ?args ...?");
        return JIM_ERR;
    }

    nargs = argc - 2;
    if (nargs != f->nargs) {
        Jim_SetResultString(interp, "received the wrong number of arguments", -1);
        return JIM_ERR;
    }

    args = Jim_Alloc(sizeof(void *) * nargs);

    s = Jim_String(argv[1]);
    /* see the comment Jim_PointerToStr() - NULL is a special case */
    if (sscanf(s, "%p", &ret) != 1) {
        Jim_SetResultFormatted(interp, "bad pointer: %s", s);
        return JIM_ERR;
    }

    for (i = 0; i < nargs; ++i) {
        s = Jim_String(argv[2 + i]);
        if (sscanf(s, "%p", &args[i]) != 1) {
            Jim_Free(args);
            Jim_SetResultFormatted(interp, "bad pointer: %s", s);
            return JIM_ERR;
        }
    }

    ffi_call(&f->cif, FFI_FN(f->p), ret, args);
    Jim_Free(args);

    /* use the return value address as the return value, to allow one-liners */
    Jim_SetResult(interp, argv[1]);

    return JIM_OK;
}

static int JimFunctionCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    /* void is a legal type only for function return values */
    static const char * const type_names[] = { TYPE_NAMES, "void", NULL };
    struct ffi_func *f;
    const char *addr;
    int i, j;

    if (argc < 3) {
        Jim_WrongNumArgs(interp, 1, argv, "rtype addr ?argtypes ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], type_names, &i, "ffi type", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    f = Jim_Alloc(sizeof(*f));

    addr = Jim_String(argv[2]);
    if (sscanf(addr, "%p", &f->p) != 1) {
        Jim_Free(f);
        Jim_SetResultFormatted(interp, "invalid function address: %#s", argv[2]);
        return JIM_ERR;
    }
    if (f->p == NULL) {
        Jim_Free(f);
        Jim_SetResultString(interp, "NULL function address", -1);
        return JIM_ERR;
    }

    f->rtype = type_structs[i];
    f->nargs = argc - 3;

    f->atypes = Jim_Alloc(sizeof(char *) * f->nargs);
    for (i = 3; i < argc; ++i) {
        if (Jim_GetEnum(interp, argv[i], type_names, &j, "ffi type", JIM_ERRMSG) != JIM_OK) {
            Jim_Free(f->atypes);
            Jim_Free(f);
            return JIM_ERR;
        }

        if (type_structs[j] == &ffi_type_void) {
            Jim_Free(f->atypes);
            Jim_Free(f);
            Jim_SetResultString(interp, "the type of a function argument cannot be void", -1);
            return JIM_ERR;
        }

        f->atypes[i - 3] = type_structs[j];
    }

    if (ffi_prep_cif(&f->cif, FFI_DEFAULT_ABI, f->nargs, f->rtype, f->atypes) != FFI_OK) {
        Jim_Free(f->atypes);
        Jim_Free(f);
        return JIM_ERR;
    }

    sprintf(buf, "ffi.function%ld", Jim_GetId(interp));
    Jim_CreateCommand(interp, buf, JimFunctionHandlerCommand, f, JimFunctionDelProc);
    Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));

    return JIM_OK;
}

/* library commands */

static void JimLibraryDelProc(Jim_Interp *interp, void *privData)
{
    JIM_NOTUSED(interp);
    dlclose(privData);
}

static int JimLibraryDlsym(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    void *p, *h = Jim_CmdPrivData(interp);
    const char *sym;

    sym = Jim_String(argv[0]);

    p = dlsym(h, sym);
    if (p == NULL) {
        Jim_SetResultFormatted(interp, "failed to resolve %s", sym);
        return JIM_ERR;
    }

    sprintf(buf, "%p", p);
    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

static int JimLibraryHandle(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[32];
    void *h = Jim_CmdPrivData(interp);

    sprintf(buf, "%p", h);
    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

static const jim_subcmd_type library_command_table[] = {
    {   "dlsym",
        "symbol",
        JimLibraryDlsym,
        1,
        1,
        /* Description: Resolves the address of a symbol */
    },
    {   "handle",
        NULL,
        JimLibraryHandle,
        0,
        0,
        /* Description: Returns the library handle */
    }
};

static int JimLibraryHandlerCommand(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, library_command_table, argc, argv), argc, argv);
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

    /* if no path is specified, return another reference to ::main */
    if (len == 0) {
        Jim_SetResult(interp, main_obj);
    } else {
        h = dlopen(path, RTLD_LAZY);
        if (h == NULL) {
            Jim_SetResultFormatted(interp, "failed to load %#s", argv[1]);
            return JIM_ERR;
        }

        sprintf(buf, "ffi.library%ld", Jim_GetId(interp));
        Jim_CreateCommand(interp, buf, JimLibraryHandlerCommand, h, JimLibraryDelProc);
        Jim_SetResult(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, buf, -1)));
    }

    return JIM_OK;
}

/* misc. commands */

static int JimCastCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    jim_wide addr;
    static const char * const type_names[] = { TYPE_NAMES, NULL };
    void *p;
    int i;

    if (argc != 3) {
        Jim_WrongNumArgs(interp, 1, argv, "type addr");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], type_names, &i, "ffi type", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    if (Jim_GetWide(interp, argv[2], &addr) != JIM_OK) {
        Jim_SetResultFormatted(interp, "invalid address: %#s", argv[2]);
        return JIM_ERR;
    }

    p = (void *)(intptr_t)addr;
    if (p == NULL) {
        Jim_SetResultString(interp, "NULL variable address", -1);
        return JIM_ERR;
    }

    Jim_RawValueToObj(interp, p, type_structs[i]);

    return JIM_OK;
}

int Jim_ffiInit(Jim_Interp *interp)
{
    char buf[32];
    void *self;

    self = dlopen(NULL, RTLD_LAZY);
    if (self == NULL) {
        return JIM_ERR;
    }

    if (Jim_PackageProvide(interp, "ffi", "1.0", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "ffi::pointer", JimPointerCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::ulong", JimUlongCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::long", JimLongCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::uint", JimUintCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::int", JimIntCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::ushort", JimUshortCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::short", JimShortCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::uchar", JimUcharCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::char", JimCharCmd, 0, 0);

#if INT64_MAX <= JIM_WIDE_MAX
    Jim_CreateCommand(interp, "ffi::uint64", JimUint64Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::int64", JimInt64Cmd, 0, 0);
#endif
    Jim_CreateCommand(interp, "ffi::uint32", JimUint32Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::int32", JimInt32Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::uint16", JimUint16Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::int16", JimInt16Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::uint8", JimUint8Cmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::int8", JimInt8Cmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::float", JimFloatCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::double", JimDoubleCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::string", JimStringCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::buffer", JimBufferCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::void", JimVoidCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::struct", JimStructCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::function", JimFunctionCmd, 0, 0);
    Jim_CreateCommand(interp, "ffi::dlopen", JimDlopenCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi::cast", JimCastCmd, 0, 0);

    Jim_CreateCommand(interp, "ffi.handle0", JimLibraryHandlerCommand, self, JimLibraryDelProc);
    main_obj = Jim_NewStringObj(interp, "ffi.handle0", -1);
    if (Jim_SetVariable(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, "main", -1)), main_obj) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_NewPointerNoAlloc(interp, &null_var, NULL, buf, NULL);
    if (Jim_SetVariable(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, "null", -1)), Jim_NewStringObj(interp, buf, -1)) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_NewIntNoAlloc(interp, &zero_var, 0, buf);
    if (Jim_SetVariable(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, "zero", -1)), Jim_NewStringObj(interp, buf, -1)) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_NewIntNoAlloc(interp, &one_var, 1, buf);
    if (Jim_SetVariable(interp, Jim_MakeGlobalNamespaceName(interp, Jim_NewStringObj(interp, "one", -1)), Jim_NewStringObj(interp, buf, -1)) != JIM_OK) {
        return JIM_ERR;
    }

    return JIM_OK;
}
