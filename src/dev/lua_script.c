/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2018, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Goutham Kannan
 *          Imran Usmani
 *          Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/naut_string.h>
#include <nautilus/libccompat.h>

/*
 * User must free the script when done
 */
char * 
read_lua_script (void)
{
    extern int __LUA_SCRIPT_START, __LUA_SCRIPT_END;
    char * data = (char*)&__LUA_SCRIPT_START;
    uint64_t file_len;

    file_len = (((uint64_t)(&__LUA_SCRIPT_END)) - ((uint64_t)(&__LUA_SCRIPT_START)));
    
    char * buf = (char *)malloc(file_len);
    if (!buf) {
        ERROR_PRINT("Could not allocate LUA script buffer\n");
        return NULL;
    }
    memset(buf, 0, file_len);

    memcpy(buf, data, file_len);

    return buf;
}
