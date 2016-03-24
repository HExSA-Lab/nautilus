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
 * Copyright (c) 2016, Yang Wu, Fei Luo and Yuanhui Yang
 * Copyright (c) 2016, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Yang Wu, Fei Luo and Yuanhui Yang
 *          {YangWu2015, FeiLuo2015, YuanhuiYang2015}@u.northwestern.edu
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <nautilus/list.h>
#include <nautilus/cga.h>
#include <dev/kbd.h>
#include <nautilus/vc.h>

#ifndef NAUT_CONFIG_DEBUG_VIRTUAL_CONSOLE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) printk("vc: ERROR: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("vc: DEBUG: " fmt, ##args)
#define INFO(fmt, args...) printk("vc: " fmt, ##args)

static spinlock_t state_lock;

#define LOCK() spin_lock(&state_lock)
#define UNLOCK() spin_unlock(&state_lock);


static struct list_head vc_list;
static struct nk_virtual_console *cur_vc = NULL;
static struct nk_virtual_console *default_vc = NULL;

#define Keycode_QUEUE_SIZE 256
#define Scancode_QUEUE_SIZE 512

struct nk_virtual_console {
  enum nk_vc_type type;
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

static inline void copy_display_to_vc(struct nk_virtual_console *vc) 
{
  memcpy(vc->BUF, (void*)VGA_BASE_ADDR, sizeof(vc->BUF));
}

static inline void copy_vc_to_display(struct nk_virtual_console *vc) 
{
  memcpy((void*)VGA_BASE_ADDR, vc->BUF, sizeof(vc->BUF));
}

struct nk_virtual_console *nk_create_vc(enum nk_vc_type new_vc_type, void (*callback)(nk_scancode_t)) 
{
  struct nk_virtual_console *new_vc = malloc(sizeof(struct nk_virtual_console));
  if(!new_vc) {
    ERROR("Failed to allocate new console\n");
    return NULL;
  }
  memset(new_vc, 0, sizeof(struct nk_virtual_console));
  new_vc->type = new_vc_type;
  new_vc->raw_noqueue_callback = callback;
  new_vc->cur_attr = 0xf0;
  spinlock_init(&new_vc->queue_lock);
  spinlock_init(&new_vc->buf_lock);
  new_vc->head = 0;
  new_vc->tail = 0;
  new_vc->num_threads = 0;
  new_vc->waiting_threads = nk_thread_queue_create();
  LOCK();
  list_add_tail(&new_vc->vc_node, &vc_list);
  UNLOCK();
  return new_vc;
}

struct nk_virtual_console *nk_create_default_vc(enum nk_vc_type new_vc_type) 
{
  struct nk_virtual_console *new_vc = malloc(sizeof(struct nk_virtual_console));
  
  if(!new_vc) {
    ERROR("Failed to allocate initial VC\n");
    return NULL;
  }
  memset(new_vc, 0, sizeof(struct nk_virtual_console));
  new_vc->type = new_vc_type;
  new_vc->cur_attr = 0xf0;
  spinlock_init(&new_vc->queue_lock);
  spinlock_init(&new_vc->buf_lock);
  new_vc->head = 0;
  new_vc->tail = 0;
  new_vc->num_threads = 0;
  new_vc->waiting_threads = nk_thread_queue_create();
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
  LOCK();
  if (vc!=cur_vc) { 
    copy_display_to_vc(cur_vc);
    cur_vc = vc;
    copy_vc_to_display(cur_vc);
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

static uint16_t make_vgaentry (char c, uint8_t attr)
{
  uint16_t c16 = c;
  uint16_t attr16 = attr;
  return c16 | attr16 << 8;
}

static int _vc_scrollup (struct nk_virtual_console *vc) 
{
  int i;
  int n = (((VGA_HEIGHT-1)*VGA_WIDTH*2)/sizeof(long));
  int lpl = (VGA_WIDTH*2)/sizeof(long);
  long * pos = (long*)vc->BUF;
  
  for (i = 0; i < n; i++) {
    *pos = *(pos + lpl);
    ++pos;
  }
  
  size_t index = (VGA_HEIGHT-1) * VGA_WIDTH;
  for (i = 0; i < VGA_WIDTH; i++) {
    vc->BUF[index++] = make_vgaentry(' ', vc->cur_attr);
  }
  
  if(vc == cur_vc) {
    copy_vc_to_display(vc);
  }
  return 0;
}

int nk_vc_scrollup (struct nk_virtual_console *vc) 
{
  int rc;
  spin_lock(&vc->buf_lock);
  rc = _vc_scrollup(vc);
  spin_unlock(&vc->buf_lock);
  return rc;
}

static int _vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  //  DEBUG("display %d %d at %d %d\n",c,attr,x,y);

  uint16_t val = make_vgaentry (c, attr);

  if(x >= VGA_WIDTH || y >= VGA_HEIGHT) {
    return -1;
  } else {
    struct nk_virtual_console *thread_vc = get_cur_thread()->vc; 
    if (!thread_vc) { 
      thread_vc = default_vc;
    }
    thread_vc->BUF[y * VGA_WIDTH + x] = val;
    if(thread_vc == cur_vc) {
      uint16_t *screen = (uint16_t*)VGA_BASE_ADDR;
      screen[y * VGA_WIDTH + x] = val;
    }
  }
  return 0;
}

int nk_vc_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) 
{
  int rc;
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }
  
  spin_lock(&vc->buf_lock);
  rc = _vc_display_char(c,attr,x,y);
  spin_unlock(&vc->buf_lock);
  return rc;
}
  


// display scrolling or explicitly on screen at a given location
static int _vc_putchar(uint8_t c) 
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;
  if (!vc) { 
    vc = default_vc;
  }
  if (c == '\n') {
    vc->cur_x = 0;
    if (++vc->cur_y == VGA_HEIGHT) {
      _vc_scrollup(vc);
      vc->cur_y--;
    }
    return 0;
  }
  _vc_display_char(c, vc->cur_attr, vc->cur_x, vc->cur_y);
  if (++vc->cur_x == VGA_WIDTH) {
    vc->cur_x = 0;
    if (++vc->cur_y == VGA_HEIGHT) {
      _vc_scrollup(vc);
      vc->cur_y--;
    }
  }
  return 0;
}

int nk_vc_putchar(uint8_t c) 
{
  int rc;
  struct nk_virtual_console *vc = get_cur_thread()->vc;
  if (!vc) { 
    vc = default_vc;
  }
  spin_lock(&vc->buf_lock);
  rc = _vc_putchar(c);
  spin_unlock(&vc->buf_lock);
  return rc;
}


int nk_vc_puts(char *s) 
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;
  if (!vc) { 
    vc = default_vc;
  }
  spin_lock(&vc->buf_lock);
  while(*s) {
    _vc_putchar(*s);
    s++;
  }
  _vc_putchar('\n');
  spin_unlock(&vc->buf_lock);
  return 0;
}

int nk_vc_setattr(uint8_t attr)
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;
  if (!vc) { 
    vc = default_vc;
  }
  spin_lock(&vc->buf_lock);
  vc->cur_attr = attr; 
  spin_unlock(&vc->buf_lock);
  return 0;
}

int nk_vc_clear(uint8_t attr)
{
  int i;
  uint16_t val = make_vgaentry (' ', attr);

  struct nk_virtual_console *vc = get_cur_thread()->vc;
  if (!vc) { 
    vc = default_vc;
  }


  spin_lock(&vc->buf_lock);

  vc->cur_attr = attr;

  for (i = 0; i < VGA_HEIGHT*VGA_WIDTH; i++) {
    vc->BUF[i] = val;
  }
  
  if (vc==cur_vc) { 
    copy_vc_to_display(vc);
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


nk_keycode_t nk_vc_getchar() 
{
  struct nk_virtual_console *vc = get_cur_thread()->vc;

  if (!vc) { 
    vc = default_vc;
  }

  if (vc->type != COOKED) {
    ERROR("Incorrect type of VC for getchar\n");
    return NO_KEY;
  }

  while (1) { 
    nk_keycode_t k = nk_dequeue_keycode(vc);
    if (k!=NO_KEY) { 
      printk("Returning %x\n",k);
      return k;
    } 
    nk_vc_wait();
  }
}

nk_scancode_t nk_vc_get_scancode()
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
    nk_vc_wait();
  }
}

static int enqueue_scancode_as_keycode(struct nk_virtual_console *cur_vc, uint8_t scan)
{
  nk_keycode_t key = kbd_translator(scan);
  if(key != 0x0000) {
    nk_enqueue_keycode(cur_vc, key);
  }
  return 0;
}

int nk_vc_handle_input(uint8_t scan) 
{
  DEBUG("Input: %x\n",scan);
  if(cur_vc->type == RAW) {
    nk_enqueue_scancode(cur_vc, scan);
  } else {
    enqueue_scancode_as_keycode(cur_vc, scan);
  }
  return 0;
}

int nk_vc_init() {
  INFO("init\n");
  spinlock_init(&state_lock);
  INIT_LIST_HEAD(&vc_list);

  default_vc = nk_create_default_vc(COOKED);
  if(!default_vc) {
    ERROR("Cannot create default console...\n");
    return -1;
  }

  cur_vc = default_vc;
  copy_display_to_vc(cur_vc);
  nk_vc_puts("Welcome to Nautilus Virtual Console System.\n--Yang Wu, Fei Luo, Yuanhui Yang.");
  return 0;
}

int nk_vc_deinit()
{
  struct list_head *cur;
  
  list_for_each(cur,&vc_list) {
    if (nk_destroy_vc(list_entry(cur,struct nk_virtual_console, vc_node))) { 
      ERROR("Failed to destroy all VCs\n");
      return -1;
    }
  }
  return 0;
}
