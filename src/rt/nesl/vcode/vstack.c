/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/


#include "config.h"
#include "vcode.h"
#include "vstack.h"

/* This module is responsible for managing vector memory.  The memory
 * management algorithm is as follows:
 * 1. vector memory is divided up into various sized blocks, which may be
 *    active or free
 * 2. corresponding to each block is a vb structure (of type vb_t)
 * 3. these structures are stored in a doubly linked list which preserves
 *    the order of the blocks in the actual vector memory.
 * 4. a set of free lists of unutilized blocks of memory is maintained, with 
 *    each list corresponding to a range of vector sizes (powers of 2)
 * 5. when memory is to be allocated, the appropriate free list is searched
 *    in first-fit manner for a large enough memory block. the required 
 *    memory is allocated from the block, which may then be split into 
 *    two pieces.  the unallocated block is put on the appropriate free list.
 * 6. reference counting is used to determine when a vector is no longer needed.
 * 7. when a vector is freed, the adjacent memory blocks are checked to 
 *    see if they are free.  if so, the blocks are merged.  free blocks are
 *    then put on the appropriate free list.
 */

vb_t *vblocks;				/* head of list */
vec_p cvl_mem;				/* vector memory */
int cvl_mem_size;			/* size of vector memory */

#define NBUCKETS 32
vb_t bucket_table[NBUCKETS];

/* a bucket is empty if the next field points back to itself */
#define isempty_bucket(n) (bucket_table[n].fnext == &bucket_table[n])

static vb_t *free_list_init PROTO_((unsigned, vec_p));
static void compact_mem PROTO_((void));

/* scratch_block always points to largest free block of memory */
vb_t *scratch_block;

/* ---------- scratch space functions -------------------------------*/

/* set scratch_block to largest free block of memory */
static void find_scratch PROTO_((void))
{
    int i = NBUCKETS - 1;
    while ( (i > 0) && isempty_bucket(i)) 
        i--;
    if (i == 0) {
        _fprintf(stderr, "vinterp: ran out of memory.  Rerun with larger -m argument.\n");
        vinterp_exit(1);
    }
    scratch_block = bucket_table[i].fprev;	/* last on list */
}

/* make sure that size_needed memory units are available in heap.
 * compact if not.
 */
void assert_mem_size_real(size_needed)
int size_needed;
{
    assert(size_needed >= 0);
#ifndef SPEEDHACKS
    if (size_needed <= scratch_block->size)
	return;
#endif
    if (garbage_notify) {
	_fprintf(stderr, "vinterp: memory compactor: %d units requested, largest available block has size %d\n",
	size_needed, scratch_block->size);
    }
    compact_mem();          /* try to make more room */
    if (size_needed > scratch_block->size) {
        _fprintf(stderr, "vinterp: ran out of memory. Rerun with larger -m argument.\n");
        vinterp_exit(1);
    }
}

/* ---------------- vblock constructors and destructors -----------------*/

/* We maintain an array of used vb_t's to avoid excess mallocs and frees. */
static vb_t **vb_free_list = NULL;	/* array of available vb */
static int vb_free_max = 0;		/* current size of free list */
static int vb_free_avail = 0;		/* first empty slot for vb_t* */
#define VB_FREE_INITIAL 16		/* initial number malloc'ed */

/* initialize vb free array */
static void vb_init PROTO_((void))
{
    int i;
    vb_t *temp  = (vb_t *) malloc (VB_FREE_INITIAL * sizeof (vb_t));
    vb_free_list =  (vb_t **) malloc (VB_FREE_INITIAL * sizeof (vb_t *));

    if (temp == NULL || vb_free_list == NULL) {
	_fprintf(stderr, "vinterp: couldn't allocate internal structures (vblock list).\n");
	vinterp_exit (1);
    }

    for (i = 0; i < VB_FREE_INITIAL; i ++) 
	vb_free_list[i] = temp + i;
    vb_free_max = VB_FREE_INITIAL;
    vb_free_avail = VB_FREE_INITIAL;
}

/* vblock constructor: get vb off free array, or malloc more vb's
 */
static vb_t *new_vb PROTO_((void))
{
    if (vb_free_avail <= 0) { 		/* no more available blocks */
	int count;			/* get some more */
	vb_t *blocks = (vb_t *) malloc(VB_FREE_INITIAL * sizeof(vb_t));
	if (blocks == NULL) {
	    _fprintf(stderr, "vinterp: error allocating internal structures (vb_t).\n");
	    vinterp_exit (1);
	}

	/* put new vb's into free table */
	assert(vb_free_max >= VB_FREE_INITIAL);	/* done in initialization */
	for (count = 0; count < VB_FREE_INITIAL; count++) {
	    vb_free_list[count] = blocks + count;
	}
	vb_free_avail = VB_FREE_INITIAL;
    }

    return vb_free_list[--vb_free_avail];
}

/* vblock destructer: return to free-table, making sure table is large enough 
 */
static void free_vb PROTO_((vb_t*));
static void free_vb(vb)
vb_t *vb;
{
    if (vb_free_avail == vb_free_max) {		/* no room in table */
	vb_free_max += VB_FREE_INITIAL;		/* make room for new ones */
	vb_free_list = (vb_t **)realloc(vb_free_list, vb_free_max*sizeof(vb_t*));
	if (vb_free_list == NULL) {
	    _fprintf(stderr, "vinterp: error allocating internal structures (vblock_list).\n");
	    vinterp_exit (1);
	}
    }
    vb_free_list[vb_free_avail++] = vb;     /* put vb on top of list */
    return;
}

/* ---------------- vmem initializer -----------------------------------*/

/* Allocate vector memory, set up vblock structures.
 * Try to allocate vmemsize number of doubles of vector memory 
 */
void vstack_init(vmemlen)
unsigned int vmemlen;
{
    int vmem_size = siz_fod((int)vmemlen);
    vec_p vmem = alo_foz(vmem_size);
    vb_t *vb;

    if (vmem == (vec_p) NULL) {
	_fprintf(stderr, "vinterp: cannot allocate vector memory of size %u.\n", vmemlen);
	_fprintf(stderr, "\trerun with smaller -m argument.\n");
	exit(1); 
    }

    cvl_mem = vmem;			/* set globals */
    cvl_mem_size = vmem_size;

    vb_init();				/* initialize vblock structures */
    vblocks = new_vb();
    vblocks->bprev = vblocks;
    vblocks->bnext = vblocks;
    vblocks->active = 1;		/* don't want to coallesce this! */

    vb = free_list_init(cvl_mem_size, cvl_mem);	/* chop up memory */

    vb->bprev = vblocks;		/* link into vblocks structure */
    vb->bnext = vblocks;
    vblocks->bnext = vb;
    vblocks->bprev = vb;

    find_scratch();
}

/* ----------------- free list stuff -------------------------------------*/

/* the free list is an array of buckets, each of which is a doubly linked 
 * list of bucket_items.
 */

/* Given a vector size, return the index of the bucket with which it is
 * associated.  bucket_size[i] is the maximum size of an item in bucket[i].
 * For now, we'll use power of two sized free lists.  Other possibilities
 * include Fibonacci numbers, or dynamically changing sizes that correspond
 * to actual vector usage....
 * This code gets called ALOT, so it should be fast.  Hence, all the
 * SPEEDHACKS mess.
 */
static int get_bucket_num PROTO_((int));
static int get_bucket_num(size)
int size;
{

#ifndef SPEEDHACKS
    static const unsigned int bucket_size[NBUCKETS] = {
	0x1, 0x2, 0x4, 0x8, 
	0x10, 0x20, 0x40, 0x80, 
	0x100, 0x200, 0x400, 0x800, 
	0x1000, 0x2000, 0x4000, 0x8000, 
	0x10000, 0x20000, 0x40000, 0x80000, 
	0x100000, 0x200000, 0x400000, 0x800000, 
	0x1000000, 0x2000000, 0x4000000, 0x8000000, 
	0x10000000, 0x20000000, 0x40000000, 0x80000000, 
    };
#endif /* SPEEDHACKS */
    int answer;

    if (size <= 1) 
	answer = 0;
    else {
#ifndef SPEEDHACKS
	const unsigned int *b = bucket_size + 1;
	while (size > *b)
	    b++;
	answer =  b - bucket_size;

#else /* SPEEDHACKS */
    /* This hack is ugly, but if we're using power of 2 lists,
     * it saves us a factor of two, (more on cray) on average over the above 
     * loop.  The list we want corresponds to the highest set bit in size.
     * so, if size = 5 (00101), the ugly stuff returns 2 (bits numbered
     * from 0, so we add 1 to get 3, and bucket_size[3] = 8. 
     * The floating point format should be defined in config.h.
     */
#if cray 
        /* cray has a single instruction for this !! */
	answer = 64 - _ldzero(size);	/* position of first 1 bit */
#else /* not-cray */

	/* assumes IEEE-754; exponent stored XS127 */
	/* cast number to float, this normalizes number; grab exponent */
	double		d;
	unsigned short	*s;
	int result;

	d = size;
	s = ((unsigned short *) &d);

#if	vax	/* D_floating format */
	result = ((s[0]>>7)&0xff) - 0x81;
#else
#if	FP_LITTLE_ENDIAN
	result = ((s[3]>>4)&0x7ff) - 0x3ff;
#else
	result = ((s[0]>>4)&0x7ff) - 0x3ff;
#endif
#endif
	if (1 << result != size) result += 1; 	/* don't add for 2^k */

	answer = result;

#endif /* cray */
#endif /* SPEEDHACKS */

    }
#ifdef BUCKET_DEBUG
    _fprintf(stderr, "get_bucket: requested %d; got %d\n", size, bucket_size[answer]);
#endif /* BUCKET_DEBUG */
    return answer;
}


/* splice vb out of both the free and vblock lists, and free the vb 
 */
static void splice_vb PROTO_((vb_t*));
static void splice_vb(vb)
vb_t *vb;
{
    vb_t *vb_prev = vb->bprev;		/* do CSE by hand */
    vb_t *vb_next = vb->bnext;

    vb_next->bprev = vb_prev;		/* vblock list */
    vb_prev->bnext = vb_next;

    vb_prev = vb->fprev;
    vb_next = vb->fnext;

    vb_next->fprev = vb_prev;		/* free list */
    vb_prev->fnext = vb_next;

    free_vb(vb);
}

/* zero out buckets, make one big block of free mem
 * caller is responsible for linking result into vblocks.
 */
static vb_t *free_list_init(vmemsize, vmemstart)
unsigned vmemsize;
vec_p vmemstart;
{
    int b;
    vb_t *vb, *bucket;

    for (b = 0, bucket = bucket_table; b < NBUCKETS; b++) {
	/* make each bucket circular */
	bucket->fnext = bucket;
	bucket->fprev = bucket;
	bucket++;
    }

    b = get_bucket_num((int)vmemsize);

    vb = new_vb();
    vb->size = vmemsize;
    vb->vector = vmemstart;
    vb->active = 0;
    vb->fnext = &bucket_table[b];
    vb->fprev = &bucket_table[b];
    bucket_table[b].fnext = vb;
    bucket_table[b].fprev = vb;

    return vb;
}

/* --------------------- vstack_pop ----------------------------------*/
/* pop an entry off the vstack: use reference counting to see if memory
 * can be freed or not.
 */
void vstack_pop_real(vb)
vb_t *vb;
{
#ifndef SPEEDHACKS
    vb->count--;
    if (vb->count > 0) {
	return;
    } else  {
#endif
	if (vb->pair_count != 0) return;	/* still used in a pair */
	else if (vb->type == Pair) {
	    vb_t *first = vb->first;
	    vb_t *second = vb->second;

	    assert (vb->count <= 0);
	    assert (vb->pair_count == 0);

	    /* can pop off children if both pair_count and count are 0 */
            if (-- first->pair_count == 0 && first->count == 0) {
		vstack_pop_real(first);
	    }
            if (-- second->pair_count == 0 && second->count == 0) {
		vstack_pop_real(second);
	    }

	    free_vb(vb);			/* free the vb struct */
	} else {
	    vb_t *bucket;

	    /* try to coalesce into adjacent blocks */
	    if (vb->bprev->active == 0) {
		vb_t *vb_prev = vb->bprev;
		vb->size += vb_prev->size;
		vb->vector = vb_prev->vector;
		splice_vb(vb_prev);
	    }
	    if (vb->bnext->active == 0) {
		vb_t *vb_next = vb->bnext;
		vb->size += vb_next->size;
		splice_vb(vb_next);
	    }

	    vb->active = 0;
	    
	    /* add to free_list */
	    bucket = &bucket_table[get_bucket_num(vb->size)];
	    vb->fnext = bucket->fnext;
	    vb->fprev = bucket;
	    bucket->fnext->fprev = vb;
	    bucket->fnext = vb;

	    /* keep scratch as biggest available block */
	    if (vb->size >= scratch_block->size) scratch_block = vb;
	}
#ifndef SPEEDHACKS
    }
#endif
}

/* --------------------- new_vector ------------------------------*/
/* allocate memory for a new vector of given type and length (and
 * segmentation, if its a segd).
 */
vb_t *new_vector(len, type, seglen)
int len;
TYPE type;
int seglen;				/* -1 if not type is not Segd */
{
    int size = -1;                      /* siz_...(len), for cvl memory */
    int bucket_num;                     /* which bucket to get from */
    int b;
    vb_t *vb;

    /* find out how much room to allocate */
    switch (type) {
        case Int:
            size = siz_foz(len); break;
        case Bool:
            size = siz_fob(len); break;
        case Float:
            size = siz_fod(len); break;
        case Segdes:
	    assert(seglen != -1);
            size = siz_fos(seglen, len); break;
        default:
            _fprintf(stderr, "vinterp: internal error: illegal type in new_vector.\n");
            vinterp_exit (1);
    }

    /* find proper free list */
    bucket_num = get_bucket_num(size);

    /* Do a first fit in the free list until we find a large enough free
     * block.  If this list doesn't have large enough block, take
     * first item in smallest, larger non-empty free list.  Set vb when
     * block is found.
     */
    vb = NULL;
    if (!(isempty_bucket(bucket_num))) {
	/* first-fit: use head of list as sentinal to avoid end test
	 * in inner loop.  This requires a check at the end.
	 */
	register vb_t *vb_free = bucket_table + bucket_num;
	vb_free->size = size;			/* sentinel */
	vb_free = vb_free->fnext;		/* start from next block */
	while (vb_free->size < size)		/* so don't check for NULL */
	    vb_free = vb_free->fnext;
	if (vb_free != &bucket_table[bucket_num]) {  /* didn't hit sentinel */
	    vb = vb_free;			     /* found it */
	}
    }
    if (vb == NULL) {		/* list empty, or no big enough block */
	for (b = bucket_num+1; (b < NBUCKETS) && isempty_bucket(b); b++)
	    ;
	if (b != NBUCKETS) vb = bucket_table[b].fnext;
    }

    if (vb == NULL) {			/* memory not available */
	compact_mem();		
	if (scratch_block->size < size) { /* only free block is scratch */
	    _fprintf(stderr,
     "vinterp: ran out of vector memory.  Rerun using larger -m argument.\n");
	    vinterp_exit(1);
	} else vb = scratch_block;
    }

    assert(vb != NULL);			/* just checking.... */
    assert(vb->size >= size);
    assert(vb->active == 0);

    if (vb->size > size) {		/* split bucket in two */
	vb_t *vb_free = new_vb();
	int new_size = vb->size - size;
	int new_b = get_bucket_num(new_size);

	vb_free->size = new_size;
	vb_free->vector = add_fov(vb->vector, size);
	vb_free->active = 0;

	vb_free->fnext = bucket_table[new_b].fnext;	/* in free list */
	vb_free->fprev = &bucket_table[new_b];
	bucket_table[new_b].fnext->fprev = vb_free;
        bucket_table[new_b].fnext = vb_free;

	vb_free->bprev = vb;				/* in block list */
	vb_free->bnext = vb->bnext;
	vb->bnext->bprev = vb_free;
	vb->bnext = vb_free;

	vb->size = size;		/* adjust size of remaining piece */
    }

    vb->fnext->fprev = vb->fprev;		/* remove from free list */
    vb->fprev->fnext = vb->fnext;
    vb->active = 1;

    vb->count = 1;
    vb->pair_count = 0;
    vb->type = type;
    vb->len = len;
    vb->seg_len = seglen;
    
    if (vb == scratch_block)		/* make sure didn't trash scratch */
	find_scratch();

    return vb;
}

vb_t *new_pair(vb_first, vb_second)
vb_t *vb_first, *vb_second;
{
    vb_t *vb_pair = new_vb();

    vb_pair->count = 1;
    vb_pair->type = Pair;
    vb_pair->first = vb_first;
    vb_pair->second = vb_second;

    vb_first->pair_count++;
    vb_second->pair_count++;

    vb_first->count--;
    vb_second->count--;

    return vb_pair;
}

/* return the items in pair through pointers */
void vb_unpair(vb_pair, p_vb_first, p_vb_second)
vb_t *vb_pair;
vb_t **p_vb_first, **p_vb_second;
{
    assert(vb_pair->type = Pair);
    assert(vb_pair->first != NULL);
    assert(vb_pair->first->pair_count > 0);
    assert(vb_pair->second != NULL);
    assert(vb_pair->second->pair_count > 0);

    *p_vb_first = vb_pair->first;
    *p_vb_second = vb_pair->second;


    (*p_vb_first)->count++;
    (*p_vb_first)->pair_count--;
    (*p_vb_second)->count++;
    (*p_vb_second)->pair_count--;

    vb_pair->count--;

    if (vb_pair->count == 0 && vb_pair->pair_count == 0) 
	free_vb(vb_pair);
}

/* to copy a vector, just increment the reference counter */
#ifndef SPEEDHACKS
void vstack_copy(vb)
vb_t *vb;
{
    vb->count ++;
}
#endif

/* ---------------------- memory compaction -----------------------------*/
/* To compact memory we traverse the vb list, copying active blocks as
 * far to the front as we can.  This uses the fact that the vb list
 * respects the order of the blocks in vector memory, so we can can always 
 * copy backwards. As we traverse the list, we can splice out free blocks.
 * When we're done, all the free-lists should be empty, and we can start
 * over.  This has the property that at any time in the process, all blocks
 * still on the free list are guarenteed to be free, and hence scratch space
 * is still available for copying.  If necessary we can complicate the
 * process by expanding the free pool as vectors are removed.
 *
 * compact_mem() resets the scratch_block to the (only) free block.
 */
static void compact_mem PROTO_((void))
{
    static int compacting = 0;		/* use this to avoid recursive calls */
    vec_p from;				/* where vector starts */
    vec_p to;				/* where it goes to */
    vec_p new_to;			/* where next vector goes */
    vb_t *vb;				/* block of memory being copied */
    int vmem_size;			/* memory still left */

    if (garbage_notify)
	_fprintf(stderr, "compacting vector memory...");
    if (compacting) {
	/* The compactor may need scratch to do the vector copies. 
	 * assert_mem_size() calls the compactor if there is not enough
	 * scratch.  Thus we may recursively call the compactor.  
	 * This is very bad.
	 */
	_fprintf(stderr, "vinterp: not enough memory to compact memory.\n\tRerun with larger -m argument.\n");
	exit(1);
    } else {
	compacting = 1;
    }

    vmem_size = cvl_mem_size;

    /* Copy vectors to top of memory */
    /* Contents of active blocks are copied, and vector field is updated.
     * vb_t's corresponding to free blocks are deleted from the blocks
     * structure.
     */
    for (vb = vblocks->bnext, to = cvl_mem;	/* start at beginning */
	 vb != vblocks;				/* do whole list */
	 ) {
	if (vb->active) {			/* block is active, so copy */
	    from = vb->vector;

	    assert_mem_size(mov_fov_scratch(vb->size));
	    mov_fov(to, from, vb->size, SCRATCH);

	    new_to = add_fov(to, vb->size);	/* bump pointer after to */
	    vb->vector = to;
	    to = new_to;
    	    vmem_size -= vb->size;
	    vb = vb->bnext;
        } else {				/* free this node */
	    vb_t *to_free = vb;			/* save pointers */
	    vb = vb->bnext;
	    splice_vb(to_free);			/* remove block */
	}
    }

    /* At this point, the free list should have been emptied out, so
     * reinitialize, starting free memory at current to.
     */
    vb = free_list_init((unsigned)vmem_size, to);

    /* relink free node (final block) into vblocks structure */
    vb->bnext = vblocks;		/* circular list */
    vb->bprev = vblocks->bprev;
    vblocks->bprev->bnext = vb;
    vblocks->bprev = vb;

    /* set scratch to the free block */
    /* don't bother calling, find_scratch(); just do */
    scratch_block = vb;

    if (garbage_notify)
	_fprintf(stderr, "done\n");
    
    compacting = 0;
}

/* -----------------------debugging routines-------------------*/
/* walk through the vblock list, printing out size of blocks and whether
 * they are free or not 
 */
void vb_print()
{
    vb_t *vb;

    _fprintf(stderr, "memory:  ");
    for (vb = vblocks->bnext;		/* start at beginning */
	 vb != vblocks;			/* do whole list */
	 vb = vb->bnext) {
	_fprintf(stderr, "%d%s%s  ", vb->size, 
				    vb->active ? "" : "*",
				    scratch_block == vb ? "#" : "");
    }
    _fprintf(stderr, "\n");
}

