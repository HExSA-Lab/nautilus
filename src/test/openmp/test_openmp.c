#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <rt/openmp/openmp.h>

#define N 4
volatile float a[N];
volatile float b[N];
volatile float c[N];


static int omp_simple()
{
	int i;

	for (i=0;i<N;i++) {
	    a[i] = i;
	    b[i] = i;
	}

#pragma omp parallel
	nk_vc_printf("I am thread %d (%d total)\n",omp_get_thread_num(),omp_get_num_threads());
	

#pragma omp parallel for
	for (i=0;i<N;i++) {
		c[i] = a[i] * b[i];
	}

	for (i=0;i<N;i++) {
	    nk_vc_printf("a[%d]=%d b[%d]=%d c[%d]=%d\n",i,(int)a[i],i,(int)b[i],i,(int)c[i]);
	}
	
	return 0;
}


static void report_num_threads(int level)
{
#pragma omp single
    {
    nk_vc_printf("Level %d: number of threads in the team - %d\n",
	level, omp_get_num_threads());
    }
}

static int 
omp_nested (void)
{
    omp_set_dynamic(0);
    #pragma omp parallel num_threads(2)
    {
        report_num_threads(1);
        #pragma omp parallel num_threads(2)
        {
            report_num_threads(2);
            #pragma omp parallel num_threads(2)
            {
                report_num_threads(3);
            }
        }
    }
    return(0);
}


int 
test_omp (void)
{
    nk_openmp_thread_init();
    nk_vc_printf("Starting simple test\n");
    omp_simple();
    //     goto out;
    nk_vc_printf("Starting nested test\n");
    omp_nested();
//out:
    nk_vc_printf("OMP test finished\n");
    nk_openmp_thread_deinit();
    return 0;
}


static int
handle_omp (char * buf, void * priv)
{
    test_omp();
    return 0;
}


static struct shell_cmd_impl omp_impl = {
    .cmd      = "omp",
    .help_str = "omp",
    .handler  = handle_omp,
};
nk_register_shell_cmd(omp_impl);

