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
 * Note that by default no encoding is actually done since Jim supports strings containing nulls!
 *
 * Alternatively, if -hex is specified, the data is read and written as ascii hex
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

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

    /* Read one char at a time */
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
            buf[count++] = ch;
        }

        if (count >= sizeof(buf)) {
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

    /* Add anything still pending */
    Jim_AppendString(interp, result, buf, count);

    if (Jim_SetVariable(interp, argv[1], result) != JIM_OK) {
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, total);

    return JIM_OK;
}

static int bio_cmd_copy(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    long count = 0;
    long maxlen = LONG_MAX;

    FILE *infh = Jim_AioFilehandle(interp, argv[0]);
    FILE *outfh = Jim_AioFilehandle(interp, argv[1]);
    
    if (infh == NULL || outfh == NULL) {
        return JIM_ERR;
    }

    if (argc == 3) {
        if (Jim_GetLong(interp, argv[2], &maxlen) != JIM_OK) {
            return JIM_ERR;
        }
    }

    while (count < maxlen) {
        int ch = fgetc(infh);
        if (ch == EOF || fputc(ch, outfh) == EOF) {
            break;
        }
        count++;
    }

    if (ferror(infh)) {
        Jim_SetResultString(interp, "", 0);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "error reading \"", Jim_GetString(argv[0], NULL), "\": ", strerror(errno), NULL);
        clearerr(infh);
        return JIM_ERR;
    }

    if (ferror(outfh)) {
        Jim_SetResultString(interp, "", 0);
        Jim_AppendStrings(interp, Jim_GetResult(interp), "error writing \"", Jim_GetString(argv[1], NULL), "\": ", strerror(errno), NULL);
        clearerr(outfh);
        return JIM_ERR;
    }

    Jim_SetResultInt(interp, count);

    return JIM_OK;
}

static int bio_cmd_write(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    FILE *fh;
    const char *pt;
    int hex = 0;
    long total = 0;
    int len;


    if (Jim_CompareStringImmediate(interp, argv[0], "-hex")) {
        hex++;
        argv++;
        argc--;
    }
    else if (argc == 3) {
        return -1;
    }

    fh = Jim_AioFilehandle(interp, argv[0]);
    if (fh == NULL) {
        return JIM_ERR;
    }

    pt = Jim_GetString(argv[1], &len);
    while (len-- > 0) {
        int ch;

        if (hex) {
            ch = hex2char(pt);
            pt += 2;
            len--;
        }
        else {
            ch = *pt++;
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
    {   .cmd = "copy",
        .function = bio_cmd_copy,
        .args = "fromfd tofd ?bytes?",
        .minargs = 2,
        .maxargs = 3,
        .description = "Read from one fd and write to another"
    },
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
