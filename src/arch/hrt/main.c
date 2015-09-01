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
#define __NAUTILUS_MAIN__

#include <nautilus/mb_utils.h>
#include <arch/hrt/init.h>


/*
 * 2nd two parameters are 
 * only used if this is an HRT 
 */
void 
main (unsigned long mbd, 
      unsigned long magic, 
      unsigned long mycpuid, 
      unsigned long apicid)

{
    if (mb_is_hrt_environ(mbd)) {

        if (mycpuid == 0) {
            hrt_bsp_init(mbd, magic, mycpuid);
        } else {
            hrt_ap_init(mbd, mycpuid);
        }

    } else {
        /* fall back to normal init */
        default_init(mbd, magic, mycpuid);
    }
}
