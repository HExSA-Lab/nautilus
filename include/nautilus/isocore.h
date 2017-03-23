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
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 *                     Adel Lahlou <AdelLahlou2017@u.northwestern.edu>
 *                     Fahad Khan <FahadKhan2017@u.northwestern.edu>
 *                     David Samuels <davidSamuels2018@u.northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *	    Adel Lahlou <AdelLahlou2017@u.northwestern.edu>
 *          Fahad Khan <FahadKhan2017@u.northwestern.edu>
 *          David Samuels <davidSamuels2018@u.northwestern.edu>
 *	
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef _ISOCORE
#define _ISOCORE

/*
  Convert this core into an isolated core
  Execution begins in a *copy* of the code
  with a distinct stack.

  Unless there is an error in startup, the
  nk_isolate() function will never return. 
  Also, the core will never again take
  another interrupt.

  codesize+stacksize must be smaller than
  the cache in which will be isolated

  The code must be position-independent, since
  we will relocate it.   If the code touches
  any global variable or the heap, isolation
  bets are off.

*/

int nk_isolate(void (*code)(void *arg),
	       uint64_t codesize,
	       uint64_t stacksize,
	       void     *arg);

#endif
