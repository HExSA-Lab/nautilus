// Converted from Edinburgh OpenMP benchmark main code
// to run as kernel code

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <rt/openmp/openmp.h>

int schedbench_main(int argc, char **argv);
int arraybench_main(int argc, char **argv);
int taskbench_main(int argc, char **argv);
int syncbench_main(int argc, char **argv);


int test_ompbench()
{
    char *args[2];

    nk_openmp_thread_init();

    args[1] = 0;

#if 0
    args[0]="arraybench";
    arraybench_main(1, args);
#endif

    args[0]="taskbench";
    taskbench_main(1, args);


#if 0
    args[0]="schedbench";
    schedbench_main(1, args);
    args[0]="syncbench";
    syncbench_main(1, args);
#endif

    nk_openmp_thread_deinit();

    return 0;
}

static int
handle_ompb (char * buf, void * priv)
{
    test_ompbench();
    return 0;
}

static struct shell_cmd_impl ompb_impl = {
    .cmd      = "ompb",
    .help_str = "ompb",
    .handler  = handle_ompb,
};
nk_register_shell_cmd(ompb_impl);

