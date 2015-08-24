/*
 * Jim - zlib bindings
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

#include <zlib.h>

#include <jim.h>

#define _PASTE(x) # x
#define PASTE(x) _PASTE(x)

static int Jim_Crc32(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long init;
    const char *in;
    int len;

    switch (argc) {
    case 3:
        init = crc32(0L, Z_NULL, 0);
        break;

    case 4:
        if (Jim_GetLong(interp, argv[3], &init) != JIM_OK) {
            return JIM_ERR;
        }
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "string ?startValue?");
        return JIM_ERR;
    }

    in = Jim_GetString(argv[2], &len);
    Jim_SetResultInt(interp, (int)crc32((uLong)init, (const Bytef *)in, (uInt) len));

    return JIM_OK;
}

static int Jim_Deflate(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    z_stream strm = {0};
    long level;
    Bytef *buf;
    const char *in;
    int len;

    switch (argc) {
    case 3:
        /* if no compression level is specified, use zlib's default */
        level = Z_DEFAULT_COMPRESSION;
        break;

    case 4:
        if (Jim_GetLong(interp, argv[3], &level) != JIM_OK) {
            return JIM_ERR;
        }

        if ((level < Z_NO_COMPRESSION) || (level > Z_BEST_COMPRESSION)) {
            Jim_SetResultString(interp, "level must be 0 to 9", -1);
            return JIM_ERR;
        }

        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "string ?level?");
        return JIM_ERR;
    }

    if (deflateInit2(&strm, level, Z_DEFLATED, -MAX_WBITS, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
        return JIM_ERR;
    }

    in = Jim_GetString(argv[2], &len);

    strm.avail_out = deflateBound(&strm, (uLong)len);
    if (strm.avail_out > INT_MAX) {
        deflateEnd(&strm);
        return JIM_ERR;
    }
    buf = (Bytef *)Jim_Alloc((int)strm.avail_out);
    strm.next_out = buf;
    strm.next_in = (Bytef *)in;
    strm.avail_in = (uInt)len;

    /* always compress in one pass - the return value holds the entire
     * decompressed data anyway, so there's no reason to do chunked
     * decompression */
    if (deflate(&strm, Z_FINISH) != Z_STREAM_END) {
        Jim_Free(strm.next_out);
        deflateEnd(&strm);
        return JIM_ERR;
    }

    deflateEnd(&strm);

    if (strm.total_out > INT_MAX) {
        Jim_Free(strm.next_out);
        return JIM_ERR;
    }

    Jim_SetResult(interp, Jim_NewStringObjNoAlloc(interp, (char *)buf, (int)strm.total_out));
    return JIM_OK;
}

static int Jim_Inflate(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    z_stream strm = {0};
    long bufsiz;
    void *buf;
    const char *in;
    Jim_Obj *out;
    int inlen, ret;

    switch (argc) {
    case 3:
        /* use small 64K chunks if no size was specified, to reduce memory
         * consumption */
        bufsiz = 64 * 1024;
        break;

    case 4:
        if (Jim_GetLong(interp, argv[3], &bufsiz) != JIM_OK) {
            return JIM_ERR;
        }
        if ((bufsiz <= 0) || (bufsiz > INT_MAX)) {
            Jim_SetResultString(interp, "buffer size must be 0 to "PASTE(INT_MAX), -1);
            return JIM_ERR;
        }
        break;

    default:
        Jim_WrongNumArgs(interp, 1, argv, "data ?bufferSize?");
        return JIM_ERR;
    }

    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) {
        return JIM_ERR;
    }

    in = Jim_GetString(argv[2], &inlen);

    /* allocate a buffer - decompression is done in chunks, into this buffer;
     * when the decompressed data size is given, decompression is faster because
     * it's done in one pass, with less memcpy() overhead */
    buf = Jim_Alloc((int)bufsiz);

    out = Jim_NewEmptyStringObj(interp);
    Jim_IncrRefCount(out);

    strm.next_in = (Bytef*)in;
    strm.avail_in = (uInt)inlen;
    do {
        do {
            strm.next_out = buf;
            strm.avail_out = (uInt)bufsiz;

            ret = inflate(&strm, Z_NO_FLUSH);
            switch (ret) {
            case Z_OK:
            case Z_STREAM_END:
                /* append each chunk to the output object */
                Jim_AppendString(interp, out, buf, (int)(bufsiz - (long)strm.avail_out));
                break;

            default:
                Jim_DecrRefCount(interp, out);
                Jim_Free(buf);
                inflateEnd(&strm);
                return JIM_ERR;
            }
        } while (strm.avail_out == 0);
    } while (ret != Z_STREAM_END);

    /* free memory used for decompression before we assign the return value */
    Jim_Free(buf);
    inflateEnd(&strm);

    Jim_SetResult(interp, out);
    Jim_DecrRefCount(interp, out);

    return JIM_OK;
}

static int JimZlibCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    static const char * const options[] = { "crc32", "deflate", "inflate", NULL };
    int option;
    enum { OPT_CRC32, OPT_DEFLATE, OPT_INFLATE };

    if (argc < 2) {
        Jim_WrongNumArgs(interp, 1, argv, "method ?args ...?");
        return JIM_ERR;
    }

    if (Jim_GetEnum(interp, argv[1], options, &option, "zlib method", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }

    switch (option) {
    case OPT_CRC32:
        return Jim_Crc32(interp, argc, argv);

    case OPT_DEFLATE:
        return Jim_Deflate(interp, argc, argv);

    case OPT_INFLATE:
        return Jim_Inflate(interp, argc, argv);
    }

    return JIM_ERR;
}

int Jim_zlibInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "zlib", "1.0", JIM_ERRMSG)) {
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "zlib", JimZlibCmd, 0, 0);

    return JIM_OK;
}
