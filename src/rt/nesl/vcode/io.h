/*
* Copyright (c) 1992, 1993, 1994, 1995 Carnegie Mellon University 
*/

#ifndef _IO_H
#define _IO_H 1

extern void write_value PROTO_((vb_t*, FILE*));
extern void write_string PROTO_((vb_t*, FILE*));
extern void do_write PROTO_((prog_entry_t*, FILE*));
extern void do_read PROTO_((prog_entry_t* , FILE*));
extern void do_fopen PROTO_((prog_entry_t*));
extern void do_fclose PROTO_((prog_entry_t*));
extern void do_fread PROTO_((prog_entry_t*));
extern void do_fwrite PROTO_((prog_entry_t*));
extern void do_spawn PROTO_((prog_entry_t*));
extern void do_fread_char PROTO_((prog_entry_t*));

#endif /* _IO_H */
