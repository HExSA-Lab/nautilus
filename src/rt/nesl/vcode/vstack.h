/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _VSTACK_H
#define _VSTACK_H

#include <cvl.h>
#include "vcode.h"

typedef struct vblock_t {
   vec_p vector;                        /* pointer into vector memory */
   int count;                           /* reference count of block */
   int pair_count;			/* number of uses in a pair */
   TYPE type;                           /* type of the vector */
   int len;                             /* length of vector */
   int seg_len;                         /* if segd, length of vector */
   int size;                            /* size, in vector memory units */
   int active;                          /* is memory active or free */
   struct vblock_t *bnext, *bprev;      /* doubly linked block list */
   struct vblock_t *fnext, *fprev;      /* doubly linked free list */
   struct vblock_t *first, *second;     /* children in a PAIR */
} vb_t;

extern void vstack_init PROTO_((unsigned));
extern vb_t *new_vector PROTO_((int, TYPE, int));
extern vb_t *new_pair PROTO_((vb_t*, vb_t*));
extern void vb_unpair PROTO_((vb_t*, vb_t**, vb_t**));
extern void assert_mem_size PROTO_((int));
extern void vb_print PROTO_((void));

extern vb_t *scratch_block;
#define SCRATCH 	(scratch_block->vector)

/* speed hack to optimize common case of reusing vectors */
#ifdef SPEEDHACKS
#define vstack_pop(_vb)					\
	do {if (--_vb->count <= 0) vstack_pop_real(_vb);} while (0)
#else
#define vstack_pop(_vb) vstack_pop_real(_vb)
#endif
extern void vstack_pop_real PROTO_((vb_t*));

/* another speed hack to optimize case of enough scratch space */
#ifdef SPEEDHACKS
#define assert_mem_size(_size)				\
    do {						\
	int _foo = (_size);				\
	if (_foo > scratch_block->size) assert_mem_size_real(_foo); \
    } while (0)
#else
#define assert_mem_size(_size) assert_mem_size_real(_size)
#endif
extern void assert_mem_size_real();

#ifdef SPEEDHACKS
#define vstack_copy(_vb) ((_vb)->count++)
#else
extern void vstack_copy PROTO_((vb_t*));
#endif

#endif /* _VSTACK_H */
