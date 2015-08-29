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
