#ifndef __NK_VC
#define __NK_VC

typedef ushort_t Keycode;
typedef uint8_t Scancode;

enum VC_TYPE {RAW, COOKED};
struct virtual_console;

struct virtual_console *nk_create_vc(enum VC_TYPE new_vc_type); 
int nk_destroy_vc(struct virtual_console *vc);

int nk_bind_vc(struct nk_thread *thread, struct virtual_console * cons, void (*newdatacallback)() );
int nk_release_vc(struct nk_thread *thread);

int nk_switch_to_vc(struct virtual_console *vc);
int nk_switch_to_prev_vc();
int nk_switch_to_next_vc();

int nk_put_char(uint8_t c);
int nk_puts(char *s); 
int nk_set_attr(uint8_t attr);
int vc_scrollup (struct virtual_console *vc);
int nk_display_char(uint8_t c, uint8_t attr, uint8_t x, uint8_t y);

int nk_enqueue_scancode(struct virtual_console *vc, Scancode scan);
int nk_enqueue_keycode(struct virtual_console *vc, Keycode key);
Scancode nk_dequeue_scancode(struct virtual_console *vc);
Keycode nk_dequeue_keycode(struct virtual_console *vc);
Keycode nk_get_char();
Scancode nk_get_scancode();

int nk_en_console(uint8_t scan);
//int keycode_translator(struct virtual_console *cur_vc, uint8_t scan);

int nk_vc_init();
#endif
