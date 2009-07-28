/*
 * jim-bio.c
 *
 * Implements the bio command for binary I/O
 *
 * bio read ?-hex? fd var length
 *
 *   Reads 'length' bytes of binary data from 'fd' and stores an encoded string
 *   in 'var' so that the string contains no internal null characters.
 *
 *   Returns the number of (original) chars read, which may be short if EOF is reached.
 *
 * bio write ?-hex? fd encoded
 *
 *   Writes the binary-encoded string 'encoded' to 'fd'
 *
 *   Returns the number of chars written (if no error)
 *
 * For the default binary encoding:
 * - 0x00 -> 0x01, 0x30
 * - 0x01 -> 0x01, 0x31
 *
 * Alternatively, if -hex is specified, the data is read and written as ascii hex
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <jim.h>
#include <jim-subcmd.h>

static int hex2char(const char *hex)
{
    int value;
    value = (hex[0] >= 'A' ? ((hex[0] & 0xdf) - 'A') + 10 : (hex[0] - '0'));
    value *= 16;
    value += (hex[1] >= 'A' ? ((hex[1] & 0xdf) - 'A') + 10 : (hex[1] - '0'));
    return value;
}

static void char2hex(int c, char hex[2])
{
    hex[1] = (c & 0x0F) >= 0x0A ? (c & 0x0F) + 'A' - 10 : (c & 0x0F) + '0';
    c /= 16;
    hex[0] = (c & 0x0F) >= 0x0A ? (c & 0x0F) + 'A' - 10 : (c & 0x0F) + '0';
}

/**
 * Modelled on Jim_ReadCmd
 *
 */
static int bio_cmd_read(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char buf[130];
    int count = 0;
    long len;
    int hex = 0;
    int total = 0;
    FILE *fh;

    if (Jim_CompareStringImmediate(interp, argv[0], "-hex")) {
        hex++;
        argv++;
        argc--;
    }
    else if (argc == 4) {
        return -1;
    }

    fh = Jim_AioFilehandle(interp, argv[0]);
    
    if (!fh) {
        return JIM_ERR;
    }

    if (Jim_GetLong(interp, argv[2], &len) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_Obj *result = Jim_NewStringObj(interp, "", 0);

    /* Read one char at a time, escaping 0x00 and 0xFF as necessary */
    while (len > 0) {
        int ch = fgetc(fh);
        if (ch == EOF) {
            break;
        }
        total++;

        if (hex) {
            char2hex(ch, buf + count);
            count += 2;
        }
        else {
            if (ch == 0 || ch == 1) {
                buf[count++] = 1;
                ch = ch + '0';
            }
            buf[count++] = ch;
        }

        /* Allow space for the null termination, plus escaping of one char */
        if (count >= sizeof(buf) - 2) {
            /* Buffer is full, so append it */
            Jim_AppendString(interp, result, buf, count);

            count = 0;
        }
        len--;
    }

    if (ferror(fh)) {
        Jim_SetResultString(interp, "error during read", -1);
        clearerr(fh);
        return JIM_ERR;
    }

    Jim_AppendString(interp, result, buf, count);

    if (Jim_SetVariable(interp, argv[1], result) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, total);

    return JIM_OK;
}

#if 0
static int bio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    OpenFile *infilePtr;
    OpenFile *outfilePtr;
    int count = 0;
    int maxlen = 0;

    if (TclGetOpenFile(interp, argv[0], &infilePtr) != JIM_OK) {
        return TCL_ERROR;
    }
    if (!infilePtr->readable) {
        Jim_AppendResult(interp, "\"", argv[0],
                "\" wasn't opened for reading", (char *) NULL);
        return TCL_ERROR;
    }
    if (TclGetOpenFile(interp, argv[1], &outfilePtr) != JIM_OK) {
        return TCL_ERROR;
    }
    if (!outfilePtr->writable) {
        Jim_AppendResult(interp, "\"", argv[1],
                "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }

    if (argc == 3) {
        if (Jim_GetInt(interp, argv[2], &maxlen) != JIM_OK) {
            return TCL_ERROR;
        }
    }

    while (maxlen == 0 || count < maxlen) {
        int ch = fgetc(infilePtr->f);
        if (ch == EOF || fputc(ch, outfilePtr->f) == EOF) {
            break;
        }
        count++;
    }

    if (ferror(infilePtr->f)) {
        Jim_AppendResult(interp, "error reading \"", argv[0], "\": ", Jim_UnixError(interp), 0);
        clearerr(infilePtr->f);
        return TCL_ERROR;
    }

    if (ferror(outfilePtr->f)) {
        Jim_AppendResult(interp, "error writing \"", argv[0], "\": ", Jim_UnixError(interp), 0);
        clearerr(outfilePtr->f);
        return TCL_ERROR;
    }

    Jim_SetIntResult(interp, count);

    return JIM_OK;
}
#endif

static int bio_cmd_write(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    FILE *fh;
    const char *pt;
    int hex = 0;
    long total = 0;


    if (Jim_CompareStringImmediate(interp, argv[0], "-hex")) {
        hex++;
        argv++;
        argc--;
    }
    else if (argc == 3) {
        return -1;
    }

    fh = Jim_AioFilehandle(interp, argv[0]);

    for (pt = Jim_GetString(argv[1], NULL); *pt; pt++) {
        int ch;

        if (hex) {
            ch = hex2char(pt);
            pt++;
        }
        else {
            ch = *pt;
            if (ch == 1) {
                pt++;
                ch = *pt - '0';
            }
        }
        if (fputc(ch, fh) == EOF) {
            break;
        }
        total++;
    }

    if (ferror(fh)) {
        Jim_SetResultString(interp, "error during write", -1);
        clearerr(fh);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, total);

    return JIM_OK;
}

static const jim_subcmd_type command_table[] = {
    {   .cmd = "read",
        .function = bio_cmd_read,
        .args = "?-hex? fd var numbytes",
        .minargs = 3,
        .maxargs = 4,
        .description = "Read binary bytes as an encoded string"
    },
    {   .cmd = "write",
        .function = bio_cmd_write,
        .args = "?-hex? fd buf",
        .minargs = 2,
        .maxargs = 3,
        .description = "Write an encoded string as binary bytes"
    },
#if 0
    {   .cmd = "copy",
        .function = bio_cmd_copy,
        .args = "fromfd tofd ?bytes?",
        .minargs = 2,
        .maxargs = 3,
        .description = "Read from one fd and write to another"
    },
#endif
    { 0 }
};

int
Jim_bioInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "bio", "1.0", JIM_ERRMSG) != JIM_OK) {
        return JIM_ERR;
    }
    Jim_CreateCommand(interp, "bio", Jim_SubCmdProc, (void *)command_table, NULL);
    return JIM_OK;
}
