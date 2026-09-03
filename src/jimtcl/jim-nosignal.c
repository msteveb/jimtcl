#include <stdio.h>
#include <signal.h>

#include <jim-signal.h>
#include <jim.h>

/* Implement trivial Jim_SignalId() just good enough for JimMakeErrorCode() in [exec] */


/* This works for mingw, but is not really portable */
#ifndef SIGPIPE
#define SIGPIPE 13
#endif
#ifndef SIGINT
#define SIGINT 2
#endif

const char *Jim_SignalId(int sig)
{
	static char buf[10];
	switch (sig) {
		case SIGINT: return "SIGINT";
		case SIGPIPE: return "SIGPIPE";

	}
	snprintf(buf, sizeof(buf), "%d", sig);
	return buf;
}
