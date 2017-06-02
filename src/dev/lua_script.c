
#include <nautilus/libccompat.h>
char* read_lua_script()
{
    int i=0;
    uint64_t file_len;
    extern int __LUA_SCRIPT_START, __LUA_SCRIPT_END;
    char *data;
    file_len = (((uint64_t)(&__LUA_SCRIPT_END)) - ((uint64_t)(&__LUA_SCRIPT_START)));
    
    char *buf = (char *)malloc(file_len);
    data = &__LUA_SCRIPT_START;

    for(i=0;i<file_len;i++) 
    {
//    printk("%c",*data);
	    buf[i]=*data;
	    data++;
    }
    buf[i]=0;

    return buf;
}

