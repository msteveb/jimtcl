#ifndef JIM_SIGNAL_H
#define JIM_SIGNAL_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the canonical name for the given signal,
 * e.g. "SIGTERM", "SIGINT"
 */
const char *Jim_SignalId(int sig);

#ifdef __cplusplus
}
#endif

#endif
