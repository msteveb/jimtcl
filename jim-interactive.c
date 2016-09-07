#include <errno.h>
#include <string.h>

#include "jimautoconf.h"
#include <jim.h>

#ifdef USE_LINENOISE
#ifdef HAVE_UNISTD_H
    #include <unistd.h>
    #include <sys/stat.h>
#endif
#include "linenoise.h"
#else
#define MAX_LINE_LEN 512
#endif

/**
 * Returns an allocated line, or NULL if EOF.
 */
char *Jim_HistoryGetline(const char *prompt)
{
#ifdef USE_LINENOISE
    return linenoise(prompt);
#else
    int len;
    char *line = malloc(MAX_LINE_LEN);

    fputs(prompt, stdout);
    fflush(stdout);

    if (fgets(line, MAX_LINE_LEN, stdin) == NULL) {
        free(line);
        return NULL;
    }
    len = strlen(line);
    if (len && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
    return line;
#endif
}

void Jim_HistoryLoad(const char *filename)
{
#ifdef USE_LINENOISE
    linenoiseHistoryLoad(filename);
#endif
}

void Jim_HistoryAdd(const char *line)
{
#ifdef USE_LINENOISE
    linenoiseHistoryAdd(line);
#endif
}

void Jim_HistorySave(const char *filename)
{
#ifdef USE_LINENOISE
    mode_t mask;
    /* Just u=rw, but note that this is only effective for newly created files */
    mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
    linenoiseHistorySave(filename);
    mask = umask(mask);
#endif
}

void Jim_HistoryShow(void)
{
#ifdef USE_LINENOISE
    /* built-in history command */
    int i;
    int len;
    char **history = linenoiseHistory(&len);
    for (i = 0; i < len; i++) {
        printf("%4d %s\n", i + 1, history[i]);
    }
#endif
}

int Jim_InteractivePrompt(Jim_Interp *interp)
{
    int retcode = JIM_OK;
    char *history_file = NULL;
#ifdef USE_LINENOISE
    const char *home;

    home = getenv("HOME");
    if (home && isatty(STDIN_FILENO)) {
        int history_len = strlen(home) + sizeof("/.jim_history");
        history_file = Jim_Alloc(history_len);
        snprintf(history_file, history_len, "%s/.jim_history", home);
        Jim_HistoryLoad(history_file);
    }
#endif

    printf("Welcome to Jim version %d.%d\n",
        JIM_VERSION / 100, JIM_VERSION % 100);
    Jim_SetVariableStrWithStr(interp, JIM_INTERACTIVE, "1");

    while (1) {
        Jim_Obj *scriptObjPtr;
        const char *result;
        int reslen;
        char prompt[20];

        if (retcode != JIM_OK) {
            const char *retcodestr = Jim_ReturnCode(retcode);

            if (*retcodestr == '?') {
                snprintf(prompt, sizeof(prompt) - 3, "[%d] . ", retcode);
            }
            else {
                snprintf(prompt, sizeof(prompt) - 3, "[%s] . ", retcodestr);
            }
        }
        else {
            strcpy(prompt, ". ");
        }

        scriptObjPtr = Jim_NewStringObj(interp, "", 0);
        Jim_IncrRefCount(scriptObjPtr);
        while (1) {
            char state;
            char *line;

            line = Jim_HistoryGetline(prompt);
            if (line == NULL) {
                if (errno == EINTR) {
                    continue;
                }
                Jim_DecrRefCount(interp, scriptObjPtr);
                retcode = JIM_OK;
                goto out;
            }
            if (Jim_Length(scriptObjPtr) != 0) {
                /* Line continuation */
                Jim_AppendString(interp, scriptObjPtr, "\n", 1);
            }
            Jim_AppendString(interp, scriptObjPtr, line, -1);
            free(line);
            if (Jim_ScriptIsComplete(interp, scriptObjPtr, &state))
                break;

            snprintf(prompt, sizeof(prompt), "%c> ", state);
        }
#ifdef USE_LINENOISE
        if (strcmp(Jim_String(scriptObjPtr), "h") == 0) {
            /* built-in history command */
            Jim_HistoryShow();
            Jim_DecrRefCount(interp, scriptObjPtr);
            continue;
        }

        Jim_HistoryAdd(Jim_String(scriptObjPtr));
        if (history_file) {
            Jim_HistorySave(history_file);
        }
#endif
        retcode = Jim_EvalObj(interp, scriptObjPtr);
        Jim_DecrRefCount(interp, scriptObjPtr);

        if (retcode == JIM_EXIT) {
            break;
        }
        if (retcode == JIM_ERR) {
            Jim_MakeErrorMessage(interp);
        }
        result = Jim_GetString(Jim_GetResult(interp), &reslen);
        if (reslen) {
            printf("%s\n", result);
        }
    }
  out:
    Jim_Free(history_file);
    return retcode;
}
