#include <jim.h>
#include <jim-subcmd.h>

static int loadtest_cmd_test(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    Jim_SetResult(interp, argv[0]);
    return JIM_OK;
}

static const jim_subcmd_type loadtest_command_table[] = {
    {   "test",
        "arg",
        loadtest_cmd_test,
        1,
        1,
    },
    { NULL }
};

#ifndef NO_ENTRYPOINT
int Jim_loadtestInit(Jim_Interp *interp)
{
    Jim_PackageProvideCheck(interp, "loadtest");
    Jim_RegisterSubCmd(interp, "loadtest", loadtest_command_table, NULL);

    return JIM_OK;
}
#endif
