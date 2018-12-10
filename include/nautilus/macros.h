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
#ifndef __MACROS_H__
#define __MACROS_H__


/*
 * Round x up to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_up(x,y) (((x) + (y) - 1) & ~((y)-1))

/*
 * Round x down to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_down(x,y) ((x) & ~((y)-1))

#define ALIGN(x,a) (((x)+(a)-1)&~((a)-1))


#endif
