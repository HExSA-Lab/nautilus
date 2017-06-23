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
#ifndef __BUG_H__
#define __BUG_H__

#include <nautilus/intrinsics.h>
#include <nautilus/printk.h>

#ifndef BUG
#define BUG() do { \
	printk("BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
	panic("BUG!"); \
} while (0)
#endif

#ifndef BUG_ON
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif

#ifndef ASSERT
#define ASSERT(condition) do { if (unlikely((condition)!=1)) BUG(); } while(0)
#endif

#ifndef __ASSEMBLER__
extern void
warn_slowpath(const char * file, int line, const char * fmt, ...);
#endif

#ifndef WARN
#define WARN(condition, format...) ({				\
        int __ret_warn_on = !!(condition);			\
        if (unlikely(__ret_warn_on))				\
                warn_slowpath(__FILE__, __LINE__, format);	\
        unlikely(__ret_warn_on);				\
})
#endif

#ifndef WARN_ON
#define WARN_ON(condition) ({					\
	int __ret_warn_on = !!(condition);			\
	if (unlikely(__ret_warn_on))				\
		warn_slowpath(__FILE__, __LINE__, "WARN_ON");	\
	unlikely(__ret_warn_on);				\
})
#endif

#ifndef WARN_ON_ONCE
#define WARN_ON_ONCE(condition)	({				\
	static int __warned;					\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once))				\
		if (WARN_ON(!__warned)) 			\
			__warned = 1;				\
	unlikely(__ret_warn_once);				\
})
#endif

#ifndef WARN_ONCE
#define WARN_ONCE(condition, format...)	({			\
	static int __warned;					\
	int __ret_warn_once = !!(condition);			\
								\
	if (unlikely(__ret_warn_once))				\
		if (WARN(!__warned, format)) 			\
			__warned = 1;				\
	unlikely(__ret_warn_once);				\
})
#endif

#endif
