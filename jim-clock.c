/*
 * jim-clock.c
 *
 * Implements the clock command
 */

/* For strptime() */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "jimautoconf.h"
#include <jim-subcmd.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

static int clock_cmd_format(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* How big is big enough? */
    char buf[100];
    time_t t;
    long seconds;
    const char *format = "%a %b %d %H:%M:%S %Z %Y";
    size_t len = 0;
    int option;
    int i;

    static const char * const options[] = {
        "-format", "-timezone", NULL
    };
    enum {
        OPT_FORMAT, OPT_TIMEZONE
    };

    if (!(argc % 2)) {
        return -1;
    }

    for (i = 1; i < argc; i += 2) {
        if (Jim_GetEnum(interp, argv[i], options, &option, NULL, JIM_ERRMSG)
                != JIM_OK) {
            return JIM_ERR;
        }

        const char *opt = Jim_String(argv[i + 1]);
        len = (size_t)Jim_Length(argv[i + 1]);

        switch (option) {
        case OPT_FORMAT:
            if (len) {
                format = opt;
            }
            break;
        case OPT_TIMEZONE:
            if (len) {
                setenv("TZ", opt, 1);
            }
            break;
        }
    }

    if (Jim_GetLong(interp, argv[0], &seconds) != JIM_OK) {
        return JIM_ERR;
    }
    t = seconds;

    if (strftime(buf, sizeof(buf), format, localtime(&t)) == 0) {
        Jim_SetResultString(interp, "format string too long", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

#ifdef HAVE_STRPTIME
static int clock_cmd_scan(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    char *pt;
    struct tm tm;
    time_t now = time(0);

    if (!Jim_CompareStringImmediate(interp, argv[1], "-format")) {
        return -1;
    }

    /* Initialise with the current date/time */
    localtime_r(&now, &tm);

    pt = strptime(Jim_String(argv[0]), Jim_String(argv[2]), &tm);
    if (pt == 0 || *pt != 0) {
        Jim_SetResultString(interp, "Failed to parse time according to format", -1);
        return JIM_ERR;
    }

    /* Now convert into a time_t */
    Jim_SetResultInt(interp, mktime(&tm));

    return JIM_OK;
}
#endif

static int clock_cmd_seconds(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResultInt(interp, time(NULL));

    return JIM_OK;
}

static int clock_cmd_micros(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000000 + tv.tv_usec);

    return JIM_OK;
}

static int clock_cmd_millis(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    Jim_SetResultInt(interp, (jim_wide) tv.tv_sec * 1000 + tv.tv_usec / 1000);

    return JIM_OK;
}

static const jim_subcmd_type clock_command_table[] = {
    {   "seconds",
        NULL,
        clock_cmd_seconds,
        0,
        0,
        /* Description: Returns the current time as seconds since the epoch */
    },
    {   "clicks",
        NULL,
        clock_cmd_micros,
        0,
        0,
        /* Description: Returns the current time in 'clicks' */
    },
    {   "microseconds",
        NULL,
        clock_cmd_micros,
        0,
        0,
        /* Description: Returns the current time in microseconds */
    },
    {   "milliseconds",
        NULL,
        clock_cmd_millis,
        0,
        0,
        /* Description: Returns the current time in milliseconds */
    },
    {   "format",
        "seconds ?-format format? ?-timezone zoneName?",
        clock_cmd_format,
        1,
        5,
        /* Description: Format the given time */
    },
#ifdef HAVE_STRPTIME
    {   "scan",
        "str -format format",
        clock_cmd_scan,
        3,
        3,
        /* Description: Determine the time according to the given format */
    },
#endif
    { NULL }
};

int Jim_clockInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "clock", "1.0", JIM_ERRMSG))
        return JIM_ERR;

    Jim_CreateCommand(interp, "clock", Jim_SubCmdProc, (void *)clock_command_table, NULL);
    return JIM_OK;
}
