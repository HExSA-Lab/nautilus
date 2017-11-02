/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

/* this module deals with VCODE constant expressions and the constant pool
 */


#include "config.h"
#include "vcode.h"
#include "parse.h"
#include "program.h"
#include "constant.h"

/* the constant pool is the set of all vector constants in a VCODE
 * program.  this pool is filled during parsing. 
 */

/* data structure for constant pool */
typedef struct const_pool_entry {
    int *vector;                /* vector of values */
    vb_t *vb;        		/* vb for constant */
    TYPE type;                  /* type of the constant */
    int len;			/* length of the vector */
} const_pool_entry_t;

static const_pool_entry_t *const_pool;	/* array of constants */
static int const_pool_size_max = 0;	/* num of consts malloced so far */
static int const_pool_size = 0;         /* num of consts in pool so far */
#define CONST_POOL_SIZE_INITIAL 64	/* initial size of pool */

#define CONST_REF_COUNT 100000000	/* large number to cheat the
					 * reference counter */

/* the vector buffer holds the array representation of the vector
 * after parsing, const_install is called to transfer these into 
 * vector memory.
 */
typedef int elt;				/* vector element */
static elt *vector_buffer;                      /* where to read vec into */
static unsigned int vector_buffer_size_max;     /* bytes in the buffer */
static unsigned int vector_buffer_size;         /* bytes in buffer so far */
static unsigned int vector_buffer_len;          /* number of elements */
#define VECTOR_BUFFER_SIZE_INITIAL 16           /* initial size */

/* allocate space for a new vector.  initialize static variables
 * appropriately. returns 1 on success, 0 otherwise
 */
int const_begin()
{
    int success = 1;

    vector_buffer_size_max = VECTOR_BUFFER_SIZE_INITIAL * sizeof (elt);

    vector_buffer = (elt *) malloc (vector_buffer_size_max);
    if (vector_buffer == NULL) {
        _fprintf(stderr, " vinterp: internal error: cannot allocate memory for constant, line %d\n", yylineno);
        success = 0;
    } else {
        vector_buffer_size = vector_buffer_len = 0;
    }
    return success;
}

/* add new element to current vector buffer, increasing size of buffer, if
 * necessary.
 */
void const_new_val(type, constant)
TYPE type;
const_union constant;
{
    unsigned newsize = 0;

    /* compute size of vector_buffer after adding elt */
    switch (type) {
        case Int: case Segdes: 
            newsize = vector_buffer_size + sizeof(int);
            break;
	case Bool:
            newsize = vector_buffer_size + sizeof(cvl_bool);
            break;
        case Float:
            newsize = vector_buffer_size + sizeof(double);
            break;
        default:
            _fprintf(stderr, "vinterp: internal error: illegal type in const_new_val\n");
            vinterp_exit(1);
            break;
    }

    /* check to see if there is enough room allocated */
    if (newsize > vector_buffer_size_max) {
        vector_buffer_size_max *= 2;
        vector_buffer = (elt *) realloc (vector_buffer, vector_buffer_size_max);
       if (vector_buffer == NULL) {
            _fprintf(stderr, "vinterp: line %d: cannot reallocate memory for constant.\n", yylineno);
            vinterp_exit (1);
        }
    }

    /* put value in buffer: cast buffer to right type and append */
    switch (type) {
        case Int: case Segdes:
            *(((int *)vector_buffer) + vector_buffer_len) = constant.ival;
            break;
        case Bool:
            *(((cvl_bool *)vector_buffer) + vector_buffer_len) = (cvl_bool)constant.bval;
            break;
        case Float:
            *(((double *)vector_buffer) + vector_buffer_len) = constant.dval;
	    break;
        default:
            _fprintf(stderr, "vinterp: internal error: illegal type in const_new_val.\n");
            vinterp_exit(1);
            break;
    }

    vector_buffer_len++;
    vector_buffer_size = newsize;
}

/* We treat character strings a bit differently since we know the whole
 * string, instead of getting an element at a time.  Character strings
 * are NOT stored with a terminating NULL.
 */
void const_string(string, len)
YYTEXT_T string;
int len;
{
    int i;

    vector_buffer = (elt *) malloc (len * sizeof(elt));
    if (vector_buffer == NULL) {
        _fprintf(stderr, "vinterp: line %d: internal error: failed to allocate constant string.\n", yylineno);
        vinterp_exit (1);
    }

    if (*string != '\"') {
        _fprintf(stderr,"vinterp: line %d: parse error: strings must start with \".\n", yylineno);
        vinterp_exit (1);
    }

    /* copy contents of string into integer array */
    for (i = 1; i < len - 1; i++) {
        vector_buffer[i-1] = (elt) string[i];
    }

    if (string[len - 1] != '\"') {
        _fprintf(stderr,"vinterp: line %d: parse error: strings must end with \".\n", yylineno);
        vinterp_exit (1);
    }

    vector_buffer_len = len - 2;        /* both "'s get removed */
}

/* Found the end of the constant.  Add new entry to constant pool.
 * Return the index into the constant table, -1 if failure. */
int const_end(type)
TYPE type;
{
    if (const_pool_size_max == 0) {		/* first constant found */
        const_pool_size_max = CONST_POOL_SIZE_INITIAL;
        const_pool = (const_pool_entry_t *)
                     malloc(const_pool_size_max * sizeof (const_pool_entry_t));
        if (const_pool == NULL) {
            _fprintf(stderr, "vinterp: internal error: can't allocate const_pool\n");
            vinterp_exit(1);
        }
    } else if (const_pool_size >= const_pool_size_max) {
	/* need to grow the const pool */
        const_pool_size_max *= 2;
        const_pool = (const_pool_entry_t *)
                     realloc(const_pool, const_pool_size_max * sizeof (const_pool_entry_t));
        if (const_pool == NULL) {
            _fprintf(stderr, "vinterp: line %d: internal error: allocate memory for const_pool.\n", yylineno);
            vinterp_exit (1);
        }
    }

    const_pool[const_pool_size].type = type;
    const_pool[const_pool_size].vector = vector_buffer;
    const_pool[const_pool_size].len = vector_buffer_len;
    const_pool_size ++;
    return (const_pool_size - 1);
}

/* Put the contents of the constant pool into the vector memory.
 * We also free up the allocted array representation of the vectors. */
void const_install()
{
    int i;

    for (i = 0; i < const_pool_size; i ++) {
	const_pool_entry_t *cpe = const_pool + i;
        vb_t *vb;		/* place in heap constant is stored */

	switch (cpe->type) {	/* use c2v to create a vector */
	    case Int:
	    case Segdes:
	    case String:
		vb = new_vector(cpe->len, Int, -1);
		assert_mem_size(c2v_fuz_scratch(cpe->len));
		c2v_fuz(vb->vector, cpe->vector, cpe->len, SCRATCH);
		break;
	    case Bool:
		vb = new_vector(cpe->len, Bool, -1);
		assert_mem_size(c2v_fub_scratch(cpe->len));
		c2v_fub(vb->vector, (cvl_bool*) cpe->vector, cpe->len, SCRATCH);
		break;
	    case Float:
		vb = new_vector(cpe->len, Float, -1);
		assert_mem_size(c2v_fud_scratch(cpe->len));
		c2v_fud(vb->vector, (double*) cpe->vector, cpe->len, SCRATCH);
		break;
	    default:
		_fprintf(stderr, "vinterp: internal error: illegal type in const_pool\n");
		vinterp_exit(1);
		vb = NULL;	/* make gcc -Wall happy */
	}

	if (cpe->type == Segdes) {
	    /* make a segdes from the length vector */
	    int vector_len;
	    vb_t *segd_vb;

	    assert_mem_size(add_ruz_scratch(cpe->len));
	    vector_len = add_ruz(vb->vector, cpe->len, SCRATCH);

	    segd_vb = new_vector(cpe->len, Segdes, vector_len);
	    assert_mem_size(mke_fov_scratch(cpe->len, vector_len));
	    mke_fov(segd_vb->vector, vb->vector, vector_len, cpe->len, SCRATCH);

	    vb->count = 0;		/* free the lengths vector */
	    vstack_pop(vb);
	    vb = segd_vb;
	}

        /* make sure the constant isn't accidentally freed */
        vb->count = CONST_REF_COUNT;
	cpe->vb = vb;
    }
}

/* display the contents of an entry in the const_pool */
void const_show_vec(index)
int index;
{
    int i;
    const_pool_entry_t *cpe = const_pool + index;

    _fprintf(stderr, "type = %s; val = (", type_string(cpe->type));
	for (i = 0; i < cpe->len; i++) {
	    switch(cpe->type) {
		case Int: case Segdes:
		    _fprintf(stderr, " %d", *((int *)cpe->vector + i));
		    break;
		case Bool:
		    _fprintf(stderr, " %s", *((cvl_bool *)cpe->vector + i) ? "T" : "F");
		    break;
		case Float:
		    _fprintf(stderr, " %.14e", *(((double *)cpe->vector) + i));
		    break;
		case String:
		    _fprintf(stderr, "%c", (char)*(cpe->vector+i));
		    break;
		default:
		    _fprintf(stderr, "vinterp: internal error: illegal type in const_pool_entry %d", i);
		    vinterp_exit(1);
	    }
	}
	_fprintf(stderr, ")");
}

/* return the vb stored for a constant, and also make reference count big */
vb_t *const_vb(index)
int index;
{
    const_pool[index].vb->count = CONST_REF_COUNT; 
    return (const_pool[index].vb);
}
