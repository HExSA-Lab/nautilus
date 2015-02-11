#include <nautilus/nautilus.h>
#include <nautilus/thread.h>

void run_legion_tests(void);

/* TODO: this won't work if legion doesn't shut down properly... */
//extern void go_circuit_c(int argc, char **argv);
extern void go_hello_c(int argc, char ** argv);
extern void go_taf_c(int argc, char ** argv);
extern void go_index_task_c(int argc, char ** argv);
extern void go_global_vars_c(int argc, char ** argv);
extern void go_log_regs_c(int argc, char ** argv);
extern void go_phys_regs_c(int argc, char ** argv);
extern void go_privileges_c(int argc, char **argv);
extern void go_partit_c(int argc, char **argv);
extern void go_multi_c(int argc, char **argv);
extern void go_custom_c(int argc, char **argv);
extern void go_circuit_c(int argc, char ** argv);
extern void go_delaunay_c(int argc, char ** argv);
extern void go_saxpy_c(int argc, char ** argv);

typedef void (*tfun_t)(int, char**);

#define NUM_TESTS 1

tfun_t tests[NUM_TESTS] = {
    //go_hello_c,
    //go_taf_c,
    //go_index_task_c,
    //go_global_vars_c,
    //go_log_regs_c,
    //go_phys_regs_c,
    //go_privileges_c,
    //go_multi_c,
    //go_custom_c,
    go_circuit_c,
    //go_partit_c,
    //go_delaunay_c,
    //go_saxpy_c,
};

static void 
legion_test_thread_func (void * in, void ** out)
{
    tfun_t fun = (tfun_t)in;
    char * argv[] = {"test", 0};

    if (fun) {
        fun(1, argv);
    }
    
    printk("legion test finished\n");
}



void 
run_legion_tests (void) 
{
    char * blah[] = {"test", 0};
    nk_thread_id_t t;
    unsigned i;

    /* I will now pull some
     * devious NUMA hackery out of my...
     */

    


    for (i = 0; i < NUM_TESTS; i++) {
        printk("starting legion test %u\n", i);
        nk_thread_start(legion_test_thread_func,
                (void*)go_circuit_c,
                NULL,
                0,
                TSTACK_2MB,
                &t,
                1);
                //nk_get_cpu_by_lapicid(96));

        nk_join(t, NULL);
    }
}

