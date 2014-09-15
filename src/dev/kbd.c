#include <nautilus.h>
#include <irq.h>
#include <dev/kbd.h>

#ifndef NAUT_CONFIG_DEBUG_KBD
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

static int 
kbd_handler (excp_entry_t * excp, excp_vec_t vec)
{

    uint8_t status, scan;

    status = inb(KBD_CMD_REG);

    io_delay();

    if ((status & STATUS_OUTPUT_FULL) != 0) {
      scan  = inb(KBD_DATA_REG);
      DEBUG_PRINT("Keyboard: status=0x%x, scancode=0x%x\n", status, scan);
      io_delay();
    }


    IRQ_HANDLER_END();
    return 0;
}

int
kbd_init (struct naut_info * naut)
{
    printk("Initialize keyboard\n");
    register_irq_handler(1, kbd_handler, NULL);
    return 0;
}

