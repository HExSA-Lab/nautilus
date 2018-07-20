/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2017, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *         Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 *
 * Parts of this file were taken from the GeekOS teaching OS 
 */
#include <nautilus/nautilus.h>
#include <nautilus/fmtout.h>
#include <nautilus/idt.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/shutdown.h>
#include <nautilus/dev.h>
#include <nautilus/chardev.h>
#include <dev/serial.h>
#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
#include <nautilus/gdb-stub.h>
#endif

/*
  The serial driver provides two stages of functionality
  
  The default configured port is used to provide early output
  during the boot process, well before the device framework is
  up and running.   This is achieved by using serial_early_init()
  and then the various serial functions. 

  Once we are past the early stage, serial_init() is then invoked.
  This in turn initializes all UARTs at the legacy addresses (e.g., 
  COM1 through COM4) and make them available via the char dev framework.
  It will also promote the early-inited serial port to this functionality
  while leaving the serial_ functions active as well. 

  Additional serial devices can be initialized via serial_init_one().

  We expect that every serial port is at least a 16550. 
  We run all serial ports at 115200 N81. 

  The serial driver additionally supports a single optional remote debugging
  port.  This port is handled in polling fashion with the exception that
  any receive interrupt or line status (e.g., break) interrupt is interpretted
  as being an interrupt request from the debugger,and will trigger an immediate
  breakpoint. 
  
*/

#define COM1_3_IRQ 4
#define COM2_4_IRQ 3
#define COM1_ADDR 0x3F8
#define COM2_ADDR 0x2F8
#define COM3_ADDR 0x3E8
#define COM4_ADDR 0x2E8



/* This state supports early output */
/* the lock is reused for late output to serialize lines / chars */
static spinlock_t serial_lock; /* for SMP */
static uint8_t serial_device_ready = 0;
static uint16_t serial_io_addr = 0;
static uint_t serial_print_level;
static uint8_t com_irq;
static struct serial_state *early_dev = 0;


#ifdef NAUT_CONFIG_SERIAL_DEBUGGER
/* 
  This state supports polled operation for communication with
  a remote debugger
*/
static uint16_t   debug_io_addr = 0;
static uint8_t    debug_irq;

#endif



/* The following state is for late output */

#define BUFSIZE 512
  
struct serial_state {
    struct nk_char_dev *dev;
    int         mmio;   // if this is a memory-mapped UART
    uint8_t     irq;    // irq
    uint64_t    addr;   // base address
    spinlock_t  input_lock;
    spinlock_t  output_lock;
    uint32_t    input_buf_head, input_buf_tail;
    uint8_t     input_buf[BUFSIZE];
    uint32_t    output_buf_head, output_buf_tail;
    uint8_t     output_buf[BUFSIZE];
};


// for com1..4
static struct serial_state legacy[4];

static int serial_do_get_characteristics(void * state, struct nk_char_dev_characteristics *c)
{
    memset(c,0,sizeof(*c));
    return 0;
}

static int serial_input_empty(struct serial_state *s)
{
    return s->input_buf_head == s->input_buf_tail;
}

static int serial_output_empty(struct serial_state *s)
{
    return s->output_buf_head == s->output_buf_tail;
}

static int serial_input_full(struct serial_state *s)
{
    return ((s->input_buf_tail + 1) % BUFSIZE) == s->input_buf_head;
}

static int serial_output_full(struct serial_state *s) 
{
    return ((s->output_buf_tail + 1) % BUFSIZE) == s->output_buf_head;
}

static void serial_input_push(struct serial_state *s, uint8_t data)
{
    s->input_buf[s->input_buf_tail] = data;
    s->input_buf_tail = (s->input_buf_tail + 1) % BUFSIZE;
}

static uint8_t serial_input_pull(struct serial_state *s)
{
    uint8_t temp = s->input_buf[s->input_buf_head];
    s->input_buf_head = (s->input_buf_head + 1) % BUFSIZE;
    return temp;
}

static void serial_output_push(struct serial_state *s, uint8_t data)
{
    s->output_buf[s->output_buf_tail] = data;
    s->output_buf_tail = (s->output_buf_tail + 1) % BUFSIZE;
}


static uint8_t serial_output_pull(struct serial_state *s)
{
    uint8_t temp = s->output_buf[s->output_buf_head];
    s->output_buf_head = (s->output_buf_head + 1) % BUFSIZE;
    return temp;
}

static int serial_do_read(void *state, uint8_t *dest)
{
    struct serial_state *s = (struct serial_state *)state;

    int rc = -1;
    int flags;

    flags = spin_lock_irq_save(&s->input_lock);

    if (serial_input_empty(s)) {
	rc = 0;
	goto out;
    } 

    *dest = serial_input_pull(s);
    rc = 1;
    
 out:
    spin_unlock_irq_restore(&s->input_lock, flags);
    return rc;
}


static void kick_output(struct serial_state *s);

static void serial_putchar_early (uchar_t c);

static int serial_do_write(void *state, uint8_t *src)
{
    struct serial_state *s = (struct serial_state *)state;

    int rc = -1;
    int flags;
 
    flags = spin_lock_irq_save(&s->output_lock);

    if (serial_output_full(s)) {
	rc = 0;
	goto out;
    } 

    serial_output_push(s,*src);
    rc = 1;
    
 out:
    kick_output(s);
    spin_unlock_irq_restore(&s->output_lock, flags);
    return rc;
}

static void serial_do_write_wait(void *state, uint8_t *src)
{
    while (serial_do_write(state,src)==0) {}
}


static int serial_do_status(void *state)
{
    struct serial_state *s = (struct serial_state *)state;

    int rc = 0;
    int flags;

    flags = spin_lock_irq_save(&s->input_lock);
    if (!serial_input_empty(s)) {
	rc |= NK_CHARDEV_READABLE;
    }
    spin_unlock_irq_restore(&s->input_lock, flags);

    flags = spin_lock_irq_save(&s->output_lock);
    if (!serial_output_full(s)) {
	rc |= NK_CHARDEV_WRITEABLE;
    }
    spin_unlock_irq_restore(&s->output_lock, flags);
    
    return rc;
}


static struct nk_char_dev_int chardevops = {
    .get_characteristics = serial_do_get_characteristics,
    .read = serial_do_read,
    .write = serial_do_write,
    .status = serial_do_status
};


static void serial_write_reg(struct serial_state *s, uint8_t offset, uint8_t val)
{
    if (s) { 
	if (s->mmio) { 
	    *(volatile uint8_t *)(s->addr + offset) = val;
	} else {
	    outb(val, (uint16_t) (s->addr + offset));
	}
    } else {
	outb(val, serial_io_addr + offset);
    }
}

static uint8_t serial_read_reg(struct serial_state *s, uint8_t offset)
{
    if (s) { 
	if (s->mmio) { 
	    return *(volatile uint8_t *)(s->addr + offset);
	} else {
	    return inb((uint16_t) (s->addr + offset));
	}
    } else {
	return inb(serial_io_addr + offset);
    }
}

#define DLL  0   // divisor latch low
#define DLM  1   // divisor latch high
#define RBR  0   // read data
#define THR  0   // write data
#define IER  1   // interrupt enable
#define IIR  2   // interrupt identify (read)
#define FCR  2   // FIFO control (write)
#define LCR  3   // line control
#define MCR  4   // modem control
#define LSR  5   // line status
#define MSR  6   // modem status
#define SCR  7   // scratch

#define USE_FIFOS 1

// return -1 => error
// return 0  => success
// return 1  => does not exist
static int serial_setup(struct serial_state *s)
{
#ifndef NAUT_CONFIG_GEM5
    // this is broken in GEM5's emulation, actually causing it to crash...
    // We force serial0 and nothing else
  
    // check for existence by changing the scratchpad reg
    serial_write_reg(s,SCR,0xde);
    if (serial_read_reg(s,SCR) != 0xde) { 
	return 1;
    }
#endif

    // line control register
    // set DLAB so we can write divisor
    serial_write_reg(s,LCR,0x80);

    // write divisor to divisor latch to set speed
    // LSB then MSB
    // 115200 / 1 = 115200 baud 
    serial_write_reg(s,DLL,1);
    serial_write_reg(s,DLM,0);

    // line control register
    // turn DLAB off, set
    // 8 bit word, 1 stop bit, no parity
    serial_write_reg(s,LCR,0x03);

    // interrupt enable register
    // raise interrupts on received data available
    // start off not getting interrupt on transmit hoilding register empty
    // this is turned on/off in kick_output()
    // ignore line status update and modem status update
    serial_write_reg(s,IER,0x01);

#if !USE_FIFOS
    // FIFO control register
    // turn off FIFOs;  chip is now going to raise an
    // interrupt on every incoming word
    serial_write_reg(s,FCR,0);
#else
    // FIFO control register
    // turn on FIFOs;  chip is now going to raise an
    // interrupt on every 14 bytes or when 4 character
    // times have passed without getting a read despite there
    // being a character available
    // 1100 0001
    serial_write_reg(s,FCR,0xc1);
#endif
    return 0;
}  

static int serial_irq_handler (excp_entry_t * excp, excp_vec_t vec, void *state);


// 0 = success, -1 = fail, +1 = does not exist
static int serial_init_one(char *name, uint64_t addr, uint8_t irq, int mmio, struct serial_state *s)
{
    int rc;

    memset(s,0,sizeof(*s));
    s->mmio = mmio;
    s->irq = irq;
    s->addr = addr;
    spinlock_init(&s->input_lock);
    spinlock_init(&s->output_lock);

    if ((rc = serial_setup(s))) { 
	memset(s,0,sizeof(*s));
	return rc;
    }

    register_irq_handler(irq, serial_irq_handler, s);
    
    s->dev = nk_char_dev_register(name,0,&chardevops,s);

    if (!s->dev) { 
	return -1;
    } 

    nk_unmask_irq(irq);

    return 0;
}

// assumes this is being done while lock held
static void kick_output(struct serial_state *s)
{
    uint64_t count=0;

    while (!serial_output_empty(s)) { 
	uint8_t ls =  serial_read_reg(s,LSR);
	if (ls & 0x20) { 
	    // transmit holding register is empty
	    // drive a byte to the device
	    uint8_t data = serial_output_pull(s);
	    serial_write_reg(s,THR,data);
	    count++;
	} else {
	    // chip is full, stop sending to it
	    // but since we have more data, have it
	    // interrupt us when it has room
	    uint8_t ier = serial_read_reg(s,IER);
	    ier |= 0x2;
	    serial_write_reg(s,IER,ier);
	    goto out;
	}
    }
    
    // the chip has room, but we have no data for it, so
    // disable the transmit interrupt for now
    uint8_t ier = serial_read_reg(s,IER);
    ier &= ~0x2;
    serial_write_reg(s,IER,ier);
 out:
    if (count>0) { 
	nk_dev_signal((struct nk_dev*)(s->dev));
    }
    return;
}

// assumes this is being done while lock held
static void kick_input(struct serial_state *s)
{
    uint64_t count=0;
    
    while (!serial_input_full(s)) { 
	uint8_t ls =  serial_read_reg(s,LSR);
	if (ls & 0x04) {
	    // parity error, skip this byte
	    continue;
	}
	if (ls & 0x08) { 
	    // framing error, skip this byte
	    continue;
	}
	if (ls & 0x10) { 
	    // break interrupt, but we do want this byte
	}
	if (ls & 0x02) { 
	    // overrun error - we have lost a byte
	    // but we do want this next one
	}
	if (ls & 0x01) { 
	    // data ready
	    // grab a byte from the device if there is room
	    uint8_t data = serial_read_reg(s,RBR);
	    serial_input_push(s,data);
	    count++;
	} else {
	    // chip is empty, stop receiving from it
	    break;
	}
    }
    if (count>0) {
	nk_dev_signal((struct nk_dev *)s->dev);
    }
}




extern int vprintk(const char * fmt, va_list args);




// Commands are of the form ~~~K
// cmd_state counts how many ~s we have seen so far
static int cmd_state = 0; 

static void reset_cmd_fsm() 
{
    cmd_state = 0;
}

static int drive_cmd_fsm(char c)
{
    if (c=='~') { 
	cmd_state++;
	if (cmd_state==4) { 
	    // dump the ~s we have seen
	    DEBUG_PRINT("~~~");
	    reset_cmd_fsm();
	    return 0;
	} else {
	    return 1;
	}
    } else {
	if (cmd_state==3) { 
	    switch (c) {
	    case 'k' :
		DEBUG_PRINT("Rebooting Machine\n");
		reboot();
		return 1;
		break;
	    case 'p' :
		DEBUG_PRINT("Manually invoking panic\n");
		panic();
		return 1;
		break;
	    case 's' :
		DEBUG_PRINT("Shutting down machine\n");
		acpi_shutdown();
		return 1;
		break;
	    default:
		// not a command; the user typed ~~~somethingelse
		// this would queue up ~~~ 
		DEBUG_PRINT("User typed \"~~~\"\n");
		reset_cmd_fsm();
		return 0;
		break;
	    }
	} else {
	    return 0;
	}
    }
}
    

static int 
serial_irq_handler_early (excp_entry_t * excp,
			  excp_vec_t vec,
			  void *state)
{
  char rcv_byte;
  char irr;
  char id;


  // Note that the DEBUG_PRINT statements here
  // are dangerous since they can do serial output
  // themselves, which introduces a race... 
  // for production, these need to be removed

  DEBUG_PRINT("serial_irq_handler\n");

  irr = inb(serial_io_addr + 2);

  DEBUG_PRINT("irr=0x%x\n",irr);
  
  id = irr & 0xf;
  
  DEBUG_PRINT("id=0x%x\n",id);
  
  switch (id) {
  case 0: 
      DEBUG_PRINT("Modem status change\n");
      goto out;
      break;
  case 1:
      DEBUG_PRINT("No interrupt reason\n");
      goto out;
      break;
  case 2:
      DEBUG_PRINT("Transmit holding register empty\n");
      goto out;
      break;
  case 4:
      DEBUG_PRINT("Received data available\n");

      rcv_byte = inb(serial_io_addr + 0);
      DEBUG_PRINT("Received data: '%c'\n",rcv_byte);
      
      if (!drive_cmd_fsm(rcv_byte)) { 
	  // this would enqueue the byte into a recv queue
	  DEBUG_PRINT("char '%c' received\n",rcv_byte);
      }
      goto out;
      break;
  case 6:
      DEBUG_PRINT("Receiver line status change\n");
      goto out;
      break;
  case 12:
      DEBUG_PRINT("Character timeout (FIFO)\n");
      goto out;
      break;
  default:
      DEBUG_PRINT("Unknown interrupt id 0x%x\n",id);
      goto out;
      break;
  }


  out:

  DEBUG_PRINT("Handler end\n");

  IRQ_HANDLER_END();

  return 0;
}


#ifdef NAUT_CONFIG_SERIAL_DEBUGGER
static int  debug_irq_handler(excp_entry_t * excp, excp_vec_t vec, void *state)
{
	char iir;
	char id;
	char lsr;
	
	iir = inb(debug_io_addr + IIR);
	
	id = iir & 0xf;
	
	// we care only about receive and LSR->break
	
	switch (id) {
	case 4:
		// receive data - gdb handler will absorb the data
        // 0x1ULL => come back to us
#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
        nk_gdb_handle_exception(excp, vec, 0, (void*) 0x1ULL);
#endif    
		break;
    
	case 6:
		// LSR
		lsr = inb(debug_io_addr + LSR);
		
		if (lsr & 0x10) {
#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
            // BREAK = invoke gdb handler
            // 0x1ULL => come back to us
            nk_gdb_handle_exception(excp, vec, 0, (void*)0x1ULL);
#endif
		}
		// we don't care about any other condition
		
		break;
		
	default:
		// we don't care
		break;
	}
	
	IRQ_HANDLER_END();
	
	return 0;
}

#endif

static int serial_irq_handler_late(struct serial_state *s,excp_entry_t * excp,excp_vec_t vec, void *state)
{
    uint8_t iir;
    int done = 0;
    
    do {
	iir = serial_read_reg(s,IIR);

	switch (iir & 0xf)  {
	case 0: // modem status reset + ignore
	    (void)serial_read_reg(s,MSR);
	    break;
	case 2: // THR empty (can send more data)
	    spin_lock(&s->output_lock);
	    kick_output(s);
	    spin_unlock(&s->output_lock);
	    break;
	case 4:  // received data available 
	case 12: // received data available (FIFO timeout)
	    spin_lock(&s->input_lock);
	    kick_input(s);
	    spin_unlock(&s->input_lock);
	    break;
	case 6: // line status reset + ignore
	    (void)serial_read_reg(s,LSR);
	    break;
	case 1:   // done
	    spin_lock(&s->output_lock);
	    kick_output(s);
	    spin_unlock(&s->output_lock);
	    spin_lock(&s->input_lock);
	    kick_input(s);
	    spin_unlock(&s->input_lock);
	    break;
	default:  // wtf
	    break;
	}

    } while ((iir & 0xf) != 1);
    
    return 0;
}



static int 
serial_irq_handler(excp_entry_t * excp,
		   excp_vec_t vec,
		   void *state)
{
    int i;
    int rc=0;

#ifdef NAUT_CONFIG_SERIAL_REDIRECT 
    if (!early_dev) { 
	// use the old handler until we have full functionality
	return serial_irq_handler_early(excp,vec,state);
    } 
#endif

    // the following is disgusting...  we don't know which serial device
    // interrupted, so we will check all legacy ones...
    
    for (i=0;i<4;i++) { 
	if (legacy[i].addr) { 
	    // valid device
	    rc |= serial_irq_handler_late(&legacy[i], excp, vec, state);
	}
    }

    // other devices?  Not yet.  
    
    IRQ_HANDLER_END();

    return rc;
}

static void 
serial_init_addr (uint16_t io_addr, int debugger) 
{
  if (!debugger) {
    serial_io_addr = io_addr;
  } else {
#ifdef NAUT_CONFIG_SERIAL_DEBUGGER
    debug_io_addr = io_addr;
#endif
  }

  //  io_adr = 3F8=COM1, 2F8=COM2, 3E8=COM3, 2E8=COM4 

  // line control register
  // set DLAB so we can write divisor
  outb(0x80, io_addr + 3);

  // write divisor to divisor latch to set speed
  // LSB then MSB
  // 115200 / 1 = 115200 baud 
  outb(1, io_addr + 0);
  outb(0, io_addr + 1);

  // line control register
  // turn DLAB off, set
  // 8 bit word, 1 stop bit, no parity
  outb(0x03, io_addr + 3);

  if (!debugger) { 
      // interrupt enable register
      // raise interrupts on received data available
      // do not raise interrupts on transmit holding register empty,
      // line status update, or modem status update
      // 
      outb(0x01, io_addr + 1);
      
      // FIFO control register
      // turn off FIFOs;  chip is now going to raise an
      // interrupt on every incoming word
      outb(0, io_addr + 2);
      
      // prepare to handle our ~~~ commands
      reset_cmd_fsm();
      
      // enable interrupts (bit 3)
      // outb(0x08, io_addr + 4);
  } else {
#ifdef NAUT_CONFIG_SERIAL_DEBUGGER
      // debugger - raise interrupts on receive and line status (break)
      outb(0x01, io_addr + 1);
      // FIFOs off
      outb(0, io_addr + 2);
      // interrupt global enable
#endif
  }
}

static void serial_putchar_early (uchar_t c)
{
    //  static unsigned short io_adr;
    if (serial_io_addr==0) { 
        return;
    }

    if (!serial_device_ready) {
        return;
    }

    int flags = spin_lock_irq_save(&serial_lock);

    if (c == '\n') { 

        /* wait for transmitter holding register ready */
        while( (inb(serial_io_addr + 5) & 0x20) == 0);

        /* send char */
        outb('\r', serial_io_addr + 0);

    } 

    /* wait for transmitter holding register ready */
    while( (inb(serial_io_addr + 5) & 0x20) == 0);

    /* send char */
    outb(c, serial_io_addr + 0);

    spin_unlock_irq_restore(&serial_lock, flags);
}

void serial_putchar(uchar_t c)
{
    if (early_dev) {
	int flags = spin_lock_irq_save(&serial_lock);
	if (c=='\n') { 
	    serial_do_write_wait(early_dev,"\r");
	}
	serial_do_write_wait(early_dev,&c);
	spin_unlock_irq_restore(&serial_lock,flags);
    } else {
	serial_putchar_early(c);
    }
    
}



void 
serial_write (const char *buf) 
{
    if (early_dev) {
		//	ERROR_PRINT("Early dev: %p\n",early_dev);
		//panic("Early Dev");
		char c;
		int flags = spin_lock_irq_save(&serial_lock);
		while ((c=*buf)) { 
			if (c=='\n') { 
				serial_do_write_wait(early_dev,"\r");
			}
			serial_do_write_wait(early_dev,&c);
			buf++;
		}
		spin_unlock_irq_restore(&serial_lock,flags);
    } else {
		while (*buf) {
			serial_putchar(*buf);
			++buf;
		}
    }
}


void 
serial_puts( const char *buf)
{
  serial_write(buf);
  serial_putchar('\n');
}


static void 
serial_print_hex (uchar_t x)
{
  uchar_t z;
  
  z = (x >> 4) & 0xf;
  serial_print("%x", z);

  z = x & 0xf;
  serial_print("%x", z);
}


static void 
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


static struct Output_Sink serial_output_sink_poll;

static void 
Serial_Emit_Poll (struct Output_Sink * o, int ch) 
{ 
  serial_putchar_early((uchar_t)ch); 
}

static void 
Serial_Finish_Poll (struct Output_Sink * o) { return; }

static void 
Serial_Emit (struct Output_Sink * o, int ch) 
{ 
  serial_putchar((uchar_t)ch); 
}

static struct Output_Sink serial_output_sink;

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
serial_print (const char * format, ...)
{
  va_list args;
  uint8_t iflag = irq_disable_save();

  va_start(args, format);
  __serial_print(format, args);
  va_end(args);

  irq_enable_restore(iflag);
}


void 
__serial_print_poll (const char * format, va_list ap) 
{
    uint8_t flags;

    if (serial_device_ready) {
        int flags = spin_lock_irq_save(&serial_lock);
        Format_Output(&serial_output_sink_poll, format, ap);
	spin_unlock_irq_restore(&serial_lock,flags);
    } else {
        vprintk(format, ap);
    }
}


void 
serial_print_poll(const char * format, ...)
{
  va_list args;
  uint8_t iflag = irq_disable_save();

  va_start(args, format);
  __serial_print_poll(format, args);
  va_end(args);

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

#ifdef NAUT_CONFIG_SERIAL_DEBUGGER

int serial_debugger_put(uint8_t c)
{
    if (!debug_io_addr) {
        return -1;
    }
    
    // Wait on THRE
    while (!(inb(debug_io_addr + 5) & 0x20)) {
    }

    // pump data
    outb(c, debug_io_addr + 0);
    
    return 0;
}

int serial_debugger_get(uint8_t *c)
{
    if (!debug_io_addr) {
        return -1;
    }
    
    // Wait on DR
    while (!(inb(debug_io_addr + 5) & 0x01)) {
    }
    
    // pump data
    *c = inb(debug_io_addr + 0);
    
    return 0;

}

#endif

static struct nk_dev_int devops = {
    .open=0,
    .close=0,
};

void 
serial_early_init (void) 
{
  serial_print_level = SERIAL_PRINT_DEBUG_LEVEL;

  spinlock_init(&serial_lock);

  serial_output_sink.Emit = &Serial_Emit;
  serial_output_sink.Finish = &Serial_Finish;
  serial_output_sink_poll.Emit = &Serial_Emit_Poll;
  serial_output_sink_poll.Finish = &Serial_Finish_Poll;

#if NAUT_CONFIG_SERIAL_REDIRECT
#if NAUT_CONFIG_SERIAL_REDIRECT_PORT == 1 
  serial_init_addr(COM1_ADDR,0);
  com_irq = COM1_3_IRQ;
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 2 
  serial_init_addr(COM2_ADDR,0);
  com_irq = COM2_4_IRQ;
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 3 
  serial_init_addr(COM3_ADDR,0);
  com_irq = COM1_3_IRQ;
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 4
  serial_init_addr(COM4_ADDR,0);
  com_irq = COM2_4_IRQ;;
#else
#error Invalid serial port
#endif
#endif

  serial_device_ready = 1;


#if NAUT_CONFIG_SERIAL_DEBUGGER
  
#if NAUT_CONFIG_SERIAL_DEBUGGER_PORT == 1 
  serial_init_addr(COM1_ADDR,1);
  debug_irq = COM1_3_IRQ;
#elif NAUT_CONFIG_SERIAL_DEBUGGER_PORT == 2 
  serial_init_addr(COM2_ADDR,1);
  debug_irq = COM2_4_IRQ;
#elif NAUT_CONFIG_SERIAL_DEBUGGER_PORT == 3 
  serial_init_addr(COM3_ADDR,1);
  debug_irq = COM1_3_IRQ;
#elif NAUT_CONFIG_SERIAL_DEBUGGER_PORT == 4
  serial_init_addr(COM4_ADDR,1);
  debug_irq = COM2_4_IRQ;
#else
#error Invalid serial debug port
#endif
#endif

}

#if NAUT_CONFIG_SERIAL_DEBUGGER && NAUT_CONFIG_SERIAL_DEBUGGER_PORT==NAUT_CONFIG_SERIAL_REDIRECT_PORT
#error "Redirect Serial Port and Debugger Serial Port cannot be the same!"
#endif

void serial_init()
{
    // post-facto register the generic serial output device
    nk_dev_register("serial-boot",NK_DEV_GENERIC,0,&devops,0);

#if NAUT_CONFIG_SERIAL_DEBUGGER
    nk_dev_register("serial-debug", NK_DEV_GENERIC, 0, &devops, 0);
#endif

    // attempt to find and setup all legacy serial devices 
    // do not change the debugger port if we have one
    // Note that this will also find the serial output port
    // we previously set up and enable interrupts on it
#if !NAUT_CONFIG_SERIAL_DEBUGGER || NAUT_CONFIG_SERIAL_DEBUGGER_PORT!=1
    serial_init_one("serial0",COM1_ADDR,COM1_3_IRQ,0,&legacy[0]);
#endif
#ifndef NAUT_CONFIG_GEM5
    // detection of serial ports broken on gem5 - we allow only serial0 to exist
#if !NAUT_CONFIG_SERIAL_DEBUGGER || NAUT_CONFIG_SERIAL_DEBUGGER_PORT!=2
    serial_init_one("serial1",COM2_ADDR,COM2_4_IRQ,0,&legacy[1]);
#endif
#if !NAUT_CONFIG_SERIAL_DEBUGGER || NAUT_CONFIG_SERIAL_DEBUGGER_PORT!=3
    serial_init_one("serial2",COM3_ADDR,COM1_3_IRQ,0,&legacy[2]);
#endif
#if !NAUT_CONFIG_SERIAL_DEBUGGER || NAUT_CONFIG_SERIAL_DEBUGGER_PORT!=4
    serial_init_one("serial3",COM4_ADDR,COM2_4_IRQ,0,&legacy[3]);
#endif
#endif
    
#ifdef NAUT_CONFIG_SERIAL_REDIRECT
#if NAUT_CONFIG_SERIAL_REDIRECT_PORT == 1 
    early_dev = &legacy[0];
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 2 
    early_dev = &legacy[1];
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 3 
    early_dev = &legacy[2];
#elif NAUT_CONFIG_SERIAL_REDIRECT_PORT == 4
    early_dev = &legacy[3];
#else
#error Invalid serial port
#endif
#endif

    // At this point it is also possible to enable interrupts for the
    // debug port since the interrupt logic (apic, ioapic, interrupt routing)
    // has been configured
#if NAUT_CONFIG_SERIAL_DEBUGGER
    register_irq_handler(debug_irq, debug_irq_handler, NULL);
    nk_unmask_irq(debug_irq);
#endif


}
