// Converted from Edinburgh OpenMP benchmark main code
// to run as kernel code

#include <rt/omp/omp.h>

int schedbench_main(int argc, char **argv);
int arraybench_main(int argc, char **argv);
int taskbench_main(int argc, char **argv);
int syncbench_main(int argc, char **argv);


int test_ompbench()
{
    char *args[2];

    nk_omp_thread_init();

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

    nk_omp_thread_deinit();

    return 0;
}

