#include <nautilus/intrinsics.h>
#include <nautilus/nautilus.h>

extern int printk (const char * fmt, ...);


void __attribute__((noinline))
__do_backtrace (void ** fp, unsigned depth)
{
    if (!fp || fp >= (void**)nk_get_nautilus_info()->sys.mem.phys_mem_avail) {
        return;
    }
    
    printk("[%2u] RIP: %p RBP: %p\n", depth, *(fp+1), *fp);

    __do_backtrace(*fp, depth+1);
}


/*
 * dump memory in 16 byte chunks
 */
void 
nk_dump_mem (void * addr, ulong_t n)
{
    int i, j;
    ulong_t new = (n % 16 == 0) ? n : ((n+16) & ~0xf);

    for (i = 0; i < new/(sizeof(void*)); i+=2) {
        printk("%p: %08p  %08p  ", ((void**)addr + i), *((void**)addr + i), *((void**)addr + i + 1));
        for (j = 0; j < 16; j++) {
            char tmp = *((char*)addr + j);
            printk("%c", (tmp < 0x7f && tmp > 0x1f) ? tmp : '.');
        }

        printk("\n");
    }
}


void 
nk_stack_dump (ulong_t n)
{
    void * rsp = NULL;
    asm volatile ("movq %%rsp, %[_r]" : [_r] "=r" (rsp));

    if (!rsp) {
        return;
    }

    printk("Stack Dump:\n");

    nk_dump_mem(rsp, n);
}


inline void 
print_gprs (void) 
{
    int i = 0;
    char * reg_names[9] = {"RAX", "RBX" , "RCX", "RDX", "RDI", "RSI", "RBP", "RSP", 0};
    void * reg_vals[8];

    asm volatile("movq %%rax, %[m]" : [m] "=m" (reg_vals[0]));
    asm volatile("movq %%rbx, %[m]" : [m] "=m" (reg_vals[1]));
    asm volatile("movq %%rcx, %[m]" : [m] "=m" (reg_vals[2]));
    asm volatile("movq %%rdx, %[m]" : [m] "=m" (reg_vals[3]));
    asm volatile("movq %%rdi, %[m]" : [m] "=m" (reg_vals[4]));
    asm volatile("movq %%rsi, %[m]" : [m] "=m" (reg_vals[5]));
    asm volatile("movq %%rbp, %[m]" : [m] "=m" (reg_vals[6]));
    asm volatile("movq %%rsp, %[m]" : [m] "=m" (reg_vals[7]));
    
    printk("[-------------- Register Contents --------------]\n");
    for (i = 0; i < 4; i++) {
        printk("%s: %8p  %s: %8p\n", reg_names[i], reg_vals[i], reg_names[i+1], reg_vals[i+1]);
    }
}
