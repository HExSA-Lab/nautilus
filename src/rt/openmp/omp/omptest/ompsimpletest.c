#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/libccompat.h>

#define ERROR(fmt, args...) ERROR_PRINT("ompstest: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("ompstest: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ompstest: " fmt, ##args)

static int handle_test (char * buf, void * priv)
{
    int i;
    #pragma omp parallel
    {
      
      /* long id = getpid(); */
      /* nk_vc_printf("****omptest==thread id %d", id);	 */
       #pragma omp for private(i) schedule(static, 1)
        for( i=0;i<8;i++){
	 long id = getpid();
         nk_vc_printf("****omptest==thread id %d\n", id);  
         nk_vc_printf("*****working %d\n",i);
      }

    }
  return 0;
}


static struct shell_cmd_impl omptest_impl = {
    .cmd      = "ompstest",
    .help_str = "ompstest (openmp simple test)",
    .handler  = handle_test,
};
nk_register_shell_cmd(omptest_impl);

