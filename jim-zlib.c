/*
 * Jim - zlib bindings
 *
 * Copyright 2015, 2016 Dima Krasner <dima@dimakrasner.com>
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
#include <jim-subcmd.h>

#define WBITS_GZIP (MAX_WBITS | 16)
/* use small 64K chunks if no size was specified during decompression, to reduce memory consumption */
#define DEF_DECOMPRESS_BUFSIZ (64 * 1024)

static int JimZlibCheckBufSize(Jim_Interp *interp, jim_wide bufsiz)
{
    if ((bufsiz <= 0) || (bufsiz > INT_MAX)) {
        Jim_SetResultFormatted(interp, "buffer size must be 0 to %#s", Jim_NewIntObj(interp, INT_MAX));
        return JIM_ERR;
    }
    return JIM_OK;
}

static int Jim_Crc32(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long init;
    const char *in;
    int len;

    if (argc == 1) {
        init = crc32(0L, Z_NULL, 0);
    } else {
        if (Jim_GetLong(interp, argv[1], &init) != JIM_OK) {
            return JIM_ERR;
        }
    }

    in = Jim_GetString(argv[0], &len);
    Jim_SetResultInt(interp, crc32((uLong)init, (const Bytef *)in, (uInt)len) & 0xFFFFFFFF);

    return JIM_OK;
}

static int Jim_Compress(Jim_Interp *interp, const char *in, int len, long level, int wbits)
{
    z_stream strm = {0};
    Bytef *buf;

    if ((level != Z_DEFAULT_COMPRESSION) && ((level < Z_NO_COMPRESSION) || (level > Z_BEST_COMPRESSION))) {
        Jim_SetResultString(interp, "level must be 0 to 9", -1);
        return JIM_ERR;
    }

    if (deflateInit2(&strm, level, Z_DEFLATED, wbits, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK) {
        return JIM_ERR;
    }

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

static int Jim_Deflate(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long level = Z_DEFAULT_COMPRESSION;
    const char *in;
    int len;

    if (argc != 1) {
        if (Jim_GetLong(interp, argv[1], &level) != JIM_OK) {
            return JIM_ERR;
        }
    }

    in = Jim_GetString(argv[0], &len);
    return Jim_Compress(interp, in, len, level, -MAX_WBITS);
}

static int Jim_Gzip(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long level = Z_DEFAULT_COMPRESSION;
    const char *in;
    int len;

    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[1], "-level")) {
            return -1;
        }

        if (Jim_GetLong(interp, argv[2], &level) != JIM_OK) {
            return -1;
        }

    }
    else if (argc != 1) {
        return -1;
    }

    in = Jim_GetString(argv[0], &len);
    return Jim_Compress(interp, in, len, level, WBITS_GZIP);
}

static int Jim_Decompress(Jim_Interp *interp, const char *in, int len, long bufsiz, int wbits)
{
    z_stream strm = {0};
    void *buf;
    Jim_Obj *out;
    int ret;

    if (JimZlibCheckBufSize(interp, bufsiz)) {
        return JIM_ERR;
    }

    if (inflateInit2(&strm, wbits) != Z_OK) {
        return JIM_ERR;
    }

    /* allocate a buffer - decompression is done in chunks, into this buffer;
     * when the decompressed data size is given, decompression is faster because
     * it's done in one pass, with less memcpy() overhead */
    buf = Jim_Alloc((int)bufsiz);

    out = Jim_NewEmptyStringObj(interp);
    Jim_IncrRefCount(out);

    strm.next_in = (Bytef*)in;
    strm.avail_in = (uInt)len;
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
                if (strm.msg != NULL)
                    Jim_SetResultString(interp, strm.msg, -1);
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

static int Jim_Inflate(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long bufsiz = DEF_DECOMPRESS_BUFSIZ;
    const char *in;
    int len;

    if (argc != 1) {
        if (Jim_GetLong(interp, argv[1], &bufsiz) != JIM_OK) {
            return JIM_ERR;
        }

        if (JimZlibCheckBufSize(interp, bufsiz)) {
            return JIM_ERR;
        }
    }

    in = Jim_GetString(argv[0], &len);
    return Jim_Decompress(interp, in, len, bufsiz, -MAX_WBITS);
}

static int Jim_Gunzip(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long bufsiz = DEF_DECOMPRESS_BUFSIZ;
    const char *in;
    int len;

    if (argc == 3) {
        if (!Jim_CompareStringImmediate(interp, argv[1], "-buffersize")) {
            return -1;
        }

        if (Jim_GetLong(interp, argv[2], &bufsiz) != JIM_OK) {
            return -1;
        }

    }
    else if (argc != 1) {
        return -1;
    }

    in = Jim_GetString(argv[0], &len);
    return Jim_Decompress(interp, in, len, bufsiz, WBITS_GZIP);
}

static const jim_subcmd_type zlib_command_table[] = {
    {   "crc32",
        "data ?startValue?",
        Jim_Crc32,
        1,
        2,
        /* Description: Calculates the CRC32 checksum of a string */
    },
    {   "deflate",
        "string ?level?",
        Jim_Deflate,
        1,
        2,
        /* Description: Compresses a string and outputs a raw, zlib-compressed stream */
    },
    {   "gzip",
        "data ?-level level?",
        Jim_Gzip,
        1,
        3,
        /* Description: Compresses a string and outputs a gzip-compressed stream */
    },
    {   "inflate",
        "data ?bufferSize?",
        Jim_Inflate,
        1,
        2,
        /* Description: Decompresses a raw, zlib-compressed stream */
    },
    {   "gunzip",
        "data ?-buffersize size?",
        Jim_Gunzip,
        1,
        3,
        /* Description: Decompresses a gzip-compressed stream */
    },
    { NULL }
};

static int JimZlibCmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, zlib_command_table, argc, argv), argc, argv);
}

int Jim_zlibInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "zlib", "1.0", JIM_ERRMSG)) {
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "zlib", JimZlibCmd, 0, 0);

    return JIM_OK;
}
