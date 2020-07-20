// These functions don't need to do anything for the libomp.a version

int nk_openmp_init()
{
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
    return 0;
}


