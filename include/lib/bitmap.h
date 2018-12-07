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
 *
 * Many parts of this file were borrowed from Linux
 */
#ifndef __LINUX_BITMAP_H
#define __LINUX_BITMAP_H

#ifndef __ASSEMBLER__

#include <nautilus/naut_types.h>
#include <lib/bitops.h>
#include <nautilus/naut_string.h>

extern int __bitmap_empty(const unsigned long *bitmap, unsigned long bits);
extern int __bitmap_full(const unsigned long *bitmap, unsigned long bits);
extern void __bitmap_complement(unsigned long *dst, const unsigned long *src,
			unsigned long bits);
extern void __bitmap_shift_right(unsigned long *dst,
                        const unsigned long *src, int shift, unsigned long bits);
extern void __bitmap_shift_left(unsigned long *dst,
                        const unsigned long *src, int shift, unsigned long bits);

extern void bitmap_set(unsigned long *map, int i, int len);
extern void bitmap_clear(unsigned long *map, int start, int nr);
extern unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int nr,
					 unsigned long align_mask);

extern int bitmap_find_free_region(unsigned long *bitmap, unsigned long bits, int order);
extern void bitmap_release_region(unsigned long *bitmap, unsigned long pos, int order);
extern int bitmap_allocate_region(unsigned long *bitmap, unsigned long pos, int order);
extern void bitmap_copy_le(void *dst, const unsigned long *src, unsigned long nbits);

#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))
#define BITMAP_LAST_WORD_MASK(nbits)					\
(									\
	((nbits) % BITS_PER_LONG) ?					\
		(1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL		\
)

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

static inline void bitmap_zero(unsigned long *dst, unsigned long nbits)
{
	if (small_const_nbits(nbits))
		*dst = 0UL;
	else {
		unsigned long len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memset(dst, 0, len);
	}
}

static inline void bitmap_fill(unsigned long *dst, unsigned long nbits)
{
	unsigned long nlongs = BITS_TO_LONGS(nbits);
	if (!small_const_nbits(nbits)) {
		unsigned long len = (nlongs - 1) * sizeof(unsigned long);
		memset(dst, 0xff,  len);
	}
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}

static inline void bitmap_copy(unsigned long *dst, const unsigned long *src,
			unsigned long nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src;
	else {
		unsigned long len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memcpy(dst, src, len);
	}
}


static inline int bitmap_empty(const unsigned long *src, unsigned long nbits)
{
	if (small_const_nbits(nbits))
		return ! (*src & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_empty(src, nbits);
}

static inline int bitmap_full(const unsigned long *src, unsigned long nbits)
{
	if (small_const_nbits(nbits))
		return ! (~(*src) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_full(src, nbits);
}


static inline void bitmap_shift_right(unsigned long *dst,
			const unsigned long *src, int n, unsigned long nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src >> n;
	else
		__bitmap_shift_right(dst, src, n, nbits);
}

static inline void bitmap_shift_left(unsigned long *dst,
			const unsigned long *src, int n, unsigned long nbits)
{
	if (small_const_nbits(nbits))
		*dst = (*src << n) & BITMAP_LAST_WORD_MASK(nbits);
	else
		__bitmap_shift_left(dst, src, n, nbits);
}


#endif /* __ASSEMBLER__ */

#endif /* __LINUX_BITMAP_H */
