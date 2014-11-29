#include <nautilus/nautilus.h>
#include <nautilus/fmtout.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <dev/serial.h>


extern int vprintk(const char * fmt, va_list args);

static spinlock_t serial_lock; /* for SMP */
static uint8_t serial_device_ready = 0;
uint16_t serial_io_addr = 0;
uint_t serial_print_level;

static void reboot (void) 
{
    uint8_t good = 0x02;
    while (good & 0x02) {
        good = inb(0x64);
    }
    outb(0xfe, 0x64);
    halt();
}


static int 
serial_irq_handler (excp_entry_t * excp,
                    excp_vec_t vec)
{
  char rcv_byte;
  char irq_id;

  irq_id = inb(serial_io_addr + 2);

  if ((irq_id & COM1_IRQ) != 0) {
    rcv_byte = inb(serial_io_addr + 0);

    if (rcv_byte == 'k') {
      serial_print("Rebooting Machine\n");
      reboot();
      //machine_real_restart();
    }

    if (rcv_byte == 'p') {
      serial_print("Manually invoking panic\n");
      panic();
    }
      
  }

  IRQ_HANDLER_END();

  return 0;
}


void 
serial_init_addr (uint16_t io_addr) 
{
  serial_io_addr = io_addr;

  //printk("Initializing Polled Serial Output on COM1 - 115200 N81 noflow\n");

  //  io_adr = 0x3F8;	/* 3F8=COM1, 2F8=COM2, 3E8=COM3, 2E8=COM4 */
  outb(0x80, io_addr + 3);

  // 115200 /* 115200 / 12 = 9600 baud */
  outb(1, io_addr + 0);
  outb(0, io_addr + 1);

  /* 8N1 */
  outb(0x03, io_addr + 3);

  /* all interrupts disabled */
  //  outb(0, io_addr + 1);
  outb(0x01, io_addr + 1);

  /* turn off FIFO, if any */
  outb(0, io_addr + 2);

  /* loopback off, interrupts (Out2) off, Out1/RTS/DTR off */
  //  outb(0, io_addr + 4);
  
  // enable interrupts (bit 3)
  outb(0x08, io_addr + 4);
}


void 
serial_putchar (uchar_t c)
{
    //  static unsigned short io_adr;
    if (serial_io_addr==0) { 
        return;
    }

    if (c == '\n') { 

        /* wait for transmitter ready */
        while( (inb(serial_io_addr + 5) & 0x40) == 0);

        /* send char */
        outb('\r', serial_io_addr + 0);

        /* wait for transmitter ready */
    }

    while( (inb(serial_io_addr + 5) & 0x40) == 0);

    /* send char */
    outb(c, serial_io_addr + 0);
}


void 
serial_putlnn (char * line, int len) 
{
  int i;

  for (i = 0; i < len && line[i] != 0; i++) { 
    serial_putchar(line[i]); 
  }

}


void 
serial_putln (char * line) 
{
  int i;

  for (i = 0; line[i] != 0; i++) { 
    serial_putchar(line[i]); 
  }

}


void 
serial_print_hex (uchar_t x)
{
  uchar_t z;
  
  z = (x >> 4) & 0xf;
  serial_print("%x", z);

  z = x & 0xf;
  serial_print("%x", z);
}


void 
serial_mem_dump (uint8_t * start, int n)
{
    int i, j;

    for (i = 0; i < n; i += 16) {

        serial_print("%8x", (ulong_t)(start + i));

        for (j = i; j < i + 16 && j < n; j += 2) {

            serial_print(" ");
            serial_print_hex(*((uchar_t *)(start + j)));
            if ((j + 1) < n) { 
                serial_print_hex(*((uchar_t *)(start + j + 1)));
            }

        }

        serial_print(" ");

        for (j=i; j<i+16 && j<n;j++) {
            serial_print("%c", ((start[j] >= 32) && (start[j] <= 126)) ? start[j] : '.');
        }
        serial_print("\n");

    }

}


static struct Output_Sink serial_output_sink;

static void 
Serial_Emit (struct Output_Sink * o, int ch) 
{ 
  serial_putchar((uchar_t)ch); 
}

static void 
Serial_Finish (struct Output_Sink * o) { return; }


void 
__serial_print (const char * format, va_list ap) 
{
    uint8_t flags;

    if (serial_device_ready) {
        flags = spin_lock_irq_save(&serial_lock);
        Format_Output(&serial_output_sink, format, ap);
        spin_unlock_irq_restore(&serial_lock, flags);
    } else {
        vprintk(format, ap);
    }
}


void
serial_print_redirect (const char * format, ...) 
{
    va_list args;
    uint8_t iflag = irq_disable_save();

    va_start(args, format);
    if (serial_device_ready) {
        __serial_print(format, args);
    } else {
        early_printk(format, args);
    }
    va_end(args);

    irq_enable_restore(iflag);
}

void 
panic_serial (const char * fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (serial_device_ready) {
        __serial_print(fmt, args);
    } else {
        early_printk(fmt, args);
    }
    va_end(args);

   __asm__ __volatile__ ("cli");
   while(1);
}


void 
serial_print (const char * format, ...)
{
  va_list args;
  uint8_t iflag = irq_disable_save();

  __serial_print(format, args);
  va_end(args);

  irq_enable_restore(iflag);
}


void 
serial_print_list (const char * format, va_list ap) 
{
  uint8_t iflag = irq_disable_save();
  __serial_print(format, ap);
  irq_enable_restore(iflag);
}


void 
serial_printlevel (int level, const char * format, ...) 
{
  if (level > serial_print_level) {
    va_list args;
    uint8_t iflag = irq_disable_save();
    
    va_start(args, format);
    __serial_print(format, args);
    va_end(args);
    
    irq_enable_restore(iflag);   
  }
}


void 
serial_init (void) 
{
  serial_print_level = SERIAL_PRINT_DEBUG_LEVEL;

  printk("Initialzing serial device\n");

  spinlock_init(&serial_lock);

  serial_output_sink.Emit = &Serial_Emit;
  serial_output_sink.Finish = &Serial_Finish;

  register_irq_handler(COM1_IRQ, serial_irq_handler, NULL);
  serial_init_addr(DEFAULT_SERIAL_ADDR);

  serial_device_ready = 1;
}
