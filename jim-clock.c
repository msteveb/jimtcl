/*
 * jim-clock.c
 *
 * Implements the clock command
 */

/* For strptime() */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500
#endif

/* for localtime_r & gmtime_r */
#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
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

#define JIM_ERR_WRONG_ARG_COUNT (3000)

typedef enum {
	CLOCK_OPT_FORMAT, CLOCK_OPT_GMT, CLOCK_OPT_LOCALE, CLOCK_OPT_TIMEZONE, CLOCK_OPT_BASE
} clock_option_id;

typedef struct {
	const char *format;
	const char *locale;
	const char *timezone;
    jim_wide base;
	int gmt;
} clock_parsed_options;

static int clock_parse_options(Jim_Interp *interp,
		                       int argc,
							   Jim_Obj *const *argv,
							   const char *const *valid_options,
							   const clock_option_id *valid_option_ids,
							   clock_parsed_options *parsed_options)
{
    int i;
    for (i = 0; i < argc; i++) {
        const char *opt = Jim_String(argv[i]);
        int option;

        if (*opt != '-') {
            Jim_SetResultFormatted(interp, "Unknown trailing data \"%#s\"", argv[i]);
            return JIM_ERR;
        }
        if (Jim_GetEnum(interp, argv[i], valid_options, &option, "option", JIM_ERRMSG) != JIM_OK) {
            return JIM_ERR;
        }
        switch (valid_option_ids[option]) {
        case CLOCK_OPT_FORMAT:
            if (++i == argc) {
            	return JIM_ERR_WRONG_ARG_COUNT;
            }
            parsed_options->format = Jim_String(argv[i]);
            break;

        case CLOCK_OPT_GMT: {
                if (++i == argc) {
                	return JIM_ERR_WRONG_ARG_COUNT;
                }
                if (Jim_GetBoolean(interp, argv[i], &parsed_options->gmt) != JIM_OK) {
                    Jim_SetResultFormatted(interp, "-gmt argument \"%#s\" is not a boolean value", argv[i]);
                    return JIM_ERR;
                }
            }
            break;

        case CLOCK_OPT_LOCALE:
            if (++i == argc) {
            	return JIM_ERR_WRONG_ARG_COUNT;
            }
            parsed_options->locale = Jim_String(argv[i]);
            break;

        case CLOCK_OPT_BASE: {
                if (++i == argc) {
                	return JIM_ERR_WRONG_ARG_COUNT;
                }
                if (Jim_GetWide(interp, argv[i], &parsed_options->base) != JIM_OK) {
                    Jim_SetResultFormatted(interp, "-base argument \"%#s\" is not a wide integer", argv[i]);
                    return JIM_ERR;
                }
                /* Currently unused */
            }
            break;

        case CLOCK_OPT_TIMEZONE:
            if (++i == argc) {
            	return JIM_ERR_WRONG_ARG_COUNT;
            }
            parsed_options->timezone = Jim_String(argv[i]);
            /* Currently unused */
            break;
        }
    }
    return JIM_OK;
}

static int clock_cmd_format(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    /* How big is big enough? */
    char buf[500];
    time_t t;
    jim_wide seconds;
    static const char * const options[] = {
        "-format", "-gmt", "-locale", "-timezone", NULL
    };
    static const clock_option_id option_ids[] = {
        CLOCK_OPT_FORMAT, CLOCK_OPT_GMT, CLOCK_OPT_LOCALE, CLOCK_OPT_TIMEZONE
    };

    clock_parsed_options parsed_options = { "%a %b %d %H:%M:%S %Z %Y", "", "", 0, 0 };

    int retval = clock_parse_options(interp, argc - 1, &argv[1], options, option_ids, &parsed_options);
    if (retval == JIM_ERR_WRONG_ARG_COUNT) {
    	Jim_SetResultString(interp,
    		"wrong # args: should be \"clock format clockval ?-format string? ?-gmt boolean? ?-locale LOCALE? ?-timezone ZONE?\"", -1);
        return JIM_ERR;
    }
    else if (retval != JIM_OK) {
    	return retval;
    }

    if (Jim_GetWide(interp, argv[0], &seconds) != JIM_OK) {
        Jim_SetResultString(interp, "Could not parse 'seconds' field into a wide integer", -1);
        return JIM_ERR;
    }
    t = seconds;

    struct tm tm_value;
    struct tm *tm_ptr;
    if (parsed_options.gmt) {
        tm_ptr = gmtime_r(&t, &tm_value);
    }
    else {
        tm_ptr = localtime_r(&t, &tm_value);
    }

    if (tm_ptr == NULL) {
        Jim_SetResultString(interp, "Error converting numeric to time", -1);
        return JIM_ERR;
    }


    /* parsed_options.locale & parsed_options.timezone currently unused */

    buf[0] = 0;
    if (parsed_options.format && strlen(parsed_options.format) != 0 && strftime(buf, sizeof(buf), parsed_options.format, &tm_value) == 0) {
        Jim_SetResultString(interp, "format string too long", -1);
        return JIM_ERR;
    }

    Jim_SetResultString(interp, buf, -1);

    return JIM_OK;
}

#ifdef HAVE_STRPTIME
static int clock_cmd_scan(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{

    /* How big is big enough? */
    const char* inputString = Jim_String(argv[0]);
    static const char * const options[] = {
        "-format", "-gmt", "-locale", "-timezone", "-base", NULL
    };
    static const clock_option_id option_ids[] = {
        CLOCK_OPT_FORMAT, CLOCK_OPT_GMT, CLOCK_OPT_LOCALE, CLOCK_OPT_TIMEZONE, CLOCK_OPT_BASE
    };

    clock_parsed_options parsed_options = { "%a %b %d %H:%M:%S %Z %Y", "", "", 0, 0 };

    int retval = clock_parse_options(interp, argc - 1, &argv[1], options, option_ids, &parsed_options);
    if (retval == JIM_ERR_WRONG_ARG_COUNT) {
    	Jim_SetResultString(interp,
    		"wrong # args: should be \"clock scan string ?-base seconds? ?-format string? ?-gmt boolean? ?-locale LOCALE? ?-timezone ZONE?\"", -1);
    	return JIM_ERR;
    }
    else if (retval != JIM_OK) {
    	return retval;
    }

    /* Currently unused:
     * parsed_options.locale,
     * parsed_options.timezone
     * parsed_options.base
     * parsed_options.gmt
     */

    char *pt;
    struct tm tm;
    time_t now = time(0);

    /* Initialise with the current date/time */
    if (parsed_options.gmt) {
    	gmtime_r(&now, &tm);
    }
    else {
    	localtime_r(&now, &tm);
    }

    pt = strptime(inputString, parsed_options.format, &tm);
    if (pt == 0 || *pt != 0) {
        Jim_SetResultString(interp, "Failed to parse time according to format", -1);
        return JIM_ERR;
    }

    /* Now convert into a time_t */
    Jim_SetResultWide(interp, mktime(&tm));

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
        "clockval ?-format string? ?-gmt boolean? ?-locale LOCALE? ?-timezone ZONE?",
        clock_cmd_format,
        1,
        -1,
        /* Description: Format the given time */
    },
#ifdef HAVE_STRPTIME
    {   "scan",
        "string ?-base seconds? ?-format string? ?-gmt boolean? ?-locale LOCALE? ?-timezone ZONE?",
        clock_cmd_scan,
        1,
        -1,
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
