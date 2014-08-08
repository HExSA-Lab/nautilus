#include <nautilus.h>
#include <irq.h>
#include <dev/kbd.h>

static int 
kbd_handler (excp_entry_t * excp, excp_vec_t vec)
{
    IRQ_HANDLER_END();
    
    printk("kbd interrupt!\n");
    return 0;
}

int
kbd_init (struct naut_info * naut)
{
    printk("Initialize keyboard\n");
    register_irq_handler(1, kbd_handler, NULL);
    return 0;
}

