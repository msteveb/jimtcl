#include "jim.h"

#define LOAD_EXT(n) \
    extern int Jim_ ## n ## Init(Jim_Interp *); \
    Jim_ ## n ## Init(interp); \

int Jim_InitStaticExtensions(Jim_Interp *interp)
{
#ifdef jim_ext_stdlib
    LOAD_EXT(stdlib);
#endif
#ifdef jim_ext_package
    LOAD_EXT(package);
#endif
#ifdef jim_ext_load
    LOAD_EXT(load);
#endif
#ifdef jim_ext_aio
    LOAD_EXT(aio);
#endif
#ifdef jim_ext_readdir
    LOAD_EXT(readdir);
#endif
#ifdef jim_ext_regexp
    LOAD_EXT(regexp);
#endif
#ifdef jim_ext_eventloop
    LOAD_EXT(eventloop);
#endif
#ifdef jim_ext_file
    LOAD_EXT(file);
#endif
#ifdef jim_ext_exec
    LOAD_EXT(exec);
#endif
#ifdef jim_ext_clock
    LOAD_EXT(clock);
#endif
#ifdef jim_ext_glob
    LOAD_EXT(glob);
#endif
#ifdef jim_ext_array
    LOAD_EXT(array);
#endif
#ifdef jim_ext_posix
    LOAD_EXT(posix);
#endif
#ifdef jim_ext_signal
    LOAD_EXT(signal);
#endif
#ifdef jim_ext_tclcompat
    LOAD_EXT(tclcompat);
#endif
#ifdef jim_ext_syslog
    LOAD_EXT(syslog);
#endif
    return JIM_OK;
}
