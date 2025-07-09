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
    /* Register the package with Jim and check that the ABI matches the interpreter */
    Jim_PackageProvideCheck(interp, "helloworld");
    Jim_RegisterSimpleCmd(interp, "hello", "", 0, 0, Hello_Cmd);
    return JIM_OK;
}
