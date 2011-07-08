/*
  * hello.c -- A minimal Jim C extension.
  */
#include <jim.h>

static int
Hello_Cmd(Jim_Interp *interp, int objc, Jim_Obj *const objv[])
{
    Jim_SetResultString(interp, "Hello, World!", -1);
    return JIM_OK;
}

/*
 * Jim_helloworldInit -- Called when Jim loads your extension.
 *
 * Note that the name *must* correspond exactly to the name of the extension:
 *  Jim_<extname>Init
 */
int
Jim_helloworldInit(Jim_Interp *interp)
{
    Jim_CreateCommand(interp, "hello", Hello_Cmd, NULL, NULL);
    return JIM_OK;
}
