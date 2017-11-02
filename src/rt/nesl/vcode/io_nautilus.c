#include  <stdarg.h>

#define PROTO_(X) ()

void write_value PROTO_((vb_t*, FILE*)) { return ; } 
void write_string PROTO_((vb_t*, FILE*)) { return ; } 
void do_write PROTO_((prog_entry_t*, FILE*)) { return ; } 
void do_read PROTO_((prog_entry_t* , FILE*)) { return ; } 
void do_fopen PROTO_((prog_entry_t*)) { return ; } 
void do_fclose PROTO_((prog_entry_t*)) { return ; } 
void do_fread PROTO_((prog_entry_t*)) { return ; } 
void do_fwrite PROTO_((prog_entry_t*)) { return ; } 
void do_spawn PROTO_((prog_entry_t*)) { return ; } 
void do_fread_char PROTO_((prog_entry_t*)) { return ; } 


void srandom(unsigned int seed) { return ; }
int getrusage(int who, void *x) { return -1; }

int vsscanf(const char *, const char *, ...);


int __isoc99_sscanf(const char *str, const char *fmt, ...) {
    va_list arg;
    int rc;
    
    va_start(arg,fmt);
    rc=vsscanf(str,fmt,arg);
    va_end(arg);
    return rc;
}


long int random() { 
    static long int x=0;
    return x++;
}
