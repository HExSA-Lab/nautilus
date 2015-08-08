#ifndef __NEMO_H__
#define __NEMO_H__


#define NEMO_MAX_EVENTS 1024
#define NEMO_INT_VEC    0xe8


typedef void (*nemo_action_t)(void);

typedef int nemo_event_id_t;

typedef struct nemo_event {
	nemo_action_t   action;
	nemo_event_id_t id;
	void *          priv_data;
} nemo_event_t;


int nemo_init(void);
nemo_event_id_t nemo_register_event_action(void (*func)(void), void * priv_data);
void nemo_unregister_event_action(nemo_event_id_t eid);
void nemo_event_notify(nemo_event_id_t eid, int cpu);
void nemo_event_broadcast(nemo_event_id_t eid);
void nemo_event_await(void);




#endif /* !__NEMO_H__! */
