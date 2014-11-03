/**
 * Kyle C. Hale 2014
 * 
 *
 * utility functions, we'll have these here just in case we need them
 *
 */

#include <naut_string.h>
#include <types.h>

#ifdef NAUT_CONFIG_USE_NAUT_BUILTINS
size_t 
strlen (const char * str)
{
    size_t ret = 0;
    while (str[ret] != 0) {
        ret++;
    }

    return ret;
}


size_t
strnlen (const char * str, size_t max)
{
    size_t ret = 0;
    while (max-- && str[ret] != 0) {
        ret++;
    }
    return ret;
}


void *
memcpy (void * dst, const void * src, size_t n)
{
    unsigned char * d = (unsigned char *)dst;
    unsigned char * s = (unsigned char *)src;

    while (n--) {
        *d++ = *s++;
    }

    return dst;
}


void * 
memset (void * dst, char c, size_t n)
{
    unsigned char * d = (unsigned char *)dst;
    while (n--) {
        *d++ = c;
    }

    return dst;
}


void * 
memmove (void * dst, const void * src, size_t n)
{
    unsigned long int dstp = (long int) dst;
    unsigned long int srcp = (long int) src;

    /* This test makes the forward copying code be used whenever possible.
       Reduces the working set.  */
    if (dstp - srcp >= n) {
        /* Copy from the beginning to the end.  */
        dst = memcpy (dst, src, n);
    } else {
        /* Copy from the end to the beginning.  */
        srcp += n;
        dstp += n;

        /* If there not too few bytes to copy, use word copy.  */
        if (n >= OP_T_THRES)
        {
            /* Copy just a few bytes to make DSTP aligned.  */
            n -= dstp % OPSIZ;
            BYTE_COPY_BWD (dstp, srcp, dstp % OPSIZ);

            /* Copy from SRCP to DSTP taking advantage of the known
               alignment of DSTP.  Number of bytes remaining is put
               in the third argument, i.e. in LEN.  This number may
               vary from machine to machine.  */

            WORD_COPY_BWD (dstp, srcp, n, n);

            /* Fall out and copy the tail.  */
        }

        /* There are just a few bytes to copy.  Use byte memory operations.  */
        BYTE_COPY_BWD (dstp, srcp, n);
    }

    return dst;
}

        

int 
memcmp (const void * s1_, const void * s2_, size_t n) {
    const char * s1 = s1_;
    const char * s2 = s2_;

    while (n > 0) {
    int cmp = (*s1 - *s2);

    if (cmp != 0) {
        return cmp;
    }

    ++s1;
    ++s2;
    }

    return 0;
}


int 
strcmp (const char * s1, const char * s2) 
{
    while (1) {
    int cmp = (*s1 - *s2);

    if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
        return cmp;
    }

    ++s1;
    ++s2;
    }
}


int 
strcasecmp (const char * s1, const char * s2) 
{
    while (1) {
    int cmp = (tolower(*s1) - tolower(*s2));

    if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
        return cmp;
    }

    ++s1;
    ++s2;
    }
}


int 
strncmp (const char * s1, const char * s2, size_t limit) 
{
    size_t i = 0;

    while (i < limit) {
    int cmp = (*s1 - *s2);

    if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
        return cmp;
    }

    ++s1;
    ++s2;
    ++i;
    }

    /* limit reached and equal */
    return 0;
}


int 
strncasecmp (const char * s1, const char * s2, size_t limit) 
{
    size_t i = 0;

    while (i < limit) {
    int cmp = (tolower(*s1) - tolower(*s2));

    if ((cmp != 0) || (*s1 == '\0') || (*s2 == '\0')) {
        return cmp;
    }

    ++s1;
    ++s2;
    ++i;
    }

    return 0;
}


char * 
strcat (char * s1, const char * s2) 
{
    char * t1 = s1;

    while (*s1) { s1++; }
    while (*s2) { *s1++ = *s2++; }

    *s1 = '\0';

    return t1;
}


char * 
strncat (char * s1, const char * s2, size_t limit) 
{
    size_t i = 0;
    char * t1;

    t1 = s1;

    while (*s1) { s1++; }

    while (i < limit) {
    if (*s2 == '\0') {
        break;
    }
    *s1++ = *s2++;
    }
    *s1 = '\0';
    return t1;
}


char * 
strcpy (char * dest, const char * src)
{
    char *ret = dest;

    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';

    return ret;
}


char * 
strncpy (char * dest, const char * src, size_t limit) 
{
    char * ret = dest;

    while ((*src != '\0') && (limit > 0)) {
    *dest++ = *src++;
    --limit;
    }

    if (limit > 0)
    *dest = '\0';

    return ret;
}


char * 
strchr (const char * s, int c) 
{
    while (*s != '\0') {
    if (*s == c)
        return (char *)s;
    ++s;
    }
    return 0;
}


char * 
strrchr (const char * s, int c) 
{
    size_t len = strlen(s);
    const char * p = s + len;

    while (p > s) {
    --p;

    if (*p == c) {
        return (char *)p;
    }
    }
    return 0;
}


char * 
strpbrk (const char * s, const char * accept) 
{
    size_t setLen = strlen(accept);

    while (*s != '\0') {
    size_t i;
    for (i = 0; i < setLen; ++i) {
        if (*s == accept[i]) {
        return (char *)s;
        }
    }
    ++s;
    }

    return 0;
}



#endif  /* USE_NAUT_BUILTINS */

int 
atoi (const char * buf) 
{
    int ret = 0;

    while ((*buf >= '0') && (*buf <= '9')) {
    ret *= 10;
    ret += (*buf - '0');
    buf++;
    }

    return ret;
}


int 
strtoi (const char * nptr, char ** endptr) 
{
    int ret = 0;
    char * buf = (char *)nptr;

    while ((*buf >= '0') && (*buf <= '9')) {
    ret *= 10;
    ret += (*buf - '0');

    buf++;

    if (endptr) {
        *endptr = buf;
    }
    }

    return ret;
}


uint64_t 
atox (const char * buf) {
    uint64_t ret = 0;

    if (*(buf + 1) == 'x') {
    buf += 2;
    }

    while (isxdigit(*buf)) {
    ret <<= 4;

    if (isdigit(*buf)) {
        ret += (*buf - '0');
    } else {
        ret += tolower(*buf) - 'a' + 10;
    }

    buf++;
    }

    return ret;
}


uint64_t 
strtox (const char * nptr, char ** endptr) 
{
    uint64_t ret = 0;
    char * buf = (char *)nptr;

    if (*(buf + 1) == 'x') {
    buf += 2;
    }

    while (isxdigit(*buf)) {
    ret <<= 4;

    if (isdigit(*buf)) {
        ret += (*buf - '0');
    } else {
        ret += tolower(*buf) - 'a' + 10;
    }

    buf++;

    if (endptr) {
        *endptr = buf;
    }
    }

    return ret;
}


size_t 
strspn (const char * s, const char * accept) 
{
    int match = 1;
    int cnt = 0;
    int i = 0;
    int accept_len = strlen(accept);

    while (match) {
    match = 0;

    for (i = 0; i < accept_len; i++) {
        if (s[cnt] == accept[i]) {
        match = 1;
        cnt++;
        break;
        }
    }
    }

    return cnt;
}


size_t 
strcspn (const char * s, const char * reject) 
{
    int match = 0;
    int cnt = 0;
    int i = 0;
    int reject_len = strlen(reject);

    while (!match) {
    for (i = 0; i < reject_len; i++) {
        if (s[cnt] == reject[i]) {
        match = 1;
        break;
        }
    }

    if (!match) {
        cnt++;
    }

    }

    return cnt;
}


char *
strstr (const char * s1, const char * s2)
{
        int l1, l2;

        l2 = strlen(s2);
        if (!l2)
                return (char *)s1;
        l1 = strlen(s1);
        while (l1 >= l2) {
                l1--;
                if (!memcmp(s1, s2, l2))
                        return (char *)s1;
                s1++;
        }
        return NULL;
}


void 
str_tolower (char * s) 
{
    while (isalpha(*s)) {
    if (!islower(*s)) {
        *s = tolower(*s);
    }
    s++;
    }
}


void 
str_toupper (char * s) 
{
    while (isalpha(*s)) {
    if (!isupper(*s)) {
        *s = toupper(*s);
    }
    s++;
    }
}



