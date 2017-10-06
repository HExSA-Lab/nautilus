//
// Copyright (c) 2017 Peter Dinda  All Rights Reserved
//

#include <nautilus/nautilus.h>
#include <rt/ndpc/ndpc.h>

#ifndef NAUT_CONFIG_NDPC_RT_DEBUG
#define DEBUG(fmt, args...)
#else
#define DEBUG(fmt, args...) DEBUG_PRINT("ndpc: " fmt, ##args)
#endif

#define ERROR(fmt, args...) ERROR_PRINT("ndpc: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("ndpc: " fmt, ##args)

int nk_ndpc_init()
{
    INFO("inited\n");
    return 0;
}

void nk_ndpc_deinit()
{
    INFO("deinited\n");
}
