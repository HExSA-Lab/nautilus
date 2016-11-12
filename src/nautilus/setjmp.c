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
 * Copyright (c) 2016, Kyle C. Hale <khale@cs.iit.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/setjmp.h>

static void
testit (jmp_buf  env,
        int      prev_res)
{
	int res = (!prev_res) ? prev_res : prev_res + 1;

	printk("Long jumping with result %d\n", res);
	longjmp(env, res);
	panic("SHOULD NOT GET HERE!\n");
}       


int
test_setlong (void)
{
  jmp_buf  env;

  int res = setjmp(env);

  printk("res = 0x%08x\n", res);

  if (res > 1) {
	  return  0;
  }

  testit(env, res);

  return 0;
}       
