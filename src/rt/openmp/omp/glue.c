// These functions don't need to do anything for the libomp.a version
#include <nautilus/nautilus.h>
#include <nautilus/env.h>
extern int pthread_init(void);
int nk_openmp_init()
{
    nk_env_create(NK_ENV_GLOBAL_NAME,
		   NK_ENV_GLOBAL_MAX_NUM_KEYS,
		   NK_ENV_GLOBAL_MAX_KEY_SIZE,
		   NK_ENV_GLOBAL_MAX_VAL_SIZE);
    pthread_init();
    return 0;
}
    
void nk_openmp_deinit()
{
    return;
}

int nk_openmp_thread_init()
{
    return 0;
}

int nk_openmp_thread_deinit()
{
    return 0;
}

char **nk_environ(void)
{   
    char **dummy_space = malloc(sizeof(char*));
    memset(dummy_space,0,sizeof(char*));
    return dummy_space;

    //return null will fail at openmplib
    //return 0;
}


