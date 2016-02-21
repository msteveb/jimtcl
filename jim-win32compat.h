#ifndef JIM_WIN32COMPAT_H
#define JIM_WIN32COMPAT_H

/* Compatibility for Windows (mingw and msvc, not cygwin */

#ifdef __cplusplus
extern "C" {
#endif

/* Note that at this point we don't yet have access to jimautoconf.h */
#if defined(_WIN32) || defined(WIN32)

#define HAVE_DLOPEN
void *dlopen(const char *path, int mode);
int dlclose(void *handle);
void *dlsym(void *handle, const char *symbol);
char *dlerror(void);

/* MinGW MS CRT always uses three digits after 'e' */
#if defined(__MINGW32__)
    #define JIM_SPRINTF_DOUBLE_NEEDS_FIX
#endif

/* MinGW does not have group/owner permissions */
#ifndef S_IRWXG
#define S_IRWXG 0
#endif
#ifndef S_IRWXO
#define S_IRWXO 0
#endif

#ifdef _MSC_VER
/* These are msvc vs gcc */

#if _MSC_VER >= 1000
	#pragma warning(disable:4146)
#endif

#include <limits.h>
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
#define strcasecmp _stricmp
#define strtoull _strtoui64
#define snprintf _snprintf

#include <io.h>

#ifdef _MSC_VER
#include <winsock2.h>
#include <sys/types.h>
#include <direct.h>

#define STDIN_FILENO 0
#define STDIN_LINENO 0

#define strncasecmp _strnicmp
#define ftello      ftell
#define fseeko      fseek

#define isatty(x)   _isatty(x)
#define strdup(x)   _strdup(x)
#define fileno(x)   _fileno(x)
#define close(x)    _close(x)
#define unlink(x)   _unlink(x)
#define rmdir(x)    _rmdir(x)
#define mkdir(x)    _mkdir(x)
#define chdir(x)    _chdir(x)
#define getcwd(x,y) _getcwd(x,y)
#define access(x,y) _access(x,y)

#else

struct timeval {
	long tv_sec;
	long tv_usec;
};

#endif 

int gettimeofday(struct timeval *tv, void *unused);

#define HAVE_OPENDIR
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

#elif defined(__MINGW32__)

#include <stdlib.h>
#define strtod __strtod

#endif

#endif /* WIN32 */

#ifdef __cplusplus
}
#endif

#endif
