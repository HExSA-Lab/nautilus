#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <dev/kbd.h>
#include <nautilus/list.h>
#include <nautilus/cga.h>
#include <nautilus/vc.h>

#include <stdbool.h>

//#ifndef NAUT_CONFIG_DEBUG_VC
//#undef DEBUG_PRINT
//#define DEBUG_PRINT(fmt, args...) 
//#endif

static struct list_head *vc_list = NULL;
static struct virtual_console *cur_vc = NULL;
static struct virtual_console *default_vc = NULL;

#define Keycode_QUEUE_SIZE 256
#define Scancode_QUEUE_SIZE 512

struct virtual_console{
    enum VC_TYPE type;
    union queue{
        Scancode s_queue[Scancode_QUEUE_SIZE];
        Keycode k_queue[Keycode_QUEUE_SIZE];
    } keyboard_queue;
    uint16_t BUF[80 * 25];
    uint8_t cur_x, cur_y, cur_attr;
    uint16_t head, tail;
    uint32_t num_threads;
    struct list_head vc_node;
    nk_thread_queue_t *waiting_threads;
};

void copy_display_to_vc(struct virtual_console *vc) {
  memcpy(vc->BUF, (void*)VGA_BASE_ADDR, sizeof(vc->BUF));
}
void copy_vc_to_display(struct virtual_console *vc) {
  memcpy((void*)VGA_BASE_ADDR, vc->BUF, sizeof(vc->BUF));
}

struct virtual_console *nk_create_vc(enum VC_TYPE new_vc_type) {
    struct virtual_console *new_vc = malloc(sizeof(struct virtual_console));
    if(!new_vc) {
        return NULL;
    }
    memset(new_vc, 0, sizeof(struct virtual_console));
    new_vc->type = new_vc_type;
    new_vc->cur_attr = 0xf0;
    new_vc->head = 0;
    new_vc->tail = 0;
    new_vc->num_threads = 0;
    new_vc->waiting_threads = nk_thread_queue_create();
    //INIT_LIST_HEAD(&new_vc->waiting_threads_list);
    list_add_tail(&new_vc->vc_node, vc_list);
    return new_vc;
}

struct virtual_console *nk_create_default_vc(enum VC_TYPE new_vc_type) {
    struct virtual_console *new_vc = malloc(sizeof(struct virtual_console));
    if(!new_vc) {
        return NULL;
    }
    memset(new_vc, 0, sizeof(struct virtual_console));
    new_vc->type = new_vc_type;
    new_vc->cur_attr = 0xf0;
    new_vc->head = 0;
    new_vc->tail = 0;
    new_vc->num_threads = 0;
    new_vc->waiting_threads = nk_thread_queue_create();
    //INIT_LIST_HEAD(&new_vc->waiting_threads_list);
    vc_list = &new_vc->vc_node;
    INIT_LIST_HEAD(vc_list);
    return new_vc;
}


int nk_destroy_vc(struct virtual_console *vc) {
    list_del(&vc->vc_node);
    free(vc);
    return 0;
}

int nk_bind_vc(nk_thread_t *thread, struct virtual_console * cons, void (*newdatacallback)() ) {
    thread->vc = cons;
    cons->num_threads++;
    return 0;
}

//release will destroy vc once the number of binders drops to zero
int nk_release_vc(nk_thread_t *thread) {
    thread->vc->num_threads--;
    if(thread->vc->num_threads == 0) {
       if(thread->vc != default_vc) {//And it shouldn't be the default vc
           nk_destroy_vc(thread->vc);
       }
    }
    thread->vc = NULL;
    return 0;
}

int nk_switch_to_vc(struct virtual_console *vc) {
    copy_display_to_vc(cur_vc);
    cur_vc = vc;
    copy_vc_to_display(cur_vc);
    return 0;
}

int nk_switch_to_prev_vc(){
    struct list_head cur_node = cur_vc->vc_node;
    struct virtual_console *target;
    /*
    if(cur_node.prev == &vc_list){
        return nk_switch_to_vc(cur_vc);
    }
    */
    if(cur_node.prev == NULL) {
        return 0;
    }
    target = container_of(cur_node.prev, struct virtual_console, vc_node);
    return nk_switch_to_vc(target);
}

int nk_switch_to_next_vc(){
    struct list_head cur_node = cur_vc->vc_node;
    struct virtual_console *target;
    /*
    if(cur_node.next == &vc_list){
        return nk_switch_to_vc(cur_vc);
    }
    */
    if(cur_node.next == NULL) {
        return 0;
    }
    target = container_of(cur_node.next, struct virtual_console, vc_node);
    return nk_switch_to_vc(target);
}


// display scrolling or explicitly on screen at a given location
int nk_put_char(uint8_t c) {
    struct virtual_console *vc = get_cur_thread()->vc;
    if (!vc) { 
      vc = default_vc;
    }
    if (c == '\n') {
        vc->cur_x = 0;
        if (++vc->cur_y == VGA_HEIGHT) {
            vc_scrollup(vc);
            vc->cur_y--;
        }
        return 0;
    }
    nk_display_char(c, vc->cur_attr, vc->cur_x, vc->cur_y);
    if (++vc->cur_x == VGA_WIDTH) {
        vc->cur_x = 0;
        if (++vc->cur_y == VGA_HEIGHT) {
            vc_scrollup(vc);
            vc->cur_y--;
        }
    }
    return 0;
}

int nk_puts(char *s) {
    while(*s) {
        nk_put_char(*s);
        s++;
    }
    putchar('\n');
    return 0;
}

int nk_set_attr(uint8_t attr) {
    get_cur_thread()->vc->cur_attr = attr;
    return 0;
}

uint16_t make_vgaentry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

int vc_scrollup (struct virtual_console *vc) 
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

int nk_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y) {
    printk("vc:display %d %d at %d %d\n",c,attr,x,y);
    uint16_t val = make_vgaentry (c, attr);
    if(x >= VGA_WIDTH || y >= VGA_HEIGHT) {
        return -1;
    } else {
        struct virtual_console *thread_vc = get_cur_thread()->vc;
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

int next_index_on_queue(enum VC_TYPE type, int index) {
    if(type == RAW) {
        return (index + 1) % Scancode_QUEUE_SIZE;
    } else {
        return (index + 1) % Keycode_QUEUE_SIZE;
    }
}

bool is_queue_empty(struct virtual_console *vc) {
    return vc->head == vc->tail;
}

bool is_queue_full(struct virtual_console *vc) {
    return next_index_on_queue(vc->type, vc->tail) == vc->head;
}

int nk_enqueue_scancode(struct virtual_console *vc, Scancode scan) {
    if(vc->type != RAW || is_queue_full(vc)) {
        return -1;
    } else {
        vc->keyboard_queue.s_queue[vc->tail] = scan;
        vc->tail = next_index_on_queue(vc->type, vc->tail);
        nk_thread_queue_wake_all(vc->waiting_threads);
    }
    return 0;
}

int nk_enqueue_keycode(struct virtual_console *vc, Keycode key) {
    if(vc->type != COOKED || is_queue_full(vc)) {
        return -1;
    } else {
        vc->keyboard_queue.k_queue[vc->tail] = key;
        vc->tail = next_index_on_queue(vc->type, vc->tail);
        nk_thread_queue_wake_all(vc->waiting_threads);
    }
    return 0;
}

Scancode nk_dequeue_scancode(struct virtual_console *vc) {
    if(vc->type != RAW || is_queue_empty(vc)) {
        printk("Error\n");
        return -1;
    } else {
        Scancode result;
        result = vc->keyboard_queue.s_queue[vc->head];
        vc->head = next_index_on_queue(vc->type, vc->head);
        return result;
    }
}

Keycode nk_dequeue_keycode(struct virtual_console *vc) {
    if(vc->type != COOKED || is_queue_empty(vc)) {
        printk("Error\n");
        return -1;
    } else {
        Keycode result;
        result = vc->keyboard_queue.k_queue[vc->head];
        vc->head = next_index_on_queue(vc->type, vc->head);
        return result;
    }
}


void nk_vc_wait()
{
    nk_thread_t * cur    = get_cur_thread();
    nk_thread_queue_t * wq = cur->vc->waiting_threads;

    /* make sure we're not putting ourselves on our 
     * own waitq */
    ASSERT(!irqs_enabled());
    ASSERT(wq != cur->waitq);

    enqueue_thread_on_waitq(cur, wq);
    nk_schedule();
}


Keycode nk_get_char() {
top: ;
    struct virtual_console *vc = get_cur_thread()->vc;
    if(vc->type != COOKED) {
        printk("Error\n");
        return -1;
    }
    if(!is_queue_empty(vc)) {
        return nk_dequeue_keycode(vc);
    } else {
        nk_vc_wait();
        goto top;
    }
}
Scancode nk_get_scancode(){
top: ;
    struct virtual_console *vc = get_cur_thread()->vc;
    if(vc->type != RAW) {
        printk("Error");
        return -1;
    }
    if(!is_queue_empty(vc)) {
        return nk_dequeue_scancode(vc);
    } else {
        nk_vc_wait();
        goto top;
    }
}

int keycode_translator(struct virtual_console *cur_vc, uint8_t scan){
    Keycode key = kbd_translator(scan);
    if(key != 0x0000) {
        nk_enqueue_keycode(cur_vc, key);
    }
    return 0;
}

int nk_en_console(uint8_t scan) {
    if(cur_vc->type == RAW) {
        nk_enqueue_scancode(cur_vc, scan);
    } else {
        keycode_translator(cur_vc, scan);
    }
    return 0;
}

int nk_vc_init() {
    printk("vc: init\n");
    //INIT_LIST_HEAD(&vc_list);// Used in nk_creat_default_vc()
    default_vc = nk_create_default_vc(COOKED);
    if(!default_vc) {
        printk("Error:");
        return -1;
    }
    cur_vc = default_vc;
    copy_display_to_vc(cur_vc);
    nk_puts("Welcome to Nautilus Virtual Console System.\n--Yang Wu, Fei Luo, Yuanhui Yang.");
    return 0;
}

