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

#include <nautilus/naut_string.h>
#include <nautilus/naut_types.h>
#include <nautilus/mm.h>

unsigned char _ctype[] = {
_C,_C,_C,_C,_C,_C,_C,_C,			/* 0-7 */
_C,_C|_S,_C|_S,_C|_S,_C|_S,_C|_S,_C,_C,		/* 8-15 */
_C,_C,_C,_C,_C,_C,_C,_C,			/* 16-23 */
_C,_C,_C,_C,_C,_C,_C,_C,			/* 24-31 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,			/* 32-39 */
_P,_P,_P,_P,_P,_P,_P,_P,			/* 40-47 */
_D,_D,_D,_D,_D,_D,_D,_D,			/* 48-55 */
_D,_D,_P,_P,_P,_P,_P,_P,			/* 56-63 */
_P,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U,	/* 64-71 */
_U,_U,_U,_U,_U,_U,_U,_U,			/* 72-79 */
_U,_U,_U,_U,_U,_U,_U,_U,			/* 80-87 */
_U,_U,_U,_P,_P,_P,_P,_P,			/* 88-95 */
_P,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L,	/* 96-103 */
_L,_L,_L,_L,_L,_L,_L,_L,			/* 104-111 */
_L,_L,_L,_L,_L,_L,_L,_L,			/* 112-119 */
_L,_L,_L,_P,_P,_P,_P,_C,			/* 120-127 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,		/* 128-143 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,		/* 144-159 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,   /* 160-175 */
_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,       /* 176-191 */
_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,       /* 192-207 */
_U,_U,_U,_U,_U,_U,_U,_P,_U,_U,_U,_U,_U,_U,_U,_L,       /* 208-223 */
_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,       /* 224-239 */
_L,_L,_L,_L,_L,_L,_L,_P,_L,_L,_L,_L,_L,_L,_L,_L};      /* 240-255 */


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
memcmp (const void * s1_, const void * s2_, size_t n) 
{
    const char * s1 = s1_;
    const char * s2 = s2_;

    while (n > 0) {

        int cmp = (*s1 - *s2);

        if (cmp != 0) {
            return cmp;
        }

        ++s1;
        ++s2;
        --n;
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
strdup (const char * s)
{
    const unsigned len = strlen(s) + 1;
    char * new = malloc(len);
    memcpy(new, s, len);
    return new;
}

char *
strpbrk (const char * cs, const char * ct)
{
	const char *sc1, *sc2;

	for (sc1 = cs; *sc1 != '\0'; ++sc1) {
		for (sc2 = ct; *sc2 != '\0'; ++sc2) {
			if (*sc1 == *sc2)
				return (char *)sc1;
		}
	}
	return NULL;
}


/**
 * strsep - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * strsep() updates @s to point after the token, ready for the next call.
 *
 * It returns empty tokens, too, behaving exactly like the libc function
 * of that name. In fact, it was stolen from glibc2 and de-fancy-fied.
 * Same semantics, slimmer shape. ;)
 */
char *
strsep (char ** s, const char * ct)
{
	char *sbegin = *s;
	char *end;

	if (sbegin == NULL)
		return NULL;

	end = strpbrk(sbegin, ct);
	if (end)
		*end++ = '\0';
	*s = end;
	return sbegin;
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

long int
atol (const char *nptr)
{
  return strtol (nptr, (char **) NULL, 10); 
}


extern long simple_strtol(const char*, char**, unsigned int);
long 
strtol (const char * str, char ** endptr, int base)
{
    return simple_strtol(str, endptr, base);
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

static void reverse(char *str, int n)
{
    int i;
    char temp;

    for (i=0;i<n/2;i++) {
	temp = str[i];
	str[i] = str[n-i-1];
	str[n-i-1] = temp;
    }
}

int ultoa(unsigned long x, char *buf, int numdigits)
{
    int i;

    for (i=0; i==0 || x  ; i++, x=x/10) {
	buf[i] = '0' + (x%10);
    }
    
    if (numdigits) {
	for (; i<numdigits; i++) {
	    buf[i] = '0';
	}
    }
    reverse(buf,i);
    buf[i] = 0;
    return i;
}

int ltoa(long x, char *buf, int numdigits)
{
    if (x<0) {
	*buf++ = '-';
	return 1+ultoa((unsigned long)(-x),buf,numdigits);
    } else {
	return ultoa((unsigned long)(x),buf,numdigits);
    }
}

int utoa(unsigned x, char *buf, int numdigits)
{
    return ultoa((unsigned long)x,buf,numdigits);
}

int itoa(int x, char *buf, int numdigits)
{
    return ltoa((long)x,buf,numdigits);
}

int ustoa(unsigned short x, char *buf, int numdigits)
{
    return ultoa((unsigned long)x,buf,numdigits);
}
int stoa(short x, char *buf, int numdigits)
{
    return ltoa((long)x,buf,numdigits);
}

int uctoa(unsigned char x, char *buf, int numdigits)
{
    return ultoa((unsigned long)x,buf,numdigits);
}

int ctoa(char x, char *buf, int numdigits)
{
    return ltoa((long)x,buf,numdigits);
}


//For Lua Support
char *
__strtok_r(char *s, const char *delim, char **last)
{
	char *spanp, *tok;
	int c, sc;

	if (s == NULL && (s = *last) == NULL)
		return (NULL);

	/*
	 * 	 * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
	 * 	 	 */
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return (NULL);
	}
	tok = s - 1;

	/*
	 * 	 * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
	 * 	 	 * Note that delim must have one NUL; we stop if we see that, too.
	 * 	 	 	 */
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = '\0';
				*last = s;
				return (tok);
			}
		} while (sc != 0);
	}
	/* NOTREACHED */
}

char *
strtok(char *s, const char *delim)
{
	static char *last;

	return (__strtok_r(s, delim, &last));
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

char *
strcasestr (const char * s1, const char * s2)
{
        int l1, l2;

        l2 = strlen(s2);
        if (!l2)
                return (char *)s1;
        l1 = strlen(s1);
        while (l1 >= l2) {
                l1--;
                if (!strncasecmp(s1, s2, l2))
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


void *memchr(const void *str, int c, size_t n)
{
    while (n) {
	if (*((const char *)str)==c) {
	    return (void*)str;
	}
	str++;
	n--;
    }

    return 0;
}



