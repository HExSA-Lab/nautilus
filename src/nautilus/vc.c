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
 * http://xtack.sandia.gov/hobbes
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
#include <dev/kbd.h>
#include <nautilus/vc.h>
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

static spinlock_t state_lock;

#define LOCK() spin_lock(&state_lock)
#define UNLOCK() spin_unlock(&state_lock);



static struct list_head vc_list;
static struct nk_virtual_console *cur_vc = NULL;
static struct nk_virtual_console *default_vc = NULL;
static struct nk_virtual_console *log_vc = NULL;
static struct nk_virtual_console *list_vc = NULL; 

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
  uint8_t cur_x, cur_y, cur_attr;
  uint16_t head, tail;
  void    (*raw_noqueue_callback)(nk_scancode_t);
  uint32_t num_threads;
  struct list_head vc_node;
  nk_thread_queue_t *waiting_threads;
};


inline int nk_vc_is_active()
{
  return cur_vc!=0;
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


struct nk_virtual_console *nk_create_vc(char *name, enum nk_vc_type new_vc_type, uint8_t attr, void (*callback)(nk_scancode_t)) 
{
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
  new_vc->cur_attr = attr;
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

  LOCK();
  list_add_tail(&new_vc->vc_node, &vc_list);
  UNLOCK();
  return new_vc;
}



int nk_destroy_vc(struct nk_virtual_console *vc) 
{
  if (vc->num_threads || !nk_queue_empty_atomic(vc->waiting_threads)) { 
    ERROR("Cannot destroy virtual console that has threads\n");
    return -1;
  }
  LOCK();
  list_del(&vc->vc_node);
  UNLOCK();
  nk_thread_queue_destroy(vc->waiting_threads);
  free(vc);
  return 0;
}

int nk_bind_vc(nk_thread_t *thread, struct nk_virtual_console * cons)
{
  if (!cons) { 
    return -1;
  }
  LOCK();
  thread->vc = cons;
  cons->num_threads++;
  UNLOCK();
  return 0;
}

//release will destroy vc once the number of binders drops to zero
//except that the default vc is never destroyed
int nk_release_vc(nk_thread_t *thread) 
{
  if (!thread || !thread->vc) { 
    return 0;
  }
  LOCK();
  thread->vc->num_threads--;
  if(thread->vc->num_threads == 0) {
    if(thread->vc != default_vc) {//And it shouldn't be the default vc
      nk_destroy_vc(thread->vc);
    }
  }
  thread->vc = NULL;
  UNLOCK();
  return 0;
}

int nk_switch_to_vc(struct nk_virtual_console *vc) 
{
  if (!vc) {
    return 0;
  }
  LOCK();
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
  UNLOCK();
  return 0;
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


static int _vc_scrollup (struct nk_virtual_console *vc) 
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
    vc->BUF[i] = vga_make_entry(' ', vc->cur_attr);
  }

  if(vc == cur_vc) {
    copy_vc_to_display(vc);
#ifdef NAUT_CONFIG_XEON_PHI
    phi_cons_notify_redraw();
#endif
  }

  return 0;
}

int nk_vc_scrollup (struct nk_virtual_console *vc) 
{
  int rc;
  if (!vc) { 
    return 0;
  }
  spin_lock(&vc->buf_lock);
  rc = _vc_scrollup(vc);
  spin_unlock(&vc->buf_lock);
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

int nk_vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  int rc=0;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  if (vc) { 
    spin_lock(&vc->buf_lock);
    rc = _vc_display_char(c,attr,x,y);
    spin_unlock(&vc->buf_lock);
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
      _vc_scrollup(vc);
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
      _vc_scrollup(vc);
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
      spin_lock(&vc->buf_lock);
      _vc_putchar_specific(vc,c);
      spin_unlock(&vc->buf_lock);
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

int nk_vc_print(char *s)
{
  if (nk_vc_is_active()) { 
    struct nk_virtual_console *vc;
    struct nk_thread * t = get_cur_thread();
    
    if (!t || !(vc = t->vc)) { 
      vc = default_vc;
    }
    if (vc) {
      spin_lock(&vc->buf_lock);
      _vc_print_specific(vc,s);
      spin_unlock(&vc->buf_lock);
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
    spin_lock(&log_vc->buf_lock);
    _vc_print_specific(log_vc, buf);
    spin_unlock(&log_vc->buf_lock);
  }
#ifdef NAUT_CONFIG_VIRTUAL_CONSOLE_SERIAL_MIRROR
  serial_write(buf);
#endif
  
  return i;
}

int nk_vc_setattr(uint8_t attr)
{
  struct nk_virtual_console *vc;
  struct nk_thread *t = get_cur_thread();

  if (!t || !(vc = t->vc)) { 
    vc = default_vc;
  }
  if (vc) { 
    spin_lock(&vc->buf_lock);
    vc->cur_attr = attr; 
    spin_unlock(&vc->buf_lock);
  }
  return 0;

}

int nk_vc_clear(uint8_t attr)
{
  int i;
  uint16_t val = vga_make_entry (' ', attr);

  struct nk_thread *t = get_cur_thread();
  struct nk_virtual_console *vc;

  if (!t || !(vc = t->vc)) { 
    vc = default_vc;
  }

  if (!vc) { 
    return 0;
  }
     
  spin_lock(&vc->buf_lock);

  vc->cur_attr = attr;
    
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

  spin_unlock(&vc->buf_lock);
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
  uint8_t flags;

  if (vc->type==RAW_NOQUEUE) { 
    vc->raw_noqueue_callback(scan);
    return 0;
  }

  flags = spin_lock_irq_save(&vc->queue_lock);

  if(vc->type != RAW || is_queue_full(vc)) {
    DEBUG("Cannot enqueue scancode 0x%x (queue is %s)\n",scan, is_queue_full(vc) ? "full" : "not full");
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return -1;
  } else {
    vc->keyboard_queue.s_queue[vc->tail] = scan;
    vc->tail = next_index_on_queue(vc->type, vc->tail);
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    nk_thread_queue_wake_all(vc->waiting_threads);
    return 0;
  }
}

int nk_enqueue_keycode(struct nk_virtual_console *vc, nk_keycode_t key) 
{
  uint8_t flags;

  flags = spin_lock_irq_save(&vc->queue_lock);

  if(vc->type != COOKED || is_queue_full(vc)) {
    DEBUG("Cannot enqueue keycode 0x%x (queue is %s)\n",key,is_queue_full(vc) ? "full" : "not full");
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return -1;
  } else {
    vc->keyboard_queue.k_queue[vc->tail] = key;
    vc->tail = next_index_on_queue(vc->type, vc->tail);
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    nk_thread_queue_wake_all(vc->waiting_threads);
    return 0;
  }
}

nk_scancode_t nk_dequeue_scancode(struct nk_virtual_console *vc) 
{
  uint8_t flags;

  flags = spin_lock_irq_save(&vc->queue_lock);

  if(vc->type != RAW || is_queue_empty(vc)) {
    DEBUG("Cannot dequeue scancode (queue is %s)\n",is_queue_empty(vc) ? "empty" : "not empty");
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return NO_SCANCODE;
  } else {
    nk_scancode_t result;
    result = vc->keyboard_queue.s_queue[vc->head];
    vc->head = next_index_on_queue(vc->type, vc->head);
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return result;
  }
}

nk_keycode_t nk_dequeue_keycode(struct nk_virtual_console *vc) 
{
  uint8_t flags;

  flags = spin_lock_irq_save(&vc->queue_lock);

  if (vc->type != COOKED || is_queue_empty(vc)) {
    DEBUG("Cannot dequeue keycode (queue is %s)\n",is_queue_empty(vc) ? "empty" : "not empty");
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return NO_KEY;
  } else {
    nk_keycode_t result;
    result = vc->keyboard_queue.k_queue[vc->head];
    vc->head = next_index_on_queue(vc->type, vc->head);
    spin_unlock_irq_restore(&vc->queue_lock,flags);
    return result;
  }
}


void nk_vc_wait()
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }

  nk_thread_queue_sleep(vc->waiting_threads);
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

int nk_vc_handle_input(nk_scancode_t scan) 
{
  DEBUG("Input: %x\n",scan);
  if(cur_vc->type == RAW) {
    nk_enqueue_scancode(cur_vc, scan);
  } else {
    enqueue_scancode_as_keycode(cur_vc, scan);
  }
  return 0;
}

static void list(void *in, void **out)
{
  struct list_head *cur;
  int i;


  if (!list_vc) { 
    ERROR("No virtual console for list..\n");
    return;
  }
  
  if (nk_bind_vc(get_cur_thread(), list_vc)) { 
    ERROR("Cannot bind virtual console for list\n");
    return;
  }
  
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

  nk_thread_start(list, 0, 0, 0, PAGE_SIZE, &list_tid, -1);
  
  INFO("List launched\n");

  return 0;
}

int nk_vc_init() 
{
  INFO("init\n");
  spinlock_init(&state_lock);
  INIT_LIST_HEAD(&vc_list);


  default_vc = nk_create_vc("default", COOKED, 0x0f, 0);
  if(!default_vc) {
    ERROR("Cannot create default console...\n");
    return -1;
  }

  log_vc = nk_create_vc("system-log", COOKED, 0x0a, 0);
  if(!log_vc) {
    ERROR("Cannot create log console...\n");
    return -1;
  }
  

  list_vc = nk_create_vc("vc-list", COOKED, 0xf9, 0);
  if(!list_vc) {
    ERROR("Cannot create vc list console...\n");
    return -1;
  }

  start_list();

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
  return 0;
}
