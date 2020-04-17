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

static int loadtest_cmd(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    return Jim_CallSubCmd(interp, Jim_ParseSubCmd(interp, loadtest_command_table, argc, argv), argc, argv);
}

#ifndef NO_ENTRYPOINT
int Jim_loadtestInit(Jim_Interp *interp)
{
    if (Jim_PackageProvide(interp, "loadtest", "1.0", JIM_ERRMSG)) {
        return JIM_ERR;
    }

    Jim_CreateCommand(interp, "loadtest", loadtest_cmd, 0, 0);

    return JIM_OK;
}
#endif
