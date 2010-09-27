
/*
 * tcl_clock.c
 *
 * Implements the clock command
 */

/* For strptime() */
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "jim.h"
#include "jim-subcmd.h"

static int clock_cmd_format(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* How big is big enough? */
    char buf[100];
    time_t t;
    long seconds;

    const char *format = "%a %b  %d %H:%M:%S %Z %Y";

    if (argc == 2 || (argc == 3 && !Jim_CompareStringImmediate(interp, argv[1], "-format"))) {
        return -1;
    }

    if (argc == 3) {
        format = Jim_GetString(argv[2], NULL);
    }

    if (Jim_GetLong(interp, argv[0], &seconds) != JIM_OK) {
        return JIM_ERR;
    }
    t = seconds;

    strftime(buf, sizeof(buf), format, localtime(&t));

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
    tm = *localtime(&now);

    pt = strptime(Jim_GetString(argv[0], NULL), Jim_GetString(argv[2], NULL), &tm);
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

static const jim_subcmd_type clock_command_table[] = {
    {   .cmd = "seconds",
        .function = clock_cmd_seconds,
        .minargs = 0,
        .maxargs = 0,
        .description = "Returns the current time as seconds since the epoch"
    },
    {   .cmd = "format",
        .args = "seconds ?-format format?",
        .function = clock_cmd_format,
        .minargs = 1,
        .maxargs = 3,
        .description = "Format the given time"
    },
#ifdef HAVE_STRPTIME
    {   .cmd = "scan",
        .args = "str -format format",
        .function = clock_cmd_scan,
        .minargs = 3,
        .maxargs = 3,
        .description = "Determine the time according to the given format"
    },
#endif
    { 0 }
};

int Jim_clockInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "clock", Jim_SubCmdProc, (void *)clock_command_table, NULL);
    return JIM_OK;
}
