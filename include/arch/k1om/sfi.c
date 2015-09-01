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
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/sfi.h>
#include <arch/k1om/k1omsfi.h>

int
__early_init_sfi (struct naut_info * naut)
{
    struct sfi_sys_tbl * sfi = sfi_find_syst();
    if (!sfi) {
        panic("Could not find  SFI table!\n");
        return -1;
    }

    if (sfi_parse_syst(&naut->sys, sfi) != 0) {
        ERROR_PRINT("Problem parsing SFI SYST\n");
        return -1;
    }

    return 0;
}
