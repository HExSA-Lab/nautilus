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
 * Copyright (c) 2016, Peter Dinda
 * Copyright (c) 2016, Yang Wu, Fei Luo and Yuanhui Yang
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:  Peter Dinda <pdinda@northwestern.edu>
 *           Yang Wu, Fei Luo and Yuanhui Yang
 *           {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <nautilus/list.h>
#include <dev/ps2.h>
#include <nautilus/vc.h>
#include <nautilus/chardev.h>
#include <nautilus/printk.h>
#include <dev/serial.h>
#include <dev/vga.h>
#ifdef NAUT_CONFIG_XEON_PHI
#include <arch/k1om/xeon_phi.h>
#endif
#ifdef NAUT_CONFIG_HVM_HRT
#include <arch/hrt/hrt.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_VIRTUAL_CONSOLE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("vc: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("vc: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("vc: " fmt, ##args)

static int up=0;

static spinlock_t state_lock;

#define STATE_LOCK_CONF uint8_t _state_lock_flags
#define STATE_LOCK() _state_lock_flags = spin_lock_irq_save(&state_lock)
#define STATE_UNLOCK() spin_unlock_irq_restore(&state_lock, _state_lock_flags);

#define BUF_LOCK_CONF uint8_t _buf_lock_flags
#define BUF_LOCK(cons) _buf_lock_flags = spin_lock_irq_save(&cons->buf_lock)
#define BUF_UNLOCK(cons) spin_unlock_irq_restore(&cons->buf_lock, _buf_lock_flags);

#define QUEUE_LOCK_CONF uint8_t _queue_lock_flags
#define QUEUE_LOCK(cons) _queue_lock_flags = spin_lock_irq_save(&cons->queue_lock)
#define QUEUE_UNLOCK(cons) spin_unlock_irq_restore(&cons->queue_lock, _queue_lock_flags);

// List of all VC in the system
static struct list_head vc_list;

// Current VC for native console
static struct nk_virtual_console *cur_vc = NULL;

// Typically used VCs
static struct nk_virtual_console *default_vc = NULL;
static struct nk_virtual_console *log_vc = NULL;
static struct nk_virtual_console *list_vc = NULL; 

// We can have any number of chardev consoles
// each of which has a current vc
struct chardev_console {
    int    inited;       // handler thread is up
    nk_thread_id_t   tid;  // thread handling this console
    char   name[DEV_NAME_LEN]; // device name
    struct nk_char_dev *dev; // device for I/O
    struct nk_virtual_console *cur_vc; // current virtual console
    struct list_head chardev_node;  // for list of chardev consoles
};

// List of all chardev consoles in the system
static struct list_head chardev_console_list;


// Broadcast updates to chardev consoles
static void chardev_consoles_putchar(struct nk_virtual_console *vc, char data);
static void chardev_consoles_print(struct nk_virtual_console *vc, char *data);

static nk_thread_id_t list_tid;

#define Keycode_QUEUE_SIZE 256
#define Scancode_QUEUE_SIZE 512

struct nk_virtual_console {
  enum nk_vc_type type;
  char name[32];
  // held with interrupts off => coordinate with interrupt handler
  spinlock_t       queue_lock;
  // held without interrupt change => coordinate with threads
  spinlock_t       buf_lock;
  union queue{
    nk_scancode_t s_queue[Scancode_QUEUE_SIZE];
    nk_keycode_t k_queue[Keycode_QUEUE_SIZE];
  } keyboard_queue;
  uint16_t BUF[VGA_WIDTH * VGA_HEIGHT];
  uint8_t cur_x, cur_y, cur_attr, fill_attr;
  uint16_t head, tail;
  void    (*raw_noqueue_callback)(nk_scancode_t, void *priv);
  void     *raw_noqueue_callback_priv;
  uint32_t num_threads;
  struct list_head vc_node;
  nk_thread_queue_t *waiting_threads;
};


inline int nk_vc_is_active()
{
  return cur_vc!=0;
}

struct nk_virtual_console * nk_get_cur_vc()
{
    return cur_vc;
}

static inline void copy_display_to_vc(struct nk_virtual_console *vc) 
{
#ifdef NAUT_CONFIG_X86_64_HOST
  vga_copy_out((void *)vc->BUF,sizeof(vc->BUF));
#endif
#ifdef NAUT_CONFIG_XEON_PHI
  vga_copy_out((void *)vc->BUF,sizeof(vc->BUF));
#endif
#ifdef NAUT_CONFIG_HVM_HRT
  // not supported
#endif
}

static inline void copy_vc_to_display(struct nk_virtual_console *vc) 
{ 
#ifdef NAUT_CONFIG_X86_64_HOST
  vga_copy_in((void*)vc->BUF,sizeof(vc->BUF));
#endif
#ifdef NAUT_CONFIG_XEON_PHI
  vga_copy_in((void*)vc->BUF,sizeof(vc->BUF));
#endif
#ifdef NAUT_CONFIG_HVM_HRT
  // not supported
#endif
}


struct nk_virtual_console *nk_create_vc(char *name, enum nk_vc_type new_vc_type, uint8_t attr, void (*callback)(nk_scancode_t, void *priv), void *priv_data) 
{
  STATE_LOCK_CONF;
  int i;

  struct nk_virtual_console *new_vc = malloc(sizeof(struct nk_virtual_console));

  if (!new_vc) {
    ERROR("Failed to allocate new console\n");
    return NULL;
  }
  memset(new_vc, 0, sizeof(struct nk_virtual_console));
  new_vc->type = new_vc_type;
  strncpy(new_vc->name,name,32);
  new_vc->raw_noqueue_callback = callback;  
  new_vc->raw_noqueue_callback_priv = priv_data;
  new_vc->cur_attr = attr;
  new_vc->fill_attr = attr;
  spinlock_init(&new_vc->queue_lock);
  spinlock_init(&new_vc->buf_lock);
  new_vc->head = 0;
  new_vc->tail = 0;
  new_vc->num_threads = 0;
  new_vc->waiting_threads = nk_thread_queue_create();

  // clear to new attr
  for (i = 0; i < VGA_HEIGHT*VGA_WIDTH; i++) {
    new_vc->BUF[i] = vga_make_entry(' ', new_vc->cur_attr);
  }

  STATE_LOCK();
  list_add_tail(&new_vc->vc_node, &vc_list);
  STATE_UNLOCK();
  return new_vc;
}


static int _switch_to_vc(struct nk_virtual_console *vc) 
{
  if (!vc) {
    return 0;
  }
  if (vc!=cur_vc) { 
    copy_display_to_vc(cur_vc);
    cur_vc = vc;
    copy_vc_to_display(cur_vc);
#ifdef NAUT_CONFIG_X86_64_HOST
    vga_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
#elif NAUT_CONFIG_XEON_PHI
    phi_cons_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
#endif
  }
  return 0;
}

static int _destroy_vc(struct nk_virtual_console *vc) 
{

  if (vc->num_threads || !nk_queue_empty_atomic(vc->waiting_threads)) { 
    ERROR("Cannot destroy virtual console that has threads\n");
    return -1;
  }

  if (vc==cur_vc) { 
    _switch_to_vc(list_vc);
  }

  list_del(&vc->vc_node);
  nk_thread_queue_destroy(vc->waiting_threads);
  free(vc);
  return 0;
}

int nk_destroy_vc(struct nk_virtual_console *vc) 
{
  STATE_LOCK_CONF;
  int rc;

  STATE_LOCK();
  rc=_destroy_vc(vc);
  STATE_UNLOCK();
  return rc;
}


int nk_bind_vc(nk_thread_t *thread, struct nk_virtual_console * cons)
{
  STATE_LOCK_CONF;
  if (!cons) { 
    return -1;
  }
  STATE_LOCK();
  thread->vc = cons;
  cons->num_threads++;
  STATE_UNLOCK();
  return 0;
}

//release will destroy vc once the number of binders drops to zero
//except that the default vc is never destroyed
int nk_release_vc(nk_thread_t *thread) 
{
  STATE_LOCK_CONF;

  if (!thread || !thread->vc) { 
    return 0;
  }
  STATE_LOCK();
  thread->vc->num_threads--;
  if(thread->vc->num_threads == 0) {
    if(thread->vc != default_vc) {//And it shouldn't be the default vc
      _destroy_vc(thread->vc);
    }
  }
  thread->vc = NULL;
  STATE_UNLOCK();
  return 0;
}

int nk_switch_to_vc(struct nk_virtual_console *vc) 
{
  int rc;
  STATE_LOCK_CONF;

  STATE_LOCK();
  rc = _switch_to_vc(vc);
  STATE_UNLOCK();
  
  return rc;
}

int nk_switch_to_prev_vc()
{
  struct list_head *cur_node = &cur_vc->vc_node;
  struct nk_virtual_console *target;
  
  if(cur_node->prev == &vc_list) {
    return 0;
  }
  target = container_of(cur_node->prev, struct nk_virtual_console, vc_node);
  return nk_switch_to_vc(target);
}

int nk_switch_to_next_vc()
{
  struct list_head *cur_node = &cur_vc->vc_node;
  struct nk_virtual_console *target;

  if(cur_node->next == &vc_list) {
    return 0;
  }

  target = container_of(cur_node->next, struct nk_virtual_console, vc_node);
  return nk_switch_to_vc(target);

}


static int _vc_scrollup_specific(struct nk_virtual_console *vc) 
{
  int i;

#ifdef NAUT_CONFIG_XEON_PHI
  if (vc==cur_vc) {
    phi_cons_notify_scrollup();
  }
#endif

  for (i=0;
       i<VGA_WIDTH*(VGA_HEIGHT-1);
       i++) {
    vc->BUF[i] = vc->BUF[i+VGA_WIDTH];
  }

  for (i = VGA_WIDTH*(VGA_HEIGHT-1);
       i < VGA_WIDTH*VGA_HEIGHT; 
       i++) {
    vc->BUF[i] = vga_make_entry(' ', vc->fill_attr);
  }

  if(vc == cur_vc) {
    copy_vc_to_display(vc);
#ifdef NAUT_CONFIG_XEON_PHI
    phi_cons_notify_redraw();
#endif
  }

  return 0;
}

int nk_vc_scrollup_specific(struct nk_virtual_console *vc) 
{
  BUF_LOCK_CONF;
  int rc;
  if (!vc) { 
    return 0;
  }
  BUF_LOCK(vc);
  rc = _vc_scrollup_specific(vc);
  BUF_UNLOCK(vc);
  return rc;
}

int nk_vc_scrollup()
{
  int rc=0;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    rc = _vc_scrollup_specific(vc);
    BUF_UNLOCK(vc);
  }

  return rc;
}

static int _vc_display_char_specific(struct nk_virtual_console *vc, uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  if (!vc) { 
    return 0;
  }

  uint16_t val = vga_make_entry (c, attr);

  if(x >= VGA_WIDTH || y >= VGA_HEIGHT) {
    return -1;
  } else {
    vc->BUF[y * VGA_WIDTH + x] = val;
    if(vc == cur_vc) {
#ifdef NAUT_CONFIG_X86_64_HOST
      vga_write_screen(x,y,val);
      vga_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
#endif
#ifdef NAUT_CONFIG_XEON_PHI
      vga_write_screen(x,y,val);
      phi_cons_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
#endif

    }
  }
  return 0;
}

static int _vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  struct nk_virtual_console *thread_vc = get_cur_thread()->vc; 
  if (!thread_vc) { 
    thread_vc = default_vc;
  }
  if (!thread_vc) { 
    return 0;
  } else {
    return _vc_display_char_specific(thread_vc,c,attr,x,y);
  }
}

int nk_vc_display_char_specific(struct nk_virtual_console *vc,
				uint8_t c, uint8_t attr, uint8_t x, uint8_t y)
{
  int rc = -1;

  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    rc = _vc_display_char_specific(vc, c, attr, x, y);
    BUF_UNLOCK(vc);
  }
  return rc;
}

int nk_vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  int rc=0;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    rc = _vc_display_char(c,attr,x,y);
    BUF_UNLOCK(vc);
  }

  return rc;
}
  

static int _vc_setpos(uint8_t x, uint8_t y) 
{
  int rc=0;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  if (vc) { 
    vc->cur_x = x;
    vc->cur_y = y;
#ifdef NAUT_CONFIG_X86_64_HOST
    if (vc==cur_vc) { 
      vga_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
    }
#endif
  }
  return 0;
}


int nk_vc_setpos(uint8_t x, uint8_t y) 
{
  int rc=0;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    rc = _vc_setpos(x,y);
    BUF_UNLOCK(vc)
  }

  return rc;
}

static int _vc_setpos_specific(struct nk_virtual_console *vc, uint8_t x, uint8_t y) 
{
  int rc=0;

  if (vc) { 
    vc->cur_x = x;
    vc->cur_y = y;
#ifdef NAUT_CONFIG_X86_64_HOST
    if (vc==cur_vc) { 
      vga_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
    }
#endif
  }

  return rc;
}


int nk_vc_setpos_specific(struct nk_virtual_console *vc, uint8_t x, uint8_t y) 
{
  int rc=0;

  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    rc = _vc_setpos_specific(vc,x,y);
    BUF_UNLOCK(vc);
  }

  return rc;
}


// display scrolling or explicitly on screen at a given location
static int _vc_putchar_specific(struct nk_virtual_console *vc, uint8_t c) 
{
  if (!vc) { 
    return 0;
  }
  if (c == '\n') {
    vc->cur_x = 0;
#ifdef NAUT_CONFIG_XEON_PHI
    if (vc==cur_vc) { 
      phi_cons_notify_line_draw(vc->cur_y);
    }
#endif
    vc->cur_y++;
    if (vc->cur_y == VGA_HEIGHT) {
      _vc_scrollup_specific(vc);
      vc->cur_y--;
    }
    if (vc==cur_vc) {
#ifdef NAUT_CONFIG_X86_64_HOST
      vga_set_cursor(vc->cur_x,vc->cur_y);
#elif NAUT_CONFIG_XEON_PHI
      phi_cons_set_cursor(vc->cur_x,vc->cur_y);
#endif
    }
    return 0;
  }
  _vc_display_char_specific(vc, c, vc->cur_attr, vc->cur_x, vc->cur_y);
  vc->cur_x++;
  if (vc->cur_x == VGA_WIDTH) {
    vc->cur_x = 0;
#ifdef NAUT_CONFIG_XEON_PHI
    if (vc==cur_vc) {
      phi_cons_notify_line_draw(vc->cur_y);
    }
#endif
    vc->cur_y++;
    if (vc->cur_y == VGA_HEIGHT) {
      _vc_scrollup_specific(vc);
      vc->cur_y--;
    }
  }
  if (vc==cur_vc) { 
#ifdef NAUT_CONFIG_X86_64_HOST
    vga_set_cursor(vc->cur_x, vc->cur_y);
#elif NAUT_CONFIG_XEON_PHI
      phi_cons_set_cursor(vc->cur_x,vc->cur_y);
#endif
  }
  return 0;
}

// display scrolling or explicitly on screen at a given location
static int _vc_putchar(uint8_t c) 
{
  struct nk_virtual_console *vc;
  struct nk_thread *t = get_cur_thread();

  if (!t || !(vc = t->vc)) {
    vc = default_vc;
  }

  if (!vc) { 
    return 0;
  } else {
    return _vc_putchar_specific(vc,c);
  }
}

int nk_vc_putchar(uint8_t c) 
{
  if (nk_vc_is_active()) { 
    struct nk_virtual_console *vc;
    struct nk_thread *t = get_cur_thread();
    
    if (!t || !(vc = t->vc)) { 
      vc = default_vc;
    }
    if (vc) {
      BUF_LOCK_CONF;
      BUF_LOCK(vc);
      _vc_putchar_specific(vc,c);
      BUF_UNLOCK(vc);
      chardev_consoles_putchar(vc, c);
    }
  } else {
#ifdef NAUT_CONFIG_X86_64_HOST
    vga_putchar(c);
#endif
#ifdef NAUT_CONFIG_HVM_HRT
    hrt_putchar(c);
#endif
#ifdef NAUT_CONFIG_XEON_PHI
    phi_cons_putchar(c);
#endif
  }

#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_SERIAL_MIRROR_ALL
  serial_putchar(c);
#endif

  return c;
}


static int _vc_print_specific(struct nk_virtual_console *vc, char *s) 
{
  if (!vc) { 
    return 0;
  }
  while(*s) {
    _vc_putchar_specific(vc, *s);
    s++;
  }
  return 0;
}


static int vc_print_specific(struct nk_virtual_console *vc, char *s)
{
    if (!vc) {
        return 0;
    }
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    _vc_print_specific(vc,s);
    BUF_UNLOCK(vc);
    chardev_consoles_print(vc,s);
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_SERIAL_MIRROR_ALL
    serial_write(s);
#endif
    return 0;
}

int nk_vc_print(char *s)
{
  if (nk_vc_is_active()) { 
    struct nk_virtual_console *vc;
    struct nk_thread * t = get_cur_thread();
    
    if (!t || !(vc = t->vc)) { 
      vc = default_vc;
    }
    if (vc) {
      BUF_LOCK_CONF;
      BUF_LOCK(vc);
      _vc_print_specific(vc,s);
      BUF_UNLOCK(vc);
      chardev_consoles_print(vc, s);
    }
  } else {
#ifdef NAUT_CONFIG_X86_64_HOST
    vga_print(s);
#endif
#ifdef NAUT_CONFIG_HVM_HRT
    hrt_print(s);
#endif
#ifdef NAUT_CONFIG_XEON_PHI
    phi_cons_print(s);
#endif
  }
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_SERIAL_MIRROR_ALL
  serial_write(s);
#endif


  return 0;
}

int nk_vc_puts(char *s)
{
  nk_vc_print(s);
  nk_vc_putchar('\n');
  return 0;
}

#define PRINT_MAX 1024

int nk_vc_printf(char *fmt, ...)
{
  char buf[PRINT_MAX];

  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,PRINT_MAX,fmt,args);
  va_end(args);
  nk_vc_print(buf);
  return i;
}

int nk_vc_printf_specific(struct nk_virtual_console *vc, char *fmt, ...)
{
  char buf[PRINT_MAX];

  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,PRINT_MAX,fmt,args);
  va_end(args);
  vc_print_specific(vc, buf);
  return i;
}

int nk_vc_log(char *fmt, ...)
{
  char buf[PRINT_MAX];

  va_list args;
  int i;
  
  va_start(args, fmt);
  i=vsnprintf(buf,PRINT_MAX,fmt,args);
  va_end(args);
  
  if (!log_vc) { 
    // no output to screen possible yet
  } else {
    BUF_LOCK_CONF;
    BUF_LOCK(log_vc);
    _vc_print_specific(log_vc, buf);
    BUF_UNLOCK(log_vc);
    chardev_consoles_print(log_vc, buf);
  }
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_SERIAL_MIRROR
  serial_write(buf);
#endif
  
  return i;
}

static int _vc_setattr_specific(struct nk_virtual_console *vc, uint8_t attr)
{
  vc->cur_attr = attr; 
  return 0;
}


int nk_vc_setattr_specific(struct nk_virtual_console *vc, uint8_t attr)
{
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    _vc_setattr_specific(vc,attr);
    BUF_UNLOCK(vc);
  }
  return 0;

}

int nk_vc_setattr(uint8_t attr)
{
  struct nk_virtual_console *vc;
  struct nk_thread *t = get_cur_thread();

  if (!t || !(vc = t->vc)) { 
    vc = default_vc;
  }
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    _vc_setattr_specific(vc,attr);
    BUF_UNLOCK(vc);
  }
  return 0;
}

static int _vc_clear_specific(struct nk_virtual_console *vc, uint8_t attr)
{
  int i;
  uint16_t val = vga_make_entry (' ', attr);

  vc->cur_attr = attr;
  vc->fill_attr = attr;
    
  for (i = 0; i < VGA_HEIGHT*VGA_WIDTH; i++) {
    vc->BUF[i] = val;
  }
  
  if (vc==cur_vc) { 
    copy_vc_to_display(vc);
#ifdef NAUT_CONFIG_XEON_PHI
    phi_cons_notify_redraw();
#endif
  }
  
  vc->cur_x=0;
  vc->cur_y=0;
  return 0;
}


int nk_vc_clear_specific(struct nk_virtual_console *vc, uint8_t attr)
{
  if (vc) { 
    BUF_LOCK_CONF;
    BUF_LOCK(vc);
    _vc_clear_specific(vc,attr);
    BUF_UNLOCK(vc);
  }
  return 0;
}

int nk_vc_clear(uint8_t attr)
{
  BUF_LOCK_CONF;
  struct nk_thread *t = get_cur_thread();
  struct nk_virtual_console *vc;

  if (!t || !(vc = t->vc)) { 
    vc = default_vc;
  }

  if (!vc) { 
    return 0;
  }
     
  BUF_LOCK(vc);
  _vc_clear_specific(vc,attr);
  BUF_UNLOCK(vc);
  return 0;
}



// called assuming lock is held
static inline int next_index_on_queue(enum nk_vc_type type, int index) 
{
  if (type == RAW) {
    return (index + 1) % Scancode_QUEUE_SIZE;
  } else if (type == COOKED) {
    return (index + 1) % Keycode_QUEUE_SIZE;
  } else {
    ERROR("Using index on a raw, unqueued VC\n");
    return 0;
  }
}

// called assuming lock is held
static inline int is_queue_empty(struct nk_virtual_console *vc) 
{
  return vc->head == vc->tail;
}

// called assuming lock is held
static inline int is_queue_full(struct nk_virtual_console *vc) 
{
  return next_index_on_queue(vc->type, vc->tail) == vc->head;
}

// called without lock
int nk_enqueue_scancode(struct nk_virtual_console *vc, nk_scancode_t scan) 
{
  QUEUE_LOCK_CONF;

  QUEUE_LOCK(vc);

  if(vc->type != RAW || is_queue_full(vc)) {
    DEBUG("Cannot enqueue scancode 0x%x (queue is %s)\n",scan, is_queue_full(vc) ? "full" : "not full");
    QUEUE_UNLOCK(vc);
    return -1;
  } else {
    vc->keyboard_queue.s_queue[vc->tail] = scan;
    vc->tail = next_index_on_queue(vc->type, vc->tail);
    QUEUE_UNLOCK(vc);
    nk_thread_queue_wake_all(vc->waiting_threads);
    return 0;
  }
}

int nk_enqueue_keycode(struct nk_virtual_console *vc, nk_keycode_t key) 
{
  QUEUE_LOCK_CONF;

  QUEUE_LOCK(vc);

  if(vc->type != COOKED || is_queue_full(vc)) {
    DEBUG("Cannot enqueue keycode 0x%x (queue is %s)\n",key,is_queue_full(vc) ? "full" : "not full");
    QUEUE_UNLOCK(vc);
    return -1;
  } else {
    vc->keyboard_queue.k_queue[vc->tail] = key;
    vc->tail = next_index_on_queue(vc->type, vc->tail);
    QUEUE_UNLOCK(vc);
    nk_thread_queue_wake_all(vc->waiting_threads);
    return 0;
  }
}

nk_scancode_t nk_dequeue_scancode(struct nk_virtual_console *vc) 
{
  QUEUE_LOCK_CONF;

  QUEUE_LOCK(vc);

  if(vc->type != RAW || is_queue_empty(vc)) {
    DEBUG("Cannot dequeue scancode (queue is %s)\n",is_queue_empty(vc) ? "empty" : "not empty");
    QUEUE_UNLOCK(vc);
    return NO_SCANCODE;
  } else {
    nk_scancode_t result;
    result = vc->keyboard_queue.s_queue[vc->head];
    vc->head = next_index_on_queue(vc->type, vc->head);
    QUEUE_UNLOCK(vc);
    return result;
  }
}

nk_keycode_t nk_dequeue_keycode(struct nk_virtual_console *vc) 
{
  QUEUE_LOCK_CONF;

  QUEUE_LOCK(vc);

  if (vc->type != COOKED || is_queue_empty(vc)) {
    DEBUG("Cannot dequeue keycode (queue is %s)\n",is_queue_empty(vc) ? "empty" : "not empty");
    QUEUE_UNLOCK(vc);
    return NO_KEY;
  } else {
    nk_keycode_t result;
    result = vc->keyboard_queue.k_queue[vc->head];
    vc->head = next_index_on_queue(vc->type, vc->head);
    QUEUE_UNLOCK(vc);
    return result;
  }
}

static int check(void *state)
{
    struct nk_virtual_console *vc = (struct nk_virtual_console *) state;

    // this check is done without a lock since user of nk_vc_wait() will
    // do a final check anyway
    
    return !is_queue_empty(vc);
}

void nk_vc_wait()
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }

  nk_thread_queue_sleep_extended(vc->waiting_threads, check, vc);
}


nk_keycode_t nk_vc_get_keycode(int wait) 
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }

  if (vc->type != COOKED) {
    ERROR("Incorrect type of VC for get_keycode\n");
    return NO_KEY;
  }

  while (1) { 
    nk_keycode_t k = nk_dequeue_keycode(vc);
    if (k!=NO_KEY) { 
      DEBUG("Returning keycode 0x%x\n",k);
      return k;
    } 
    if (wait) { 
      nk_vc_wait();
    } else {
      return NO_KEY;
    }
  }
}

int nk_vc_getchar_extended(int wait)
{
  nk_keycode_t key;

  while (1) { 
    key = nk_vc_get_keycode(wait);
    
    switch (key) { 
    case KEY_UNKNOWN:
    case KEY_LCTRL:
    case KEY_RCTRL:
    case KEY_LSHIFT:
    case KEY_RSHIFT:
    case KEY_PRINTSCRN:
    case KEY_LALT:
    case KEY_RALT:
    case KEY_CAPSLOCK:
    case KEY_F1:
    case KEY_F2:
    case KEY_F3:
    case KEY_F4:
    case KEY_F5:
    case KEY_F6:
    case KEY_F7:
    case KEY_F8:
    case KEY_F9:
    case KEY_F10:
    case KEY_F11:
    case KEY_F12:
    case KEY_NUMLOCK:
    case KEY_SCRLOCK:
    case KEY_KPHOME:
    case KEY_KPUP:
    case KEY_KPMINUS:
    case KEY_KPLEFT:
    case KEY_KPCENTER:
    case KEY_KPRIGHT:
    case KEY_KPPLUS:
    case KEY_KPEND:
    case KEY_KPDOWN:
    case KEY_KPPGDN:
    case KEY_KPINSERT:
    case KEY_KPDEL:
    case KEY_SYSREQ:
      DEBUG("Ignoring special key 0x%x\n",key);
      continue;
      break;
    default: {
      int c = key&0xff;
      if (c=='\r') { 
	c='\n';
      }
      DEBUG("Regular key 0x%x ('%c')\n", c, c);
      return c;
    }
    }
  }
}

int nk_vc_getchar()
{
  return nk_vc_getchar_extended(1);
}

int nk_vc_gets(char *buf, int n, int display)
{
  int i;
  int c;
  for (i=0;i<(n-1);i++) {
    buf[i] = nk_vc_getchar();
    if (display) { 
      nk_vc_putchar(buf[i]);
    }
    if (buf[i] == '\n') {
      buf[i] = 0;
      return i;
    }
  }
  buf[n-1]=0;
  return n-1;
}

nk_scancode_t nk_vc_get_scancode(int wait)
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }

  if (vc->type != RAW) {
    ERROR("Incorrect type of VC for get_scancode\n");
    return NO_SCANCODE;
  }

  while (1) {
    nk_scancode_t s = nk_dequeue_scancode(vc);
    if (s!=NO_SCANCODE) {
      return s;
    }
    if (wait) { 
      nk_vc_wait();
    } else {
      return NO_SCANCODE;
    }
  }
}

static int enqueue_scancode_as_keycode(struct nk_virtual_console *cur_vc, uint8_t scan)
{
  nk_keycode_t key = kbd_translate(scan);
  if(key != NO_KEY) {
    nk_enqueue_keycode(cur_vc, key);
  }
  return 0;
}

int nk_vc_handle_keyboard(nk_scancode_t scan) 
{
  if (!up) {
    return 0;
  }
  DEBUG("Input: %x\n",scan);
  switch (cur_vc->type) { 
  case RAW_NOQUEUE:
    DEBUG("Delivering event to console %s via callback\n",cur_vc->name);
    cur_vc->raw_noqueue_callback(scan, cur_vc->raw_noqueue_callback_priv);
    return 0;
    break;
  case RAW:
    return nk_enqueue_scancode(cur_vc, scan);
    break;
  case COOKED:
    return enqueue_scancode_as_keycode(cur_vc, scan);
    break;
  default:
    ERROR("vc %s does not have a valid type\n",cur_vc->name);
    break;
  }
  return 0;
}

int nk_vc_handle_mouse(nk_mouse_event_t *m) 
{
  if (!up) {
    return 0;
  }
  // mouse events are not currently routed
  DEBUG("Discarding mouse event\n");
  DEBUG("Mouse Packet: buttons: %s %s %s\n",
	m->left ? "down" : "up",
	m->middle ? "down" : "up",
	m->right ? "down" : "up");
  DEBUG("Mouse Packet: dx: %d dy: %d res: %u\n", m->dx, m->dy, m->res );
  return 0;
}

static int vc_list_inited=0;

static void list(void *in, void **out)
{
  struct list_head *cur;
  int i;


  if (!list_vc) { 
    ERROR("No virtual console for list..\n");
    return;
  }
  
  if (nk_thread_name(get_cur_thread(),"vc-list")) {   
    ERROR_PRINT("Cannot name vc-list's thread\n");
    return;
  }

  if (nk_bind_vc(get_cur_thread(), list_vc)) { 
    ERROR("Cannot bind virtual console for list\n");
    return;
  }
  
  // declare we are up
  __sync_fetch_and_add(&vc_list_inited,1);

  while (1) {
    nk_vc_clear(0xf9);
   
    nk_vc_print("List of VCs (space to regenerate)\n\n");

    i=0;
    list_for_each(cur,&vc_list) {
      nk_vc_printf("%c : %s\n", 'a'+i, list_entry(cur,struct nk_virtual_console, vc_node)->name);
      i++;
    }

    int c = nk_vc_getchar(1);
    
    i=0;
    list_for_each(cur,&vc_list) {
      if (c == (i+'a')) { 
	nk_switch_to_vc(list_entry(cur,struct nk_virtual_console, vc_node));
	break;
      }
      i++;
    }
  }
}


int nk_switch_to_vc_list()
{
  return nk_switch_to_vc(list_vc);
}

static int start_list()
{

  if (nk_thread_start(list, 0, 0, 1, PAGE_SIZE_4KB, &list_tid, 0)) {
    ERROR("Failed to launch VC list\n");
    return -1;
  }
  
  while (!__sync_fetch_and_add(&vc_list_inited,0)) {
      // wait for vc list to be up
      nk_yield();
  }

  INFO("List launched\n");

  return 0;
}



static void _chardev_consoles_print(struct nk_virtual_console *vc, char *data, uint64_t len)
{
    struct list_head *cur;

    STATE_LOCK_CONF;

    STATE_LOCK();

    list_for_each(cur,&chardev_console_list) {
	struct chardev_console *c = list_entry(cur,struct chardev_console, chardev_node);
	if (c->cur_vc == vc) { 
	    uint64_t i;
	    // translate lf->crlf
	    for (i=0;i<len;i++) {
		if (data[i]=='\n') { 
		    nk_char_dev_write(c->dev,1,"\r",NK_DEV_REQ_BLOCKING);
		}
		nk_char_dev_write(c->dev,1,&data[i],NK_DEV_REQ_BLOCKING);
	    }
	}
    }
    
    STATE_UNLOCK();
}

static void chardev_consoles_print(struct nk_virtual_console *vc, char *data)
{
    _chardev_consoles_print(vc,data,strlen(data));
}
 
static void chardev_consoles_putchar(struct nk_virtual_console *vc, char data)
{
    _chardev_consoles_print(vc,&data,1);
}

static int chardev_console_handle_input(struct chardev_console *c, uint8_t data)
{
    if (c->cur_vc->type == COOKED) { 
	return nk_enqueue_keycode(c->cur_vc,data);
    } else {
	// raw data not currently handled
	return 0;
    }
}

//
// Commands:
// 
//   ``1 = left
//   ``2 = right
//   ``3 = vc list
static void chardev_console(void *in, void **out)
{
    uint8_t data[3];
    char myname[80]; 
    uint8_t cur=0;


    struct chardev_console *c = (struct chardev_console *)in;

    c->dev = nk_char_dev_find(c->name);

    if (!c->dev) {
	ERROR("Unable to open char device %s - no chardev console started\n",c->name);
	return;
    } 

    strcpy(myname,c->name);
    strcat(myname,"-console");

    if (nk_thread_name(get_cur_thread(),myname)) {   
        ERROR_PRINT("Cannot name chardev console's thread\n");
        return;
    }

    // declare we are up
    __sync_fetch_and_add(&c->inited,1);
    

    char buf[80];
		
    snprintf(buf,80,"\n*** Console %s // prev=``1 next=``2 list=``3 ***\n",myname);

    nk_char_dev_write(c->dev,strlen(buf),buf,NK_DEV_REQ_BLOCKING);

    snprintf(buf,80,"\n*** %s ***\n",c->cur_vc->name);
    nk_char_dev_write(c->dev,strlen(buf),buf,NK_DEV_REQ_BLOCKING);
    

    cur = 0;
    while (1) {
	if (nk_char_dev_read(c->dev,1,&data[cur],NK_DEV_REQ_BLOCKING)!=1) { 
	    ERROR("FAILED TO READ CHARDEV %s\n",c->name);
	}


	if (cur==0) {
	    if (data[cur]!='`') { 
		chardev_console_handle_input(c,data[0]);
	    } else {
		cur++;
	    }
	    continue;
	} 
	
	if (cur==1) { 
	    if (data[cur]!='`') { 
		chardev_console_handle_input(c,data[0]);
		chardev_console_handle_input(c,data[1]);
		cur=0;
	    } else {
		cur++;
	    }
	    continue;
	}

	if (cur==2) { 
	    struct list_head *cur_node = &c->cur_vc->vc_node;
	    struct list_head *next_node = &c->cur_vc->vc_node;
	    int do_data=0;


	    switch (data[cur]) {
	    case '1': 
                if (cur_node->prev != &vc_list) {
		    next_node = cur_node->prev;
                }
		break;
	    case '2': 
                if (cur_node->next != &vc_list) {
		    next_node = cur_node->next;
                }
		break;
	    case '3': {
		// display the vcs
		struct list_head *cur;
		int i;
		char which;
		strcpy(buf,"\nList of VCs \n\n");
		nk_char_dev_write(c->dev,strlen(buf),buf,NK_DEV_REQ_BLOCKING);
		i=0;
		list_for_each(cur,&vc_list) {
		    snprintf(buf,80,"%c : %s\n", 'a'+i, list_entry(cur,struct nk_virtual_console, vc_node)->name);
		    nk_char_dev_write(c->dev,strlen(buf),buf,NK_DEV_REQ_BLOCKING);
		    i++;
		}
		// get user input
		if (nk_char_dev_read(c->dev,1,&which,NK_DEV_REQ_BLOCKING)!=1) { 
		    nk_char_dev_write(c->dev,7,"\nERROR\n",NK_DEV_REQ_BLOCKING);
		    next_node = cur_node;
		    break;
		} 
		i=0;
		list_for_each(cur,&vc_list) {
		    if (which == (i+'a')) { 
			next_node = cur;
			break;
		    }
		    i++;
		}
	    }
		break;
	    default:
		do_data = 1;
		next_node = cur_node;
	    }

	    c->cur_vc = container_of(next_node, struct nk_virtual_console, vc_node);

	    if (do_data) {
		chardev_console_handle_input(c,data[0]);
		chardev_console_handle_input(c,data[1]);
		chardev_console_handle_input(c,data[2]);
	    } else {
		char buf[80];
		
		snprintf(buf,80,"\n*** %s ***\n",c->cur_vc->name);
		
		nk_char_dev_write(c->dev,strlen(buf),buf,NK_DEV_REQ_BLOCKING);
	    }
	    
	    cur = 0;

	    continue;
	}
    }		
}




int nk_vc_start_chardev_console(char *chardev)
{
    struct chardev_console *c = malloc(sizeof(*c));
				       
    if (!c) { 
	ERROR("Cannot allocate chardev console for %s\n",chardev);
	return -1;
    }

    memset(c,0,sizeof(*c));
    strncpy(c->name,chardev,DEV_NAME_LEN);
    c->name[DEV_NAME_LEN-1] = 0;
    c->cur_vc = default_vc;

    // make sure everyone sees this is zeroed...
    __sync_fetch_and_and(&c->inited,0);

    if (nk_thread_start(chardev_console, c, 0, 1, PAGE_SIZE_4KB, &c->tid, 0)) {
	ERROR("Failed to launch chardev console handler for %s\n",c->name);
	free(c);
	return -1;
    }
    
    while (!__sync_fetch_and_add(&c->inited,0)) {
	nk_yield();
	// wait for console thread to start
    }
    
    STATE_LOCK_CONF;

    STATE_LOCK();
    list_add_tail(&c->chardev_node, &chardev_console_list);
    STATE_UNLOCK();

    INFO("chardev console %s launched\n",c->name);
    
    return 0;
}


int nk_vc_init() 
{
  INFO("init\n");
  spinlock_init(&state_lock);
  INIT_LIST_HEAD(&vc_list);
  INIT_LIST_HEAD(&chardev_console_list);

  default_vc = nk_create_vc("default", COOKED, 0x0f, 0, 0);
  if(!default_vc) {
    ERROR("Cannot create default console...\n");
    return -1;
  }

  log_vc = nk_create_vc("system-log", COOKED, 0x0a, 0, 0);
  if(!log_vc) {
    ERROR("Cannot create log console...\n");
    return -1;
  }
  

  list_vc = nk_create_vc("vc-list", COOKED, 0xf9, 0, 0);
  if(!list_vc) {
    ERROR("Cannot create vc list console...\n");
    return -1;
  }

  if (start_list()) { 
    ERROR("Cannot create vc list thread\n");
    return -1;
  }

  cur_vc = default_vc;
  copy_display_to_vc(cur_vc);
  
#ifdef NAUT_CONFIG_X86_64_HOST
  vga_get_cursor(&(cur_vc->cur_x),&(cur_vc->cur_y));
  vga_init_screen();
  vga_set_cursor(cur_vc->cur_x,cur_vc->cur_y);
#elif NAUT_CONFIG_XEON_PHI
  phi_cons_get_cursor(&(cur_vc->cur_x), &(cur_vc->cur_y));
  phi_cons_clear_screen();
  phi_cons_set_cursor(cur_vc->cur_x, cur_vc->cur_y);
#endif

  up=1;

  return 0;
}


int nk_vc_deinit()
{
  struct list_head *cur;

  nk_thread_destroy(list_tid);

  // deactivate VC
  cur_vc = 0;

  list_for_each(cur,&vc_list) {
    if (nk_destroy_vc(list_entry(cur,struct nk_virtual_console, vc_node))) { 
      ERROR("Failed to destroy all VCs\n");
      return -1;
    }
  }

  // should free chardev consoles here...

  return 0;
}
