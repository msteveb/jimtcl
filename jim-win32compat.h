#ifndef JIM_WIN32COMPAT_H
#define JIM_WIN32COMPAT_H

#ifndef WIN32
	#define WIN32 1
#endif
#ifndef STRICT
	#define STRICT
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if _MSC_VER >= 1000
	#pragma warning(disable:4146)
#endif

#undef JIM_DYNLIB

#define strcasecmp _stricmp

#define jim_wide _int64
#ifndef LLONG_MAX
	#define LLONG_MAX    9223372036854775807I64
#endif
#ifndef LLONG_MIN
	#define LLONG_MIN    (-LLONG_MAX - 1I64)
#endif
#define JIM_WIDE_MIN LLONG_MIN
#define JIM_WIDE_MAX LLONG_MAX
#define JIM_WIDE_MODIFIER "I64d"

#include <io.h>

struct timeval {
	long tv_sec;
	long tv_usec;
};

int gettimeofday(struct timeval *tv, void *unused);

struct dirent {
	char *d_name;
};

typedef struct DIR {
	long                handle; /* -1 for failed rewind */
	struct _finddata_t  info;
	struct dirent       result; /* d_name null iff first time */
	char                *name;  /* null-terminated char string */
} DIR;

DIR *opendir(const char *name);
int closedir(DIR *dir);
struct dirent *readdir(DIR *dir);

#endif
