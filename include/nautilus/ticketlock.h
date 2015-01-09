#ifndef __TICKET_LOCK_H__
#define __TICKET_LOCK_H__


#ifdef __cplusplus
extern "C" {
#endif


union nk_ticket_lock {
    uint32_t val;

    struct {
        uint16_t ticket;
        uint16_t users;
    } lock;
};

typedef union nk_ticket_lock nk_ticket_lock_t;

void nk_ticket_lock_init(nk_ticket_lock_t * l);
void nk_ticket_lock_deinit(nk_ticket_lock_t * l);
void nk_ticket_lock(nk_ticket_lock_t * l);
void nk_ticket_unlock(nk_ticket_lock_t * l);
int nk_ticket_trylock(nk_ticket_lock_t * l);


#ifdef __cplusplus
}
#endif


#endif
