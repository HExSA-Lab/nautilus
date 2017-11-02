/*
 * Copyright 1992, 1993, 1994, 1995 Carnegie Mellon University
 */

/* Warning: Portability issues led to some uglyness here.  This is the
 * most likely file in which to encounter build problems.  Almost all the
 * system dependencies have been isoloated to this file.
 */

#include <nautilus/nautilus.h>
#include <nautilus/naut_string.h>

//#include <ctype.h>              /* for pseudo parser in do_read */

/* Some of these cray things might be needed for System V in general */
#ifdef cray
#include <sys/types.h>
#include <unistd.h>
#define MAXPATHLEN PATHSIZE
#endif

#if __hpux
#include <sys/unistd.h>
#endif

#if __osf__
#define _OSF_SOURCE
#define _BSD
#include <unistd.h>
#include <sys/types.h>
#endif

#if MACH
#ifndef __PARAGON__
#include <sysent.h>  			
#endif
#endif /* end MACH */

#if CM2
#include <sys/types.h>		/* can't find it on the CM2 */
#endif /* end CM2 */

#if sun 
#include <unistd.h>
#endif	/* end sun */

#if _AIX
#include <unistd.h>
#include <sys/limits.h>
#endif /* end _AIX */

#include <sys/param.h>
#include <sys/file.h>
#include <errno.h>

#ifndef NOSPAWN
#include <sys/wait.h>		/* needed in do_spawn() */
#include <sys/time.h>  		/* needed in do_spawn() */
#include <sys/resource.h>	/* needed in do_spawn() */
#endif /* NOSPAWN */

#include "config.h"
#include "vcode.h"
#include "vstack.h"
#include "program.h"
#include "constant.h"
#include "stack.h"

/* Use vfork() instead of fork(), if it is available */
#if bsd | cray
#define FORK vfork
#else
#define FORK fork
#endif /* bsd */

extern __const char *__const sys_errlist[];
static char err_msg[256];		/* store error messages here */
static int is_error = 0;

/* Most of the io routines return an error flag and message.
 * do this here.  Assume msg is in err_msg, and is_error is true
 * if error occurred.
 */
static void push_io_error PROTO_((void))
{
    if (is_error) {
	int err_len = (int) strlen(err_msg);
	int *err_int = (int *) malloc(err_len *sizeof(int));
	int i;

	if (err_int == NULL) {
	    _fprintf(stderr, "vinterp: io error, can't print error message\n");
	    vinterp_exit(1);
	}

	for (i = 0; i < err_len; i++) {		/* turn err_msg to ints */
	    err_int[i] = (int) err_msg[i];
	}

	stack_push(err_len, Int, NOTSEGD);	/* error to stack */
	assert_mem_size(c2v_fuz_scratch(err_len));
	c2v_fuz(se_vb(TOS)->vector, err_int, err_len, SCRATCH);
	free(err_int);

	stack_push(1, Bool, NOTSEGD);		/* error flag to stack */
	assert_mem_size(rep_vub_scratch(1));
	rep_vub(se_vb(TOS)->vector, 0, (cvl_bool) 0, 1, SCRATCH);
    } else {
	stack_push(0, Int, NOTSEGD);		/* 0 len error message */
	stack_push(1, Bool, NOTSEGD);		/* error flag */
	assert_mem_size(rep_vub_scratch(1));
	rep_vub(se_vb(TOS)->vector, 0, (cvl_bool) 1, 1, SCRATCH);
    }
}

/* write a vector onto a stream, make no change to the stack */
void write_value(vb, stream)
vb_t *vb;
FILE *stream;
{
    int i;
    vb_t *vb_print = NULL;		/* vector to be printed */

    if (vb->type == Segdes) {
	/* need to convert to a lengths representation and then 
	 * print the results of that.
	 */
	vb_t *vb_segdes = new_vector(vb->len, Int, NOTSEGD);

	assert_mem_size(len_fos_scratch(vb->seg_len, vb->len));
	len_fos(vb_segdes->vector, vb->vector, vb->seg_len, vb->len, SCRATCH);

	vb_print = vb_segdes;
	_putc((int)'[', stream);
    } else {
	vb_print = vb;
	_putc('(', stream);
    }

    switch (vb_print->type) {
	case Int:
	case Segdes: {
	    int len = vb_print->len;
	    int *data = (int *)malloc (len * sizeof(int));

	    if (data == NULL) {
		is_error = 1;
		(void) strcpy(err_msg, "couldn't malloc io buffer");
		break;
	    }
	    assert_mem_size(v2c_fuz_scratch(len));
	    v2c_fuz(data, vb_print->vector, len, SCRATCH);

	    for (i = 0; i < len; i++) {
	        _fprintf(stream, " %d", data[i]);
	    }
	    free((char *)data);
	    break;
	}
	case Bool: {
	    int len = vb_print->len;
	    cvl_bool *data = (cvl_bool *)malloc (len * sizeof(cvl_bool));

	    if (data == NULL) {
		is_error = 1;
		strcpy(err_msg, "couldn't malloc io buffer");
		break;
	    }
	    assert_mem_size(v2c_fub_scratch(len));
	    v2c_fub(data, vb_print->vector, len, SCRATCH);

	    for (i = 0; i < len; i++) {
		_putc(' ', stream);
		_putc(data[i] ? 'T' : 'F', stream);
	    }

	    free((char *)data);
	    break;
	}
	case Float: {
	    int len = vb_print->len;
	    double *data = (double *)malloc (len * sizeof(double));

	    if (data == NULL) {
		is_error = 1;
		strcpy(err_msg, "couldn't malloc io buffer");
		break;
	    }
	    assert_mem_size(v2c_fud_scratch(len));
	    v2c_fud(data, vb_print->vector, len, SCRATCH);

	    /* use e format so that output numbers look similar */
	    for (i = 0; i < len; i++) {
	        _fprintf(stream," %.14e", data[i]);
	    }

	    free((char *)data);
	    break;
	}
	default:
	    _fprintf(stderr, "vinterp: internal error: Illegally typed vector.\n");
	    vinterp_exit (1);
    }
    _putc(' ', stream);
    if (vb->type == Segdes) {
	_putc(']', stream);
	vstack_pop(vb_print);		/* vb_print is tmp in segdes case */
    } else {
	_putc(')', stream);
    }

    _putc('\n', stream);

    /* check to see if an error occured */
    if (ferror(stream)) {
	is_error = 1;
	strcpy(err_msg, sys_errlist[errno]);
    } else {
        fflush(stream);
    }
}

/* write a string to stream; do not modify the stack */
void write_string(vb, stream)
vb_t *vb;
FILE *stream;
{
    int len = vb->len;
    int i;
    int *string = (int *) malloc(len * sizeof(int));

    assert(vb->type == Int);

    if (string == NULL) {
	strcpy(err_msg,"couldn't allocate io buffer for string");
	is_error = 1;
	return;
    }

    assert_mem_size(v2c_fuz_scratch(len));
    v2c_fuz(string, vb->vector, len, SCRATCH);

    /* since strings are vectors of ints, and not chars, we can't
     * just write(2) the string
     */
    for (i = 0; i< len; i++) {
	_putc((char)string[i], stream);	/* (void) removed for SP1 */
    }

    /* check to see if an error occured */
    if (ferror(stream)) {
	is_error = 1;
	strcpy(err_msg, sys_errlist[errno]);
    } else {
        fflush(stream);
    }

    free((char *)string);
}

/* perform the WRITE function; write_value() and write_string() do the work.
 * This puts two values on the stack: 
 *    boolean - T if the write was successful, F otherwise
 *    error-string - null if no error, otherwise an error message
 */
void do_write(pe, stream)
prog_entry_t *pe;
FILE *stream;
{
    is_error = 0;

    if (pe->type == String)
	write_string(se_vb(TOS), stream);
    else 
	write_value(se_vb(TOS), stream);

    stack_pop(TOS);			/* remove vector from stack */

    push_io_error();
}

/* Read values of the correct type from stream, storing them in an array.
 * When we're done, we use the c2v CVL calls to create a vector.
 * This function puts three values on the stack: 
 *    boolean - T if the read was successful, F otherwise
 *    error-string - null if no error, otherwise an error message
 *    value vector - value actually read in
 */
void do_read(pe, stream)
prog_entry_t *pe;
FILE *stream;
{
    int count;			        /* total number elements read */
    vec_p vector;			/* where to copy input */
    TYPE type = pe->type;
    TYPE stack_type;			/* type of vector element:
					 * String -> Int */
    int c;				/* char read in */

    int start_vector;			/* must appear as first char */
    int end_vector;			/* must appear as last char */

    int *read_array = NULL;		/* input buffer */
    int input_size_max = 1000;		/* size of buffer */

    extern int isatty();
    is_error = 0;

    /* print out a nice message if working interactively */
    if (isatty(fileno(stream))) {
	char *type_string = NULL, *form_string = NULL;
	switch(type) {
	    case Int: 
		type_string = "integer";
		form_string = "(1 2 3)";
		break;
	    case Float:
		type_string = "float";
		form_string = "(1.0 2.0 3.0)";
		break;
	    case Bool:
		type_string = "boolean";
		form_string = "(T F)";
		break;
	    case String:
		type_string = "string";
		form_string = "\"string\"";
		break;
	    case Segdes:
		type_string = "segment descriptor";
		form_string = "[1 2 3]";
		break;
	    default:
		_fprintf(stderr, "vinterp: internal error: improper type %d in do_read().\n", type);
		vinterp_exit(1);
		break;
	}
        _fprintf(stderr, "vinterp: input %s vector, in form %s: ", type_string, form_string);
    }

    switch (type) {
	case Int: case Float: case Bool:
	    start_vector = '('; end_vector = ')';
	    stack_type = type;
	    break;
	case String: 
	    end_vector = start_vector = '\"';
	    stack_type = Int;
	    break;
	case Segdes:
	    start_vector = '['; end_vector = ']';
	    stack_type = Int;
	    break;
	default:
	    _fprintf(stderr, "vinterp: internal error: improper type %d in do_read().\n", type);
	    vinterp_exit(1);
	    /* NOTREACHED */
	    end_vector = start_vector = '\0'; 	/* make gcc happy */
	    stack_type = Illegal;
	    break;
    }

    /* skip whitespace */
    while ((c = getc(stream)) != EOF && isspace(c))
	;

    /* get start of vector */
    if (c != start_vector) {
	is_error = 1;
	(void) sprintf(err_msg, "vinterp: input error: vector must start with %c", start_vector);
	goto ERROR;
    }

    /* allocate input buffer */
    read_array = (int *)malloc (input_size_max*sizeof(int));
    if (read_array == NULL) {
	is_error = 1;
	strcpy(err_msg, "vinterp: can't malloc read buffer.");
	goto ERROR;
    }

    count = 0;
    /* read routines for each type.  each of these leaves the char that
     * stopped the read (EOF, vector delimiter, etc) in c,
     * and sets count to the number of items read.
     */
    switch (type) {
	case Int: case Segdes:
	    /* write our own integer parser to avoid fscanf overhead */
	    while (1) {			/* read in new integer */
		unsigned int val = 0;	/* int being read */
	        int ispositive = 1;	/* positive or negative */

    		while ((c = getc(stream)) != EOF && isspace(c))
		    ;

		if (isdigit(c)) goto DIGIT;
		else if (c == '-') { ispositive = 0; c = getc(stream); }
		else if (c == '+') { c = getc(stream); }
		else goto DONE;			/* illegal char */

		while (isdigit(c)) {		/* numeral -> number */
DIGIT:		    val = 10*val + (c - '0');
		    c = getc(stream);
		} 

		/* now put number into read buffer */
		if (count >= input_size_max) {
		    input_size_max *= 2;
		    read_array = (int *)realloc((char*)read_array, 
						input_size_max * sizeof(int));
		    if (read_array == NULL) {
			is_error = 1;
			strcpy(err_msg, "vinterp: could not reallocate read buffer.");
			goto ERROR;
		    }
		}
		read_array[count] = ispositive ? val : -val;
		count ++;
	    }
	    break;   	/* int case */
	
	case Bool:
	    while (1) {				/* read in bools */
	    cvl_bool val;
		c = getc(stream);
		switch(c) {
		    case ' ': case '\t': case '\n': continue;
		    case 'T': val = 1; break;
		    case 'F': val = 0; break;
		    default: goto DONE;
		}
		/* put into read buffer */
		if (count*(sizeof(cvl_bool)) >= input_size_max) {
		    input_size_max *= 2;
		    read_array = (int *)realloc((char*)read_array, 
						input_size_max * sizeof(int));
		    if (read_array == NULL) {
			is_error = 1;
			strcpy(err_msg, "vinterp: could not reallocate read buffer.");
			goto ERROR;
		    }
		}
		((cvl_bool*)read_array)[count] = val;
		count ++;
	    }
	    break;

	case String:
	    /* For now, just get a char.  Eventually, we would like '\' 
	     * to do something useful.
	     */
	    while (1) {
	        c = getc(stream);
		if ((c == (int) '\"') || (c != EOF)) goto DONE;

		if (count >= input_size_max) {	/* put into read buffer */
		    input_size_max *= 2;
		    read_array = (int *)realloc((char*)read_array, 
						input_size_max * sizeof(int));
		    if (read_array == NULL) {
			is_error = 1;
			strcpy(err_msg, "vinterp: could not reallocate read buffer.");
			goto ERROR;
		    }
		}
		read_array[count] = c;
		count ++;
	    }
	    break;
	
        case Float:
	    while (1) {
		double val;
		if (fscanf(stream, " %lf", &val) != 1) {
		    c = getc(stream);
		    goto DONE;
		} else {
		    if (count*sizeof(double) >= input_size_max) { 
			input_size_max *= 2;
			read_array = (int *)realloc((char*)read_array, 
						    input_size_max * sizeof(int));
			if (read_array == NULL) {
			    is_error = 1;
			    strcpy(err_msg, "vinterp: could not reallocate read buffer.");
			    goto ERROR;
			}
		    }
		    ((double *)read_array)[count] = val;
		    count ++;
		}
	    }
	    break;
	default:
	    _fprintf(stderr, "vinterp: internal error: improper type %d in do_read().\n", type);
	    vinterp_exit(1);
    } /* end switch */

DONE:
    /* get end of vector */
    if (c != end_vector) {
	is_error = 1;
	(void) sprintf(err_msg, "input error: vector must end with %c", end_vector);
	goto ERROR;
    }

    stack_push(count, stack_type, -1);	/* allocate stack space */
    vector = se_vb(TOS)->vector;
    switch (stack_type) {
	case Int: 			/* this covers Segdes case as well */
	    assert_mem_size(c2v_fuz_scratch(count));
	    c2v_fuz(vector, read_array, count, SCRATCH);
	    break;
	case Bool:
	    assert_mem_size(c2v_fub_scratch(count));
	    c2v_fub(vector, (cvl_bool *)read_array, count, SCRATCH);
	    break;
	case Float:
	    assert_mem_size(c2v_fud_scratch(count));
	    c2v_fud(vector, (double *)read_array, count, SCRATCH);
	    break;
	default: 
	    _fprintf(stderr, "vinterp: illegal type in READ.\n");
	    vinterp_exit (1);
    }

    if (type == Segdes) {
	/* do a MAKE_SEGDES from the length vector */
	int vector_len;
	stack_entry_t len_se = TOS;

	assert_mem_size(add_ruz_scratch(count));
	vector_len = add_ruz(se_vb(len_se)->vector, count, SCRATCH);

	stack_push(count, Segdes, vector_len);

	assert_mem_size(mke_fov_scratch(count, vector_len));
	mke_fov(se_vb(TOS)->vector, se_vb(len_se)->vector, 
		vector_len, count,  SCRATCH);

	stack_pop(len_se);		/* remove the length vector */
    }

    if (is_error) {
ERROR:
	stack_push(0, stack_type, NOTSEGD);	/* null result */
    }

    /* put results of operation on stack */
    push_io_error();

    /* free read_array */
    if (read_array != NULL) free(read_array);
}

/* ----------------------- File I/O ------------------------*/

/* file_list: List of open files and their file descriptors.
 *	      The index of a FILE in file_list will be used by vcode
 *	      to refer to the file.
 *	      We initialize file_list so that stdin, stdout, and stderr
 *	      are already present.
 * NOFILE is max number open files for a process.
 */

static FILE *file_list[NOFILE+1]; /* = {stdin, stdout, stderr, };  */

void init_file_list()
{
  file_list[0] = stdin;
  file_list[1] = stdout;
  file_list[2] = stderr;
}  

/* Get syntx error if these were defined correctly in vcode.h */
#if (STDIN_FD != 0 || STDOUT_FD != 1 || STDERR_FD !=2)
*_FD defined incorrectly in vcode.h
#endif

#define FD_ERROR -2

/* add a file to file_list.  return index into list, -1 if error */
static int fd_insert PROTO_((FILE*));
static int fd_insert(file)
FILE *file;
{
    int fd;

    /* Find first null entry in list */
    file_list[NOFILE] = (FILE *)NULL;		/* sentinel */
    for (fd = 0; file_list[fd] != (FILE *)NULL; fd++)
	;

    if (fd < NOFILE) {
	/* everything ok */
	assert(file_list[fd] == (FILE *)NULL);
	file_list[fd] = file;
    } else {
	/* no room in file table */
	fd = -1;
    }

    return fd;
}

/* get the top of stack and turn it into an fd.
 * Return fd, FD_ERROR otherwise
 * Set is_error and err_msg if there are any problems.
 */
static int fd_from_stack PROTO_((prog_entry_t*));
static int fd_from_stack(pe)
prog_entry_t *pe;
{
    vb_t *fd_vb;				/* file descriptor vector */
    int fd;					/* int form of fd */

    fd_vb = se_vb(TOS);

    /* check type and length */
    if (check_args && (fd_vb->type != Int) && (fd_vb->len != 1) ) {
	_fprintf(stderr, "vinterp: line %d: %s: top of stack not scalar integer.\n",
	       pe->lineno, pe->vopdes->vopname);
	vinterp_exit(1);
    }

    /* get value */
    assert_mem_size(ext_vuz_scratch(1));
    fd = (int) ext_vuz(fd_vb->vector, 0, 1, SCRATCH);
    stack_pop(TOS);		/* remove from stack */

    /* check for validity */
    if ((fd != NULL_STREAM_FD) &&
	(fd < 0 || fd >= NOFILE || file_list[fd] == NULL)) {
	/* invalid: write results to stack */
	sprintf(err_msg, "vinterp: line %d: illegal VCODE file descriptor for %s: %d", pe->lineno, pe->vopdes->vopname, fd);
	is_error = 1;
	fd = FD_ERROR;
    }

    return fd;
}

/* open a file.  Take two arguments off the stack: 
 *    filename - string (must convert CVL string to C string)
 *    mode -     integer describing how to open file: 
 *	1 - open for reading 
 *	2 - create for writing
 *	3 - append on write (write to end of file or create for writing)
 * Puts three values on the stack: 
 *    boolean -          T if open was successful, F is error
 *    error string -     character vector containing error message, empty if
 *                       none
 *    file descriptor -  an integer (index into file_list)
 *
 * We create the opened file so that it is readable and writable by all.
 */

void do_fopen(pe)
prog_entry_t *pe;
{
    vb_t *mode_vb; 				/* mode for opening file */
    int cvl_mode;
    char *open_mode;				/* type arg to fopen(2) */

    vb_t *filename_vb;				/* name of file to open */
    char filename[MAXPATHLEN];			/* string of name */
    int  filename_int[MAXPATHLEN];		/* int vector of name */
    int filename_len;				/* length of filename vector */

    int fd;					/* file descriptor */
    FILE *file;					/* stream returned by fopen */
    int i;

    is_error = 0;

    /* get and (optionally) check type and length of mode */
    mode_vb = se_vb(TOS);
    if (check_args && (mode_vb->type != Int || mode_vb->len != 1)) {
	_fprintf(stderr, "vinterp: line %d: FOPEN: top of stack not integer scalar. \n", pe->lineno);
	vinterp_exit(1);
    }

    assert_mem_size(ext_vuz_scratch(1));	/* get CVL mode off of stack */
    cvl_mode = (int) ext_vuz(mode_vb->vector, 0, 1, SCRATCH);

    /* convert to open_type */
    if (cvl_mode == 1) open_mode = "r";
    else if (cvl_mode == 2) open_mode = "w";
    else if (cvl_mode == 3) open_mode = "a";
    else {
	_fprintf(stderr, "vinterp: line %d: FOPEN: top of stack (%d) not legal mode.\n", pe->lineno, cvl_mode);
	vinterp_exit(1);
	/* NOTREACHED */	
	open_mode = "";			/* make gcc happy */
    }

    /* remove mode from stack */
    stack_pop(TOS);

    /* now work with filename */
    filename_vb = se_vb(TOS);

    /* check type of filename (strings are stored as ints) */
    if (check_args && (mode_vb->type != Int)) {
	_fprintf(stderr, "vinterp: line %d: FOPEN: second argument not string. \n", pe->lineno);
	vinterp_exit(1);
    }

    /* convert filename vector to C string */
    filename_len = filename_vb->len;

    if (filename_len >= MAXPATHLEN) {		/* name too long */
	is_error = 1;
	strcpy(err_msg, "file name too long");
	goto ERROR;
    }

    assert_mem_size(v2c_fuz_scratch(filename_len));
    v2c_fuz(filename_int, filename_vb->vector, filename_len, SCRATCH);
    for (i = 0; i < filename_len; i++) {
	filename[i] = (char) filename_int[i];
    }
    filename[i] = '\0';			/* terminate string */

    stack_pop(TOS);			/* remove file name from stack */

    /* open file */
    file = fopen(filename, open_mode);

    if (file != (FILE *)NULL) {		/* open was successful */
	fd = fd_insert(file);
	if (fd == -1) {			/* couldn't insert fd */
	    is_error = 1;
	    strcpy(err_msg, "no room in internal file table");
	    goto ERROR;
	}
    } else { 				/* error opening file */
	is_error = 1;
	strcpy(err_msg, sys_errlist[errno]);
	goto ERROR;
    }

    if (is_error) {
ERROR:
	fd = -1;			/* dummy illegal value */
    }

    /* put results on stack */
    stack_push(1, Int, -1);		/* file descriptor */
    assert_mem_size(rep_vuz_scratch(1));
    rep_vuz(se_vb(TOS)->vector, 0, fd, 1, SCRATCH);

    push_io_error();
}

/* close a file: Takes a single argument, an integer file descriptor.
 * returns two values: 
 *    boolean - indicating success or failure
 *    string  - empty if successful, error message if failure
 */
void do_fclose(pe)
prog_entry_t *pe;
{
    int fd;					/* index into file_list */
    FILE *file;					/* what to close */

    is_error = 0;
    fd = fd_from_stack(pe);
    if (fd == FD_ERROR || fd < 0 || fd >= NOFILE) goto ERROR;

    file = file_list[fd];			/* get stream from list */

    if (file != (FILE *)NULL) {			/* make sure fd is valid */
	if (fclose(file) != EOF) {		/* success */
	    file_list[fd] = (FILE *)NULL;	/* open up slot */
	} else {
	    is_error = 1;
	    strcpy(err_msg, "vinterp: FCLOSE: could not close file");
	}
    } else {			/* invalid fd */
	is_error = 1;
	strcpy(err_msg, "illegal VCODE file descriptor in FCLOSE");
    }

ERROR:
    push_io_error();
}


/* Read from a file. File descriptor is on top of stack.
 * Returns three values on the stack:
 *   bool - T if read is successful
 *   string - error message if failure in read, null otherwise
 *   vector - result of the read
 * Call do_read() to do the actual read.
 */   
void do_fread(pe)
prog_entry_t *pe;
{
    int fd;				/* index into file_list */
    FILE *file;				/* from file_list */

    is_error = 0;

    fd = fd_from_stack(pe);
    if (fd == FD_ERROR || fd < 0 || fd >= NOFILE) goto ERROR;

    file = file_list[fd];		/* get actual file handler */

    if (file == NULL) {			/* check for validity */
	/* invalid: write results to stack */
	strcpy(err_msg, "illegal VCODE file descriptor for FREAD");
	is_error = 1;

ERROR:	stack_push(0, pe->type, -1);	/* null result */
	push_io_error();
    } else {
	/* do_read does all the work */
        do_read(pe, file);
    }
}


/* Write to a file. File descriptor is on top of stack.
 * Returns two values on the stack:
 *   bool - T if read is successful
 *   string - error message if failure in read, null otherwise
 * Call do_write() to do the actual write
 */   
void do_fwrite(pe)
prog_entry_t *pe;
{
    int fd;
    FILE *file;				/* from file_list */

    is_error = 0;

    fd = fd_from_stack(pe);

    if (fd == FD_ERROR || fd < 0 || fd >= NOFILE) goto ERROR;

    file = file_list[fd];		/* get actual file handler */

    if (file == NULL) {			/* check for validity */
	/* invalid: write results to stack */
	strcpy(err_msg, "illegal VCODE file descriptor in FWRITE");
ERROR:	is_error = 1;
	
	/* remove value to write from stack */
	stack_pop(TOS);
	push_io_error();

    } else {
	do_write(pe, file);		/* do_write does all the work */
    }
}

/* do_spawn() : This allows VCODE to communicate with other C processes.
 * The SPAWN command takes 4 args from the stack:
 *    execution string - a string that will be passed to execvp
 *    in_fd -  a file descriptor -  stdin of new process
 *    out_fd - a file descriptor -  stdout of new process
 *    err_fd - a file descriptor -  stderr of new process
 * Any of these arguments may be the NULL_STREAM_FD, in which case,
 * new streams are created.
 * The command returns three file descriptors, a boolean status flag 
 * and an error message.
 */
/* define NOSPAWN if you want to disallow spawning of processes. We use
 * this for our Web Nesl demo.
 */
void do_spawn(pe)
prog_entry_t *pe;
{
#ifdef NOSPAWN
    /* no spawning allowed: pop all the args, put dummy results, a false
     * status flag and a reasonable error strinf on the stack
     */
    int in_fd, out_fd, err_fd;		/* int form of fds */

    stack_pop(TOS);	/* pop 3 fds and a string */
    stack_pop(TOS);
    stack_pop(TOS);
    stack_pop(TOS);

    in_fd = out_fd = err_fd = -1;	/* dummy return value */

    is_error = 1;
    strcpy(err_msg, "SPAWN not permitted");
    goto ERROR;

#else

    vb_t *command_vb;			/* command to run (vector) */
    char *command;			/* command to run (string) */
    int *command_int;			/* command (int version) */
    int command_len;			/* length of command vector */
    int i;				/* temporary */

    int in_fd, out_fd, err_fd;		/* int form of fds */

    int child_in_fd[2], child_out_fd[2], child_err_fd[2];  /* for pipe */

    char **argv;			/* put command here (malloc) */
    int isParent;			/* return of fork() */
    int pipe_err = 0;			/* set if pipe fails */

    is_error = 0;

    /* get the various fd from the stack */
    err_fd = fd_from_stack(pe);
    out_fd = fd_from_stack(pe);
    in_fd = fd_from_stack(pe);	

    if (is_error) goto ERROR;

    /* now get the command */
    command_vb = se_vb(TOS);
    if (check_args && (command_vb->type != Int)) {
	_fprintf(stderr, "vinterp: line %d: SPAWN: first arg not string.\n", pe->lineno);
	vinterp_exit(1);
    }

    /* convert command vector to C string */
    command_len = command_vb->len;
    command_int = (int *)malloc((command_len) * sizeof(int));
    command = (char *)malloc((command_len+1) * sizeof(char));


    if (command_int == NULL || command == NULL) {
	_fprintf(stderr, "vinterp: line %d: internal error: couldn't allocate command string (len = %d) for SPAWN\n", pe->lineno, command_len);
	vinterp_exit(1);
    }
    assert_mem_size(v2c_fuz_scratch(command_len));
    v2c_fuz(command_int, command_vb->vector, command_len, SCRATCH);

    for (i = 0; i < command_len; i++) {
	command[i] = (char) command_int[i];
    }
    command[command_len] = '\0';	/* null terminate string */

    stack_pop(TOS);			/* remove command name from stack */
    free(command_int);

    /* Before we spawn, clean up any old children that have died */
#if cray  | __svr4__
    /* cray and SVR4 don't have wait3.  probably need to keep track of old pids
     * to clean up before spawns
     */
#else
    {
#if __hpux
	int status;
	int *reserved = NULL;
#else
#if _AIX
	int status;
	struct rusage *reserved = NULL;
#else 
	union wait status;
	struct rusage *reserved = NULL;
#endif /* _AIX */
#endif /* __hpux */
	int pid;
	/* loop as long as wait3 returns a dead child */
	do {
            pid = wait3(&status, WNOHANG, reserved);
	} while (pid > 0);
    }
#endif /* CRAY or SVR4 */

    /* Now we can do the spawning */

    { 
        /* set up argument list for call from command */

	int argc;	/* arg count for subprocess */
	int i;
	int inspace;	/* for command parser */

	/* Must break the command string up into pieces:
	 * Convert each space into a NULL.  Eventually, should
	 * put in a quoting mechanism.
	 * For each new word (space->non-space) put a pointer into argv.
	 */
	argv = (char **) malloc (sizeof (char *) * command_len);
	if (argv == NULL) {
	    _fprintf(stderr, "vinterp: SPAWN: line %d: couldn't allocate argv.\n", pe->lineno);
	    vinterp_exit(1);
	}
	argc = 0;
	inspace = 1;	/* start on space to catch beginning */
	for (i = 0; i < command_len; i++) {
	    if (isspace(command[i])) {
		command[i] = (char) NULL; /* cast added for SP1 */
		inspace = 1;
	    } else if (inspace) {
		/* found new word: went from space to non-space */
		argv[argc++] = command + i;
		inspace = 0;
	    }
	}
	argv[argc] = NULL;
    }
    
    /* For each NULL_STREAM argument, we must set up a pipe
     * so that the two processes can communicate.
     */
    pipe_err = 0;
    if (in_fd == NULL_STREAM_FD) {if (-1 == pipe(child_in_fd)) pipe_err = 1;}
    if (out_fd == NULL_STREAM_FD) {if (-1 == pipe(child_out_fd)) pipe_err = 1;}
    if (err_fd == NULL_STREAM_FD) {if (-1 == pipe(child_err_fd)) pipe_err = 1;}
    
    if (pipe_err) {
	is_error = 1;
	strcpy(err_msg, "SPAWN: error opening pipe");
	goto ERROR;
    }
    isParent = FORK();			/* Fork off a subprocess */

    if (isParent == -1) {
	is_error = 1;
	strcpy(err_msg, "SPAWN: error performing fork:");
	strcat(err_msg, sys_errlist[errno]);
	goto ERROR;
    }

/* macros for ends of pipe */
#define READ_END(fd)	fd[0]
#define WRITE_END(fd)	fd[1]

    if (isParent) { 	/* parent process will be the interpreter */
	/* if NULL_STREAM_FD was used, get the corect end of the pipe,
	 * convert the fd into a FILE *, put it in the file_list,
	 * and close other end.
	 */
	FILE * file = stdin;			/* initialized to non-NULL */
	int close_error = 0;

	if (in_fd == NULL_STREAM_FD) {		/* write to child's stdin */
	    file = fdopen(WRITE_END(child_in_fd), "w");
	    in_fd = fd_insert(file);
	    if (-1 == close(READ_END(child_in_fd))) close_error = 1;
	}
	if (out_fd == NULL_STREAM_FD) {		/* read from child stdout */
	    file = fdopen(READ_END(child_out_fd), "r");
	    out_fd = fd_insert(file);
	    if (-1 == close(WRITE_END(child_out_fd))) close_error = 1;
	}
	if (err_fd == NULL_STREAM_FD) {		/* read from child stderr */
	    file = fdopen(READ_END(child_err_fd), "r");
	    err_fd = fd_insert(file);
	    if (-1 == close(WRITE_END(child_err_fd))) close_error = 1;
	}

	if (in_fd == FD_ERROR || out_fd == FD_ERROR || FD_ERROR == -1) {
	    is_error = 1;
	} else if (close_error) {
	    strcpy(err_msg, "error closing end of pipe during spawn");
	    is_error = 1;
	}
    } else {		/* child will exec the command */
	/* Set up IO strams for child.  If NULL_STREAM was specified, 
	 * make correct end of pipe into appropriate stream, and close other
	 * end.  Otherwise, use supplied stream. 
	 */
	extern int execvp();
	if (in_fd == NULL_STREAM_FD) {		/* read from stdin */
	    if ((-1 == dup2(READ_END(child_in_fd), fileno(stdin))) ||
	        (-1 == close(WRITE_END(child_in_fd))))
	       goto CHILD_ERROR;
	} else if (-1 == dup2(fileno(file_list[in_fd]), fileno(stdin)))
	    goto CHILD_ERROR;

	if (out_fd == NULL_STREAM_FD) {		/* write to stdout */
	    if ((-1 == dup2(WRITE_END(child_out_fd), fileno(stdout))) ||
	        (-1 == close(READ_END(child_out_fd))))
	       goto CHILD_ERROR;
	} else if (-1 == dup2(fileno(file_list[out_fd]), fileno(stdout)))
	    goto CHILD_ERROR;

	if (err_fd == NULL_STREAM_FD) {		/* write to stderr */
	    if ((-1 == dup2(WRITE_END(child_err_fd), fileno(stderr))) ||
	        (-1 == close(READ_END(child_err_fd))))
	       goto CHILD_ERROR;
	} else if (-1 == dup2(fileno(file_list[err_fd]), fileno(stderr)))
	    goto CHILD_ERROR;

	/* use execvp (search through PATH) to start co-process */
	(void) execvp(argv[0], argv);

	/* reach here only on error */
CHILD_ERROR:
	_fprintf(stderr, "vinterp: line %d internal error: execvp failed:\n",
		 pe->lineno);
	_fprintf(stderr, "\t%s\n", sys_errlist[errno]);
	_fprintf(stderr, "\tcommand was %s\n", command);
fflush(stderr);

	/* Call _exit() and not exit, to avoid messing up parent's IO
	 * structures */
	_exit(1);
    }

    /* Possible core leak: if there are errors, these might not get
     * freed.  On the other hand, depending on where the error occurs,
     * they might not have been malloced, so need to free before ERROR
     */
    free(command);
    free(argv);
#endif /* NOSPAWN */

ERROR:
    /* put results on stack */
    stack_push(1, Int, NOTSEGD);		/* err_fd */
    assert_mem_size(rep_vuz_scratch(1));
    rep_vuz(se_vb(TOS)->vector, 0, err_fd, 1, SCRATCH);

    stack_push(1, Int, NOTSEGD);		/* out_fd */
    assert_mem_size(rep_vuz_scratch(1));
    rep_vuz(se_vb(TOS)->vector, 0, out_fd, 1, SCRATCH);

    stack_push(1, Int, NOTSEGD);		/* in_fd */
    assert_mem_size(rep_vuz_scratch(1));
    rep_vuz(se_vb(TOS)->vector, 0, in_fd, 1, SCRATCH);

    push_io_error();
}


/* Read from a character stream. 
 * Takes three args from stack:
 *   stream to read from
 *   int giving maximum number of chars to read (negative means don't worry)
 *   char vector giving list of delimiters (empty means don't worry)
 * Returns three values on the stack:
 *   bool - T if read is successful
 *   string - error message if failure in read, null otherwise
 *   stopping condition - integer = -1 on EOF, -2 if maxchars, else delim
 *   vector - result of the read
 */   

void do_fread_char(pe)
prog_entry_t *pe;
{
    vb_t *fd_vb = se_vb(TOS);		/* get fd from stack */
    int fd;
    FILE *file;				/* from file_list */
    int max_char;			/* max number of chars to read */
    vb_t *max_char_vb;
    vb_t *delim_vb;			/* delimiters for intput */
    int delim_mask[256];
    int i;
    int c;
    int *read_array;			/* input buffer */
    int input_size_max = 1000;		/* size of buffer */
    int stop_status = 0;

    /* check for integer scalar value */
    if (check_args && (fd_vb->type != Int || fd_vb->len != 1)) {
	_fprintf(stderr, "vinterp: line %d: FREAD_CHAR: top of stack not integer scalar. \n", pe->lineno);
	vinterp_exit(1);
    }

    /* get fd off of stack */
    assert_mem_size(ext_vuz_scratch(1));
    fd = (int) ext_vuz(fd_vb->vector, 0, 1, SCRATCH);

    stack_pop(TOS);			/* remove fd from stack */

    file = file_list[fd];		/* get actual file handler */

    if (file == NULL) {			/* check for validity */
	stack_pop(TOS);			/* remove max_char arg */
	stack_pop(TOS);			/* remove delimiter arg */

	stack_push(0, pe->type, -1);	/* null result */
	stack_push(0, Int, -1);		/* null stop status */

	is_error = 1;
	strcpy(err_msg, "illegal VCODE file descriptor in FREAD");
	push_io_error();		/* error flag and string */

	return;
    }

    /* set up max char */
    max_char_vb = se_vb(TOS);		
    if (check_args && (max_char_vb->type != Int || max_char_vb->len != 1)) {
	_fprintf(stderr, "vinterp: line %d: FREAD_CHAR: second element on stack not scalar int.\n", pe->lineno);
	vinterp_exit(1);
    }
    assert_mem_size(ext_vuz_scratch(1));
    max_char = (int) ext_vuz(max_char_vb->vector, 0, 1, SCRATCH);
    if (max_char < 0) max_char = 1 << 30;	/* no limit, essentially */
    stack_pop(TOS);

    /* get delimiters */
    delim_vb = se_vb(TOS);
    if (check_args && (delim_vb->type != Int)) {
	_fprintf(stderr, "vinterp: line %d: FREAD_CHAR: third element on stack not string.\n", pe->lineno);
	vinterp_exit(1);
    }

    /* zero out delim mask */
    for (i = 0; i < 256; i++) delim_mask[i] = 0;

    assert_mem_size(ext_vuz_scratch(1));

    /* create mask */
    if (check_args) {		/* check each element */
	for (i = 0; i < delim_vb->len; i++) {
	    /* might want to do this with v2c instead of ext */
	    unsigned int delim = ext_vuz(delim_vb->vector, i, 1, SCRATCH);
	    if (delim < 256) 
	        delim_mask[delim] = 1;
	    else {
		_fprintf(stderr, "vinterp: line %d: delimiter element %d for FREAD_CHAR is not a legal character. \n", pe->lineno, i);
		vinterp_exit(1);
	    }
	}
    } else {
	for (i = 0; i < delim_vb->len; i++) {
	    unsigned int delim = ext_vuz(delim_vb->vector, i, 1, SCRATCH);
	    assert(delim < 256);
	    delim_mask[delim] = 1;
	}
    }
    stack_pop(TOS);

    /* allocate input buffer */
    read_array = (int *)malloc (input_size_max*sizeof(int));
    if (read_array == NULL) {
	_fprintf(stderr, "vinterp: can't malloc read buffer.\n");
	vinterp_exit(1);
    }

    /* Read chars from file until EOF, delimiter or max_char is found */
    /* i is number of characters successfullly read */
    i = 0;
    while ((i < max_char) && 			/* too many chars */
	   ((c = getc(file)) != EOF) && 	/* EOF */
	   delim_mask[c] == 0) {		/* no delimiter */

	/* check to see if room in buffer, reallocate if not */
	if (i == input_size_max) {
	    input_size_max *= 2;
	    read_array = (int *)realloc((char*)read_array, 
		     	                input_size_max * sizeof(int));
	    if (read_array == NULL) {
	        _fprintf(stderr, "vinterp: could not reallocate read buffer.\n");
	        vinterp_exit(1);
	    }
	}

        read_array[i] = c;
	i++;
    }

    assert(i >= 0);
    assert(i <= max_char);

    /* put string on stack */
    stack_push(i, Int, -1);		/* allocate stack space */
    assert_mem_size(c2v_fuz_scratch(i));
    c2v_fuz(se_vb(TOS)->vector, read_array, i, SCRATCH);

    free(read_array);

    /* figure out stop status */
    if (i == max_char) stop_status = -2;
    else if (c == EOF) stop_status = -1;
    else stop_status = c;

    stack_push(1, Int, -1);		/* put status on stack */
    assert_mem_size(rep_vuz_scratch(1));
    rep_vuz(se_vb(TOS)->vector, 0, stop_status, 1, SCRATCH);

    stack_push(0, Int, -1);     /* put null error string on stack */

    stack_push(1, Bool, -1);	/* bool return code on stack */
    assert_mem_size(rep_vub_scratch(1));
    rep_vub(se_vb(TOS)->vector, 0, (cvl_bool)1, 1, SCRATCH);
}

