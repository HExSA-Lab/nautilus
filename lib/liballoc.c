#include <lib/liballoc.h>
#include <nautilus/naut_types.h>
#include <nautilus/math.h>
#include <nautilus/naut_assert.h>
#include <nautilus/paging.h>
#include <nautilus/nautilus.h>

/* TODO: currently, free'd pages will go back to 
 * ilog2 of their size, not the index they were placed in. 
 * This will eventually cause us to run out of memory even 
 * if we're freeing stuff. This is due to the requirement
 * that we fill in each index before we promote to the next bin
 */


#ifdef NAUT_CONFIG_DEBUG_MALLOC
#include <nautilus/printk.h>
#define MALLOC_DEBUG(fmt, args...) printk("MALLOC: " fmt, ##args);
#else 
#define MALLOC_DEBUG(fmt, args...) 
#endif

#define LIBALLOC_MAGIC	0xc001c0de
#define MAXCOMPLETE		5
#define MAXEXP	32
#define MINEXP  6

#define MODE_BEST			0
#define MODE_INSTANT		1

#define BLOCK_ALLOCATED -1
#define DEFAULT_ALIGN_BYTES    16

#define MODE	MODE_BEST

struct boundary_tag* l_freePages[MAXEXP];		//< Allowing for 2^MAXEXP blocks
int 				 l_completePages[MAXEXP];	//< Allowing for 2^MAXEXP blocks


#ifdef NAUT_CONFIG_DEBUG_MALLOC
unsigned long l_allocated = 0;		//< The real amount of memory allocated.
unsigned long l_inuse = 0;			//< The amount of memory in use (malloc'ed). 
#endif


static unsigned l_initialized = 0;			//< Flag to indicate initialization.	
static unsigned l_pageSize  = PAGE_SIZE;	    //< Individual page size
static unsigned l_pageCount = 1;			    //< Minimum number of pages to allocate.


// ***********   HELPER FUNCTIONS  *******************************


static void*
liballoc_memset (void* s, int c, size_t n)
{
	int i;
	for (i = 0; i < n ; i++)
		((char*)s)[i] = c;
	
	return s;
}


static void*
liballoc_memcpy (void* s1, const void* s2, size_t n)
{
    char *cdest;
    char *csrc;
    unsigned int *ldest = (unsigned int*)s1;
    unsigned int *lsrc  = (unsigned int*)s2;

    while (n >= sizeof(unsigned int))
    {
        *ldest++ = *lsrc++;
        n -= sizeof(unsigned int);
    }

    cdest = (char*)ldest;
    csrc  = (char*)lsrc;

    while (n > 0)
    {
        *cdest++ = *csrc++;
        n -= 1;
    }

    return s1;
}
 

#ifdef NAUT_CONFIG_DEBUG_MALLOC
static void 
dump_array (void)
{
	int i = 0;
	struct boundary_tag *tag = NULL;

	MALLOC_DEBUG("------ Free pages array ---------\n");
	MALLOC_DEBUG("System memory allocated: %u\n", l_allocated);
	MALLOC_DEBUG("Memory in used (malloc'ed): %u\n", l_inuse);

		for (i = 0; i < MAXEXP; i++) {
            //MALLOC_DEBUG("l_completePages: %p\n", (void*)(l_completePages + i));
			MALLOC_DEBUG("%.2i(%d): ", i, l_completePages[i]);
	
			tag = l_freePages[i];

			while (tag != NULL) {

				if (tag->split_left  != NULL) {
                    printk("*");
                }

				printk("%u", tag->real_size);

				if (tag->split_right != NULL) {
                    printk("*");
                }
	
				printk(" ");
				tag = tag->next;
			}

			printk("\n");

		}

	MALLOC_DEBUG("'*' denotes a split to the left/right of a tag\n");
}
#endif


static inline void 
insert_tag (struct boundary_tag *tag, int index)
{
	int realIndex;
    int i;

	
	if (index < 0) {

		realIndex = ilog2(tag->real_size - sizeof(struct boundary_tag));
        realIndex = (realIndex < MINEXP) ? MINEXP : realIndex;

	} else {
		realIndex = index;
    }

	tag->index = realIndex;

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    for (i = 0; i < MAXEXP; i++) {

        struct boundary_tag * t = l_freePages[i];
        int count = 0;

        while (t) {

            if (t == tag) {
                panic("Inserting tag [%p] (realsize=%u) of idx=%d that already exists at freeList[%u], elm=%u, it's idx=%d, realsize=%u\n",
                        tag,
                        tag->real_size,
                        tag->index,
                        i,
                        count,
                        t->index,
                        t->real_size);
            }
            
            t = t->next;
            count++;
        }
    }
#endif

    ASSERT(tag->index != BLOCK_ALLOCATED);

	
	if (l_freePages[realIndex]) {
		l_freePages[realIndex]->prev = tag;
		tag->next = l_freePages[realIndex];
	}

	l_freePages[realIndex] = tag;
}


static inline void 
remove_tag (struct boundary_tag *tag)
{
    ASSERT(tag->index != BLOCK_ALLOCATED);

    //printk("removing tag [%p]\n", tag);

    // KCH: pretty sure this is the problem, what if index isn't right? e.g. in 
    // the case where we split a tag
	if (l_freePages[tag->index] == tag) {
        //printk("removing it from front of free list bucket[%u]\n", tag->index);
        l_freePages[tag->index] = tag->next;
    }

	if (tag->prev) {
        tag->prev->next = tag->next;
    }

	if (tag->next) {
        tag->next->prev = tag->prev;
    }

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    // do a sweep just to make sure
    int i, count;
    for (i = 0; i < MAXEXP; i++) {

        struct boundary_tag * t = l_freePages[i];
        count = 0;

        while (t) {
            if (t == tag) {
                ERROR_PRINT("tag [%p] not properly removed! it's still in freePages[%u], elm %u, (idx=%d, size=%u, realsize=%u)\n", 
                        tag,
                        i, 
                        count, 
                        tag->index,
                        tag->size,
                        tag->real_size);

                if (t->split_right) {
                    ERROR_PRINT("it's right split is idx=%d, size=%u, realsize=%u\n",
                            t->split_right->index,
                            t->split_right->size,
                            t->split_right->real_size);
                }

                if (t->split_left) {
                    ERROR_PRINT("it's left split is idx=%d, size=%u, realsize=%u\n",
                            t->split_left->index,
                            t->split_left->size,
                            t->split_left->real_size);
                }
                panic("Exiting\n");
            }
            t = t->next;
            count++;
        }
    }
#endif

	tag->next  = NULL;
	tag->prev  = NULL;
	tag->index = BLOCK_ALLOCATED;
}


static inline struct boundary_tag* 
melt_left (struct boundary_tag *tag)
{
	struct boundary_tag *left = tag->split_left;

    ASSERT(left);
    ASSERT(left->index != BLOCK_ALLOCATED);
    ASSERT(tag->index == BLOCK_ALLOCATED);

    remove_tag(left);

	left->real_size   += tag->real_size;
	left->split_right  = tag->split_right;
	
	if (tag->split_right) {
        tag->split_right->split_left = left;
    }


	return left;
}


static inline struct boundary_tag* 
absorb_right (struct boundary_tag *tag)
{
    struct boundary_tag *right = tag->split_right;

    ASSERT(right);
    ASSERT(right->index != BLOCK_ALLOCATED);
    ASSERT(tag->index == BLOCK_ALLOCATED);

    remove_tag(right);

    tag->real_size   += right->real_size;
    tag->split_right  = right->split_right;

    if (right->split_right) {
        right->split_right->split_left = tag;
    }

    return tag;
}


static inline struct boundary_tag* 
split_tag (struct boundary_tag * tag, int index)
{
    ASSERT(tag);

    // subtract the size of the original tag
    long remainder = (long)tag->real_size - (long)sizeof(struct boundary_tag) - (long)tag->size - (long)tag->padding;

    ASSERT(remainder > 0);

    struct boundary_tag *new_tag = 
        (struct boundary_tag*)((unsigned long)tag + sizeof(struct boundary_tag) + tag->size + tag->padding);	

    ASSERT(new_tag > tag);

    new_tag->magic     = LIBALLOC_MAGIC;
    new_tag->real_size = remainder;	

    new_tag->next = NULL;
    new_tag->prev = NULL;

    new_tag->padding  = 0;
    new_tag->pad_byte = 0;

    new_tag->split_left  = tag;
    new_tag->split_right = tag->split_right;

    if (new_tag->split_right != NULL) {
        new_tag->split_right->split_left = new_tag;
    }

    tag->split_right = new_tag;
    tag->real_size   -= new_tag->real_size;

    insert_tag(new_tag, index);

    return new_tag;
}


static struct boundary_tag* 
allocate_new_tag (unsigned long size)
{
    unsigned long pages;
    unsigned long usage;
    struct boundary_tag *tag;

    // This is how much space is required.
    usage  = size + sizeof(struct boundary_tag);

    // Perfect amount of space
    pages = usage / l_pageSize;
    pages = (usage % l_pageSize) ? pages + 1 : pages;

    // Make sure it's >= the minimum size.
    pages = (pages < l_pageCount) ? l_pageCount : pages;

    MALLOC_DEBUG("Liballoc allocating %u pages (usage=%u B) (page_size=%lx)\n", pages, usage, l_pageSize);
    tag = (struct boundary_tag*)liballoc_alloc(pages);

    if (!tag)  {
        MALLOC_DEBUG("ERROR: liballoc out of memory!\n");
        return NULL;	// uh oh, we ran out of memory.
    } 

    ASSERT(nk_page_allocated((addr_t)tag));

    tag->magic 		= LIBALLOC_MAGIC;
    tag->size 		= size;
    tag->real_size 	= pages * l_pageSize;
    tag->index 		= BLOCK_ALLOCATED;

    tag->next		= NULL;
    tag->prev		= NULL;
    tag->split_left 	= NULL;
    tag->split_right 	= NULL;

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    MALLOC_DEBUG("Allocated block at %p of %u pages (%u bytes) for req. size %u bytes\n", 
            tag, pages, pages * l_pageSize, size);

    l_allocated += pages * l_pageSize;

    MALLOC_DEBUG("Total memory usage = %u KB\n",  (unsigned long)(l_allocated / (1024)));
#endif

    return tag;
}


static void
malloc_init (void)
{
    int index;

    MALLOC_DEBUG("%s\n","liballoc initializing.");

    // clear out the list(s) of free pages */
    for (index = 0; index < MAXEXP; index++) {
        l_freePages[index]     = NULL;
        l_completePages[index] = 0;
    }

    l_initialized = 1;
}

/* checks if this is within the bounds of the
 * allocator, if not, pulls it up to 2^MINEXP size
 */
static inline void 
adjust_size (int * index, size_t * size)
{
    if (*index < MINEXP) {
        *index = MINEXP;
        *size  = 1 << MINEXP;
    }
}


static inline unsigned
get_pad (struct boundary_tag * tag)
{
    unsigned pad;

    /* check for minimal alignment, this is how much padding we need to have */
    pad = (((unsigned long)tag + sizeof(struct boundary_tag)) & 
            (MALLOC_DEFAULT_ALIGN-1)) ? 
        MALLOC_DEFAULT_ALIGN - (((unsigned long)tag + sizeof(struct boundary_tag)) % MALLOC_DEFAULT_ALIGN) :
        0;

    ASSERT(pad < MALLOC_DEFAULT_ALIGN);
    return pad;
}


/* returns 1 if we can fit a new request in this tag, 0 otherwise */
static inline uint8_t 
check_tag_for_fit (struct boundary_tag * tag, size_t size)
{
    unsigned align_pad = get_pad(tag);

    if (tag->real_size >= size + sizeof(struct boundary_tag) + align_pad) {
        MALLOC_DEBUG("Tag search found %u >= %u\n",
                tag->real_size - sizeof(struct boundary_tag), 
                size + sizeof(struct boundary_tag));
        return 1;
    }

    return 0;
}


static struct boundary_tag*
find_free_tag (int index, size_t size)
{
    struct boundary_tag * tag = NULL;

    // we want to have at least one free page for each size, so 
    // bail on the search if that bin is empty
    // TODO: need a smarter algorithm to ensure even distribution among
    // free lists
    
    if (!l_freePages[index]) {
        return NULL;
    }

    // Find a tag with enough space
    do {

        // Start at the front of the list.
        tag = l_freePages[index];

        while (tag != NULL) {

            ASSERT(tag->index != BLOCK_ALLOCATED);

            // Is there enough space in this tag?
            if (check_tag_for_fit(tag, size)) {
                MALLOC_DEBUG("Found tag at index position %u\n", index);
                return tag;
            }

            tag = tag->next;
        }

        // promote to next exponent if we need to
    } while (tag == NULL && ++index < MAXEXP);

    return NULL;
}


void*
malloc (size_t size)
{
    int index;
    void *ptr;
    struct boundary_tag *tag = NULL;
    int flags;
    size_t size_actual;

    NK_MALLOC_PROF_ENTRY();

    MALLOC_DEBUG("malloc request for %u bytes\n", size);

    flags = liballoc_lock();

    if (!l_initialized) {
        malloc_init();
    }

    index       = ilog2(size) + MODE;
    size_actual = size;

    // round up to min block size if we need to 
    adjust_size(&index, &size_actual);

    MALLOC_DEBUG("index calculated as %d\n", index);

    tag = find_free_tag(index, size_actual);

    // No page found in all of the lists. Make a new one.
    if (!tag) {	

        MALLOC_DEBUG("No free pages found, allocating a new one\n");

        if ((tag = allocate_new_tag(size)) == NULL) {
            panic("Malloc could not allocate page!\n");
            liballoc_unlock(flags);
            return NULL;
        }

    } else {

        // take it off the free list
        ASSERT(tag->index != BLOCK_ALLOCATED);
        remove_tag(tag);

        // we're taking one of the completely free pages away 
        // from the free list
        if ((tag->split_left == NULL) && 
                (tag->split_right == NULL)) {

            l_completePages[index]--;
        }
    }

    ASSERT(tag->index == BLOCK_ALLOCATED);
    ASSERT(tag->magic == LIBALLOC_MAGIC);
    ASSERT(index <= MAXEXP);

    // check for padding
    unsigned align_pad = get_pad(tag);

    tag->size    = size_actual;
    tag->padding = align_pad;

    MALLOC_DEBUG("Found tag (%p) with %lu bytes available (req. %lu bytes, actual=%lu, pad=%lu, leaving %lu) which has exponent: %u (%lu bytes)\n", 
            (void*)((unsigned long)tag + sizeof(struct boundary_tag)),
            tag->real_size - sizeof(struct boundary_tag), 
            size, 
            size_actual,
            align_pad,
            tag->real_size - tag->size - sizeof(struct boundary_tag) - align_pad,
            index, 
            1UL << index);

    // do we have leftover space to recycle?
    long remainder = (long)tag->real_size - (long)tag->size - (long)(2 * sizeof(struct boundary_tag)) - (long)align_pad;

    if (remainder >= (1L << MINEXP)) {

        MALLOC_DEBUG("There's plenty of room left, splitting: (remainder=%u)\n", remainder);

        // we only change the index if the remaining chunk is *smaller* than this index
        // NOTE: probably a better way to make this placement decision
        struct boundary_tag *new_tag = (remainder >= (1L<<index)) ? split_tag(tag, index) : split_tag(tag, -1);

        ASSERT(new_tag);
        ASSERT(new_tag != tag);
        ASSERT(new_tag->index != BLOCK_ALLOCATED);

        MALLOC_DEBUG("Old tag has become %u bytes, new tag is now %u bytes (%u exp)\n", tag->real_size, new_tag->real_size, new_tag->index );
    }

    ptr = (void*)((unsigned long)tag + sizeof(struct boundary_tag) + tag->padding);

    ASSERT((unsigned long)ptr % MALLOC_DEFAULT_ALIGN == 0);

    // set the pad count
    *(uint8_t*)((unsigned long)ptr - 1) = tag->padding;

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    l_inuse += tag->size + tag->padding;
    MALLOC_DEBUG("Malloc [ret=%p] used=%u.%uKB allocated=%u.%u\n", 
            ptr, 
            (unsigned long)l_inuse / 1024, 
            (unsigned long)l_inuse % 1024,
            (unsigned long)l_allocated / 1024,
            (unsigned long)l_allocated % 1024);

    dump_array();
#endif

    liballoc_unlock(flags);

    NK_MALLOC_PROF_EXIT();
    return ptr;
}


void 
free (void *ptr)
{
    int index;
    struct boundary_tag *tag;
    int flags;

    if (!ptr) {
        ERROR_PRINT("Free given bad pointer!\n");
        return;
    }

    flags = liballoc_lock();

    uint8_t padding = *(uint8_t*)((unsigned long)ptr - 1);

    tag = (struct boundary_tag*)((unsigned long)ptr - padding - sizeof(struct boundary_tag));

    ASSERT(tag);
    ASSERT(tag->magic == LIBALLOC_MAGIC);

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    l_inuse = l_inuse - tag->size - tag->padding;
    MALLOC_DEBUG("freeing [%p] - used=%u.%u KB allocated=%u.%u KB\n", 
            ptr, 
            (unsigned long)l_inuse / 1024, 
            (unsigned long)l_inuse % 1024, 
            (unsigned long)l_allocated / 1024,
            (unsigned long)l_allocated % 1024);
#endif

    // MELT LEFT...
    while (tag->split_left && 
            (tag->split_left->index != BLOCK_ALLOCATED)) {

        MALLOC_DEBUG("Melting tag left into available memory. Left (total size) was %u B, now %u B\n", 
                tag->split_left->real_size, 
                tag->split_left->real_size + tag->real_size)

        tag = melt_left(tag);

    }

    // MELT RIGHT...
    while (tag->split_right && 
            (tag->split_right->index != BLOCK_ALLOCATED)) {

        MALLOC_DEBUG("Melting tag right into available memory. This was %u B, now %u B\n", 
                tag->real_size, 
                tag->split_right->real_size + tag->real_size);

        tag = absorb_right(tag);

    }

    // Where is it going back to?
    index = ilog2(tag->real_size - sizeof(struct boundary_tag));
    index = (index < MINEXP) ? MINEXP : index;

    ASSERT(index <= MAXEXP); 
    ASSERT(index >= MINEXP);

    // A whole, empty block?
    if ((tag->split_left == NULL) && 
            (tag->split_right == NULL)) {	

        MALLOC_DEBUG("Free: this is a complete block\n");

        // we're holding too many free pages, return some to the page map 
        if (l_completePages[index] == MAXCOMPLETE) {

            ASSERT(tag->real_size == l_pageSize);

            unsigned long pages = tag->real_size / l_pageSize;

            pages += (tag->real_size % l_pageSize) ? 1 : 0;

            if (pages < l_pageCount) {
                pages = l_pageCount;
            }

            ASSERT(nk_page_allocated((addr_t)tag));

            liballoc_free(tag, pages);

            ASSERT(nk_page_free((addr_t)tag));

#ifdef NAUT_CONFIG_DEBUG_MALLOC 
            l_allocated -= pages * l_pageSize;
            MALLOC_DEBUG("Freeing memory region [%p] (%u pages)\n", tag, pages);
            dump_array();
#endif
            liballoc_unlock(flags);
            return;
        }

        // Increase the count of complete pages.
        l_completePages[index]++;
    }

    insert_tag(tag, index);

    MALLOC_DEBUG("Returning tag to free list with %uB (requested %uB), exponent=%u\n", 
            tag->real_size, 
            tag->size, 
            index); 

#ifdef NAUT_CONFIG_DEBUG_MALLOC
    dump_array();
#endif

    liballoc_unlock(flags);
}


void* 
calloc (size_t nobj, size_t size)
{
    int real_size;
    void *p;

    real_size = nobj * size;

    p = malloc(real_size);

    liballoc_memset(p, 0, real_size);

    return p;
}


void*
realloc (void *p, size_t size)
{
	void *ptr;
	struct boundary_tag *tag;
	int real_size;
    int flags;
	
	if (size == 0) {
		free(p);
		return NULL;
	}

	if (p == NULL) {
        return malloc(size);
    }

    flags = liballoc_lock();

    tag = (struct boundary_tag*)((unsigned long)p - sizeof( struct boundary_tag ));
    real_size = tag->size;

    liballoc_unlock(flags);

	if (real_size > size) {
        real_size = size;
    }

	ptr = malloc(size);
	liballoc_memcpy(ptr, p, real_size);
	free(p);

	return ptr;
}
