#include <nautilus.h>
#include <irq.h>
#include <thread.h>
#include <dev/kbd.h>

#ifndef NAUT_CONFIG_DEBUG_KBD
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

static int 
kbd_handler (excp_entry_t * excp, excp_vec_t vec)
{

    uint8_t status;
    uint8_t scan;
    uint8_t key;
    uint8_t flag;

    status = inb(KBD_CMD_REG);

    io_delay();

    if ((status & STATUS_OUTPUT_FULL) != 0) {
      scan  = inb(KBD_DATA_REG);
      DEBUG_PRINT("Keyboard: status=0x%x, scancode=0x%x\n", status, scan);
      io_delay();

      if (!(scan & KBD_RELEASE)) {
          goto out;
      }

#if NAUT_CONFIG_THREAD_EXIT_KEYCODE == 0xc4
      /* this is a key release */
      if (scan == 0xc4) {
          void * ret = NULL;
          IRQ_HANDLER_END();
          nk_thread_exit(ret);
      }
#endif
      
    }


out:
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

