/*
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University.
 * Copyright (c) 1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON, THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF
 * THIS SOFTWARE IN ITS "AS IS" CONDITION, AND DISCLAIM ANY LIABILITY
 * OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF
 * THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * modified by Kyle Hale 2014 <kh@u.northwestern.edu>
 */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <doprnt.h>
#include <cga.h>
#include <printk.h>


struct printk_state {
	char buf[PRINTK_BUFMAX];
	unsigned int index;
};


static void
flush (struct printk_state *state)
{
	int i;

	for (i = 0; i < state->index; i++)
		putchar(state->buf[i]);

	state->index = 0;
}


static void
printk_char (char * arg, int c)
{
	struct printk_state *state = (struct printk_state *) arg;

	if (c == '\n')
	{
		state->buf[state->index] = 0;
		puts(state->buf);
		state->index = 0;
	}
	else if ((c == 0) || (state->index >= PRINTK_BUFMAX))
	{
		flush(state);
		putchar(c);
	}
	else
	{
		state->buf[state->index] = c;
		state->index++;
	}
}


static int 
vprintk (const char * fmt, va_list args)
{
	struct printk_state state;

	state.index = 0;
	_doprnt(fmt, args, 0, printk_char, (char *) &state);

	if (state.index != 0)
	    flush(&state);

	/* _doprnt currently doesn't pass back error codes,
	   so just assume nothing bad happened.  */
	return 0;
}


void 
panic (const char * fmt, ...)
{
    va_list arg;

    va_start(arg, fmt);
    vprintk(fmt, arg);
    va_end(arg);

   __asm__ __volatile__ ("cli");
   while(1);
}



/* NOTE: this is only to be used by the serial_print_redirect function! */
int
early_printk (const char *fmt, va_list args)
{
    return vprintk(fmt, args);
}


int
printk (const char *fmt, ...)
{
	va_list	args;
	int err;

	va_start(args, fmt);
	err = vprintk(fmt, args);
	va_end(args);

	return err;
}

