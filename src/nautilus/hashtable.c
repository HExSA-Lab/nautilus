/*
   Copyright (c) 2002, 2004, Christopher Clark
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.

 * Neither the name of the original author; nor the names of any contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.


 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Modifications made by Jack Lange <jarusl@cs.northwestern.edu> */
/* Further modifications by Kyle C. Hale 2014 <kh@u.northwestern.edu> */

#include <nautilus/nautilus.h>
#include <nautilus/hashtable.h>
#include <nautilus/naut_string.h>
#include <nautilus/mm.h>


struct nk_hash_entry {
    addr_t key;
    addr_t value;
    uint_t hash;
    struct nk_hash_entry * next;
};

struct nk_hashtable {
    uint_t table_length;
    struct nk_hash_entry ** table;
    uint_t entry_count;
    uint_t load_limit;
    uint_t prime_index;
    uint_t (*hash_fn) (addr_t key);
    int (*eq_fn) (addr_t key1, addr_t key2);
};


/* HASH FUNCTIONS */


static inline uint_t 
do_hash (struct nk_hashtable * htable, addr_t key) 
{
    /* Aim to protect against poor hash functions by adding logic here
     * - logic taken from java 1.4 hashtable source */
    uint_t i = htable->hash_fn(key);
    i += ~(i << 9);
    i ^=  ((i >> 14) | (i << 18)); /* >>> */
    i +=  (i << 4);
    i ^=  ((i >> 10) | (i << 22)); /* >>> */

    return i;
}


/* HASH AN UNSIGNED LONG */
/* LINUX UNSIGHED LONG HASH FUNCTION */
#ifdef __NAUT_32BIT__
/* 2^31 + 2^29 - 2^25 + 2^22 - 2^19 - 2^16 + 1 */
#define GOLDEN_RATIO_PRIME 0x9e370001UL
#define BITS_PER_LONG 32
#else
/*  2^63 + 2^61 - 2^57 + 2^54 - 2^51 - 2^18 + 1 */
#define GOLDEN_RATIO_PRIME 0x9e37fffffffc0001UL
#define BITS_PER_LONG 64
#endif


ulong_t 
nk_hash_long (ulong_t val, uint_t bits) 
{
    ulong_t hash = val;

#ifdef __NAUT_32BIT__
    /* On some cpus multiply is faster, on others gcc will do shifts */
    hash *= GOLDEN_RATIO_PRIME;
#else 
    /*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
    ulong_t n = hash;
    n <<= 18;
    hash -= n;
    n <<= 33;
    hash -= n;
    n <<= 3;
    hash += n;
    n <<= 3;
    hash -= n;
    n <<= 4;
    hash += n;
    n <<= 2;
    hash += n;
#endif

    /* High bits are more random, so use them. */
    return hash >> (BITS_PER_LONG - bits);
}


/* HASH GENERIC MEMORY BUFFER */
/* ELF HEADER HASH FUNCTION */
ulong_t 
nk_hash_buffer (uchar_t * msg, uint_t length) 
{
    ulong_t hash = 0;
    ulong_t temp = 0;
    uint_t i;

    for (i = 0; i < length; i++) {
        hash = (hash << 4) + *(msg + i) + i;
        if ((temp = (hash & 0xF0000000))) {
            hash ^= (temp >> 24);
        }
        hash &= ~temp;
    }
    return hash;
}


/*****************************************************************************/
/* indexFor */
static inline uint_t 
indexFor (uint_t table_length, uint_t hash_value) 
{
    return (hash_value % table_length);
};


/* Only works if table_length == 2^N */
/*
   static inline uint_t indexFor(uint_t table_length, uint_t hashvalue)
   {
   return (hashvalue & (table_length - 1u));
   }
   */

/*****************************************************************************/
#define freekey(X) free(X)


static void* 
tmp_realloc (void * old_ptr, uint_t old_size, uint_t new_size) 
{
    void * new_buf = malloc(new_size);

    if (new_buf == NULL) {
        return NULL;
    }

    memcpy(new_buf, old_ptr, old_size);
    free(old_ptr);

    return new_buf;
}


/*
   Credit for primes table: Aaron Krowne
http://br.endernet.org/~akrowne/
http://planetmath.org/encyclopedia/GoodHashTablePrimes.html
*/
static const uint_t primes[] = 
{ 
    53, 97, 193, 389,
    769, 1543, 3079, 6151,
    12289, 24593, 49157, 98317,
    196613, 393241, 786433, 1572869,
    3145739, 6291469, 12582917, 25165843,
    50331653, 100663319, 201326611, 402653189,
    805306457, 1610612741 
};


// this assumes that the max load factor is .65
static const uint_t load_factors[] = 
{
    35, 64, 126, 253,
    500, 1003, 2002, 3999,
    7988, 15986, 31953, 63907,
    127799, 255607, 511182, 1022365,
    2044731, 4089455, 8178897, 16357798,
    32715575, 65431158, 130862298, 261724573,
    523449198, 1046898282 
};

const uint_t prime_table_length = sizeof(primes) / sizeof(primes[0]);


/*****************************************************************************/
struct nk_hashtable * 
nk_create_htable (uint_t min_size,
                  uint_t (*hash_fn) (addr_t),
                  int (*eq_fn) (addr_t, addr_t)) 
{
    struct nk_hashtable * htable;
    uint_t prime_index;
    uint_t size = primes[0];

    /* Check requested hashtable isn't too large */
    if (min_size > (1u << 30)) {
        return NULL;
    }

    /* Enforce size as prime */
    for (prime_index = 0; prime_index < prime_table_length; prime_index++) {
        if (primes[prime_index] > min_size) { 
            size = primes[prime_index]; 
            break; 
        }
    }

    /* couldn't get a big enough table */
    if (prime_index == prime_table_length) {
        return NULL;
    }

    htable = (struct nk_hashtable *)malloc(sizeof(struct nk_hashtable));

    if (htable == NULL) {
        return NULL; /*oom*/
    }

    htable->table = (struct nk_hash_entry **)malloc(sizeof(struct nk_hash_entry*) * size);

    if (htable->table == NULL) { 
        free(htable); 
        return NULL;  /*oom*/
    }


    memset(htable->table, 0, size * sizeof(struct nk_hash_entry *));

    htable->table_length  = size;
    htable->prime_index   = prime_index;
    htable->entry_count   = 0;
    htable->hash_fn       = hash_fn;
    htable->eq_fn         = eq_fn;
    htable->load_limit    = load_factors[prime_index];

    return htable;
}



/*****************************************************************************/
static int 
hashtable_expand (struct nk_hashtable * htable) 
{
    /* Double the size of the table to accomodate more entries */
    struct nk_hash_entry ** new_table;
    struct nk_hash_entry * tmp_entry;
    struct nk_hash_entry ** entry_ptr;
    uint_t new_size;
    uint_t i;
    uint_t index;

    /* Check we're not hitting max capacity */
    if (htable->prime_index == (prime_table_length - 1)) {
        return 0;
    }

    new_size = primes[++(htable->prime_index)];

    new_table = (struct nk_hash_entry **)malloc(sizeof(struct nk_hash_entry*) * new_size);

    if (new_table != NULL) {
        memset(new_table, 0, new_size * sizeof(struct hash_entry *));
        /* This algorithm is not 'stable'. ie. it reverses the list
         * when it transfers entries between the tables */

        for (i = 0; i < htable->table_length; i++) {

            while ((tmp_entry = htable->table[i]) != NULL) {
                htable->table[i] = tmp_entry->next;

                index = indexFor(new_size, tmp_entry->hash);

                tmp_entry->next = new_table[index];

                new_table[index] = tmp_entry;
            }
        }

        free(htable->table);

        htable->table = new_table;
    } else {
        /* Plan B: realloc instead */

        //new_table = (struct hash_entry **)realloc(htable->table, new_size * sizeof(struct hash_entry *));
        new_table = (struct nk_hash_entry **)tmp_realloc(htable->table, primes[htable->prime_index - 1], 
                new_size * sizeof(struct nk_hash_entry *));

        if (new_table == NULL) {
            (htable->prime_index)--;
            return 0;
        }

        htable->table = new_table;

        memset(new_table[htable->table_length], 0, new_size - htable->table_length);

        for (i = 0; i < htable->table_length; i++) {

            for (entry_ptr = &(new_table[i]), tmp_entry = *entry_ptr; 
                    tmp_entry != NULL; 
                    tmp_entry = *entry_ptr) {

                index = indexFor(new_size, tmp_entry->hash);

                if (i == index) {
                    entry_ptr = &(tmp_entry->next);
                } else {
                    *entry_ptr = tmp_entry->next;
                    tmp_entry->next = new_table[index];
                    new_table[index] = tmp_entry;
                }
            }
        }
    }

    htable->table_length = new_size;

    htable->load_limit   = load_factors[htable->prime_index];

    return -1;
}

/*****************************************************************************/
uint_t 
nk_htable_count (struct nk_hashtable * htable) 
{
    return htable->entry_count;
}

/*****************************************************************************/
int 
nk_htable_insert (struct nk_hashtable * htable, 
                  addr_t key, 
                  addr_t value) 
{
    /* This method allows duplicate keys - but they shouldn't be used */
    uint_t index;
    struct nk_hash_entry * new_entry;

    if (++(htable->entry_count) > htable->load_limit) {
        /* Ignore the return value. If expand fails, we should
         * still try cramming just this value into the existing table
         * -- we may not have memory for a larger table, but one more
         * element may be ok. Next time we insert, we'll try expanding again.*/
        hashtable_expand(htable);
    }


    new_entry = (struct nk_hash_entry *)malloc(sizeof(struct nk_hash_entry));

    if (new_entry == NULL) { 
        (htable->entry_count)--; 
        return 0; /*oom*/
    }

    new_entry->hash = do_hash(htable, key);

    index = indexFor(htable->table_length, new_entry->hash);

    new_entry->key = key;
    new_entry->value = value;

    new_entry->next = htable->table[index];

    htable->table[index] = new_entry;

    return -1;
}



int 
nk_htable_change (struct nk_hashtable * htable, 
                  addr_t key, 
                  addr_t value, 
                  int free_value) 
{
    struct nk_hash_entry * tmp_entry;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

            if (free_value) {
                free((void *)(tmp_entry->value));
            }

            tmp_entry->value = value;
            return -1;
        }
        tmp_entry = tmp_entry->next;
    }
    return 0;
}



int 
nk_htable_inc (struct nk_hashtable * htable, 
               addr_t key, 
               addr_t value) 
{
    struct nk_hash_entry * tmp_entry;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

            tmp_entry->value += value;
            return -1;
        }
        tmp_entry = tmp_entry->next;
    }
    return 0;
}


int 
nk_htable_dec (struct nk_hashtable * htable, addr_t key, addr_t value) 
{
    struct nk_hash_entry * tmp_entry;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    tmp_entry = htable->table[index];

    while (tmp_entry != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == tmp_entry->hash) && (htable->eq_fn(key, tmp_entry->key))) {

            tmp_entry->value -= value;
            return -1;
        }
        tmp_entry = tmp_entry->next;
    }
    return 0;
}




/*****************************************************************************/
/* returns value associated with key */
addr_t 
nk_htable_search (struct nk_hashtable * htable, addr_t key) 
{
    struct nk_hash_entry * cursor;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    cursor = htable->table[index];

    while (cursor != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == cursor->hash) && 
                (htable->eq_fn(key, cursor->key))) {
            return cursor->value;
        }

        cursor = cursor->next;
    }

    return (addr_t)NULL;
}

/*****************************************************************************/
/* returns value associated with key */
addr_t 
nk_htable_remove(struct nk_hashtable * htable, addr_t key, int free_key) 
{
    /* TODO: consider compacting the table when the load factor drops enough,
     *       or provide a 'compact' method. */

    struct nk_hash_entry * cursor;
    struct nk_hash_entry ** entry_ptr;
    addr_t value;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);

    index = indexFor(htable->table_length, hash_value);

    entry_ptr = &(htable->table[index]);
    cursor = *entry_ptr;

    while (cursor != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == cursor->hash) && 
                (htable->eq_fn(key, cursor->key))) {

            *entry_ptr = cursor->next;
            htable->entry_count--;
            value = cursor->value;

            if (free_key) {
                freekey((void *)(cursor->key));
            }
            free(cursor);

            return value;
        }

        entry_ptr = &(cursor->next);
        cursor = cursor->next;
    }
    return (addr_t)NULL;
}

/*****************************************************************************/
/* destroy */
void 
nk_free_htable (struct nk_hashtable * htable, int free_values, int free_keys) 
{
    uint_t i;
    struct nk_hash_entry * cursor;;
    struct nk_hash_entry **table = htable->table;

    if (free_values) {
        for (i = 0; i < htable->table_length; i++) {
            cursor = table[i];

            while (cursor != NULL) { 
                struct nk_hash_entry * tmp;

                tmp = cursor; 
                cursor = cursor->next; 

                if (free_keys) {
                    freekey((void *)(tmp->key)); 
                }
                free((void *)(tmp->value)); 
                free(tmp); 
            }
        }
    } else {
        for (i = 0; i < htable->table_length; i++) {
            cursor = table[i];

            while (cursor != NULL) { 
                struct nk_hash_entry * tmp;

                tmp = cursor; 
                cursor = cursor->next; 

                if (free_keys) {
                    freekey((void *)(tmp->key)); 
                }
                free(tmp); 
            }
        }
    }

    free(htable->table);
    free(htable);
}




/* HASH TABLE ITERATORS */



struct nk_hashtable_iter * 
nk_create_htable_iter (struct nk_hashtable * htable) 
{
    uint_t i;
    uint_t table_length;

    struct nk_hashtable_iter * iter = (struct nk_hashtable_iter *)malloc(sizeof(struct nk_hashtable_iter));

    if (iter == NULL) {
        return NULL;
    }

    iter->htable = htable;
    iter->entry = NULL;
    iter->parent = NULL;
    table_length = htable->table_length;
    iter->index = table_length;

    if (htable->entry_count == 0) {
        return iter;
    }

    for (i = 0; i < table_length; i++) {
        if (htable->table[i] != NULL) {
            iter->entry = htable->table[i];
            iter->index = i;
            break;
        }
    }

    return iter;
}


addr_t 
nk_htable_get_iter_key (struct nk_hashtable_iter * iter) 
{
    return iter->entry->key; 
}

addr_t 
nk_htable_get_iter_value (struct nk_hashtable_iter * iter) 
{
    return iter->entry->value; 
}


/* advance - advance the iterator to the next element
 *           returns zero if advanced to end of table */
int 
nk_htable_iter_advance (struct nk_hashtable_iter * iter) 
{
    uint_t j;
    uint_t table_length;
    struct nk_hash_entry ** table;
    struct nk_hash_entry * next;

    if (iter->entry == NULL) {
        return 0; /* stupidity check */
    }


    next = iter->entry->next;

    if (next != NULL) {
        iter->parent = iter->entry;
        iter->entry = next;
        return -1;
    }

    table_length = iter->htable->table_length;
    iter->parent = NULL;

    if (table_length <= (j = ++(iter->index))) {
        iter->entry = NULL;
        return 0;
    }

    table = iter->htable->table;

    while ((next = table[j]) == NULL) {
        if (++j >= table_length) {
            iter->index = table_length;
            iter->entry = NULL;
            return 0;
        }
    }

    iter->index = j;
    iter->entry = next;

    return -1;
}


/* remove - remove the entry at the current iterator position
 *          and advance the iterator, if there is a successive
 *          element.
 *          If you want the value, read it before you remove:
 *          beware memory leaks if you don't.
 *          Returns zero if end of iteration. */
int 
nk_htable_iter_remove (struct nk_hashtable_iter * iter, int free_key) 
{
    struct nk_hash_entry * remember_entry; 
    struct nk_hash_entry * remember_parent;
    int ret;

    /* Do the removal */
    if ((iter->parent) == NULL) {
        /* element is head of a chain */
        iter->htable->table[iter->index] = iter->entry->next;
    } else {
        /* element is mid-chain */
        iter->parent->next = iter->entry->next;
    }


    /* itr->e is now outside the hashtable */
    remember_entry = iter->entry;
    iter->htable->entry_count--;
    if (free_key) {
        freekey((void *)(remember_entry->key));
    }

    /* Advance the iterator, correcting the parent */
    remember_parent = iter->parent;
    ret = nk_htable_iter_advance(iter);

    if (iter->parent == remember_entry) { 
        iter->parent = remember_parent; 
    }

    free(remember_entry);
    return ret;
}


/* returns zero if not found */
int 
nk_htable_iter_search (struct nk_hashtable_iter * iter,
                       struct nk_hashtable * htable, 
                       addr_t key) 
{
    struct nk_hash_entry * entry;
    struct nk_hash_entry * parent;
    uint_t hash_value;
    uint_t index;

    hash_value = do_hash(htable, key);
    index = indexFor(htable->table_length, hash_value);

    entry = htable->table[index];
    parent = NULL;

    while (entry != NULL) {
        /* Check hash value to short circuit heavier comparison */
        if ((hash_value == entry->hash) && 
                (htable->eq_fn(key, entry->key))) {
            iter->index = index;
            iter->entry = entry;
            iter->parent = parent;
            iter->htable = htable;
            return -1;
        }
        parent = entry;
        entry = entry->next;
    }
    return 0;
}


void 
nk_destroy_htable_iter (struct nk_hashtable_iter * iter) 
{
    if (iter) {
        free(iter);
    }
}
