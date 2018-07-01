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
 * Copyright (c) 2018, Peter Dinda
 * Copyright (c) 2018, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors:   Peter Dinda  <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */


#include "ipvcd.h"

#include "lwip/opt.h"

#if LWIP_SOCKET

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <nautilus/nautilus.h>
#include <nautilus/naut_string.h>
#include <nautilus/chardev.h>
#include <nautilus/vc.h>


//#define DEBUG(fmt, args...) DEBUG_PRINT("ipvcd: " fmt, ##args)
#define DEBUG(fmt,args...)

#define PORT 23

static uint64_t count=0;

struct ipvc {
    char                 name[32];
    int                  sock;
    struct nk_char_dev  *cdev;
    // messages are just chars
    struct nk_msg_queue *in;
    struct nk_msg_queue *out;
};


static int do_get_characteristics(void *state, struct nk_char_dev_characteristics *c)
{
    struct ipvc *vc = (struct ipvc *) state;
    return 0;
}

static int do_status(void *state)
{
    struct ipvc *vc = (struct ipvc *) state;
    int rc = 0;
    if (!nk_msg_queue_empty(vc->in)) {
	rc |= NK_CHARDEV_READABLE;
    }
    if (!nk_msg_queue_full(vc->out)) {
	rc |= NK_CHARDEV_WRITEABLE;
    }
    return rc;
}

static int do_read(void *state, uint8_t *dest)
{
    struct ipvc *vc = (struct ipvc *) state;

    void *msg;

    
    DEBUG("do_read\n");
    if (!nk_msg_queue_try_pull(vc->in,&msg)) {
	*dest = (uint8_t) (uint64_t) msg;
	DEBUG("do_read success - %c\n",*dest);
	return 1;
    } else {
	DEBUG("do_read no data\n");
	//nk_sched_yield(0);
	return 0;
    }
}

static int do_write(void *state, uint8_t *src)
{
    struct ipvc *vc = (struct ipvc *) state;

    void *msg = (void*)(uint64_t)(*src);

    DEBUG("do_write of %c\n",*src);

    if (!nk_msg_queue_try_push(vc->out,msg)) {
	DEBUG("do_write success\n");
	return 1;
    } else {
	DEBUG("do_write no space\n");
	//nk_sched_yield(0);
	return 0;
    }
}

static struct nk_char_dev_int ops = {
    .get_characteristics = do_get_characteristics,
    .read = do_read,
    .write = do_write,
    .status = do_status
};

    

static void handle(void *arg)
{
    struct ipvc *vc = (struct ipvc *)arg;

    // finish creating the vc
    DEBUG("handling vc - sock=%d\n",vc->sock);
    
    // we have message queues
    vc->in = nk_msg_queue_create(0,128,0,0);
    if (!vc) {
	ERROR("Failed to allocate input msg queue\n");
	goto out;
    }
    vc->out = nk_msg_queue_create(0,128,0,0);
    if (!vc) {
	ERROR("Failed to allocate output msg queue\n");
	goto out;
    }

    // we are a chardev
    vc->cdev = nk_char_dev_register(vc->name,0,&ops,vc);
    if (!vc->cdev) {
	ERROR("Failed to register as chardev\n");
	goto out;
    }

    // we need to be sure we have a non-blocking socket
    // since we do not have select shared with non-LWIP stuff...
    u32_t opt  = fcntl(vc->sock,F_GETFL,0);
    opt |= O_NONBLOCK;
    fcntl(vc->sock,F_SETFL,opt);

    DEBUG("start console\n");
    
    // and we need support on the vc side
    if (nk_vc_start_chardev_console(vc->name)) {
	ERROR("Failed to start chardev console\n");
	goto out;
    }

    // And now we will simply pump bytes both ways
    // burning CPU as we go...
    while (1) {
	char c;
	void *msg=0;
	int had_data=0;
	if (read(vc->sock,&c,1)==1) {
	    // pump data 
	    DEBUG("data in %c\n",c);
	    msg = (void*)(uint64_t)c;
	    if (!nk_msg_queue_try_push(vc->in,msg)) { 
		nk_dev_signal((struct nk_dev *)vc->cdev);
	    }
	    DEBUG("pushed input\n");
	    had_data=1;
	}
	if (!nk_msg_queue_try_pull(vc->out,&msg)) {
	    c = (char)(uint64_t)msg;
	    DEBUG("data out %c\n",c);
	    write(vc->sock,&c,1);
	    nk_dev_signal((struct nk_dev *)vc->cdev);
	    DEBUG("pushed output\n");
	    had_data=1;
	}
	if (!had_data) {
	    nk_sched_yield(0);
	}
    }
	
 out:
    if (vc) {
	if (vc->in) { nk_msg_queue_release(vc->in); vc->in=0; }
	if (vc->out) { nk_msg_queue_release(vc->in); vc->out=0; }
	close(vc->sock); vc->sock=-1;
    }
    
}
    

static void ipvcd(void *arg)
{
    
    
    int acc_sock;
    int con_sock;
    struct sockaddr_in addr;
    struct sockaddr_in src_addr;
    socklen_t src_len;
    char c;

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = PP_HTONS(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    acc_sock = socket(AF_INET,SOCK_STREAM,0);

    if (acc_sock<0) {
	printf("Failed to create socket\n");
	return;
    }


    if (bind(acc_sock,(const struct sockaddr *)&addr,sizeof(addr))<0) {
	printf("Failed to bind socket\n");
	return;
    }


    if (listen(acc_sock,5)<0) {
	printf("Failed to listen on socket\n");
	return;
    }

    
    while (1) {
	src_len=sizeof(src_addr);
	con_sock = accept(acc_sock,(struct sockaddr *)&src_addr,&src_len);
	if (con_sock<0) {
	    printf("Failed to accept connection\n");
	    return ;
	}
	
	struct ipvc *vc = malloc(sizeof(*vc));
	if (!vc) {
	    ERROR("Cannot allocate for new connection\n");
	    close(con_sock);
	    continue;
	}
	
	snprintf(vc->name,32,"ipvcd-%lu",__sync_fetch_and_add(&count,1));
	vc->sock = con_sock;
	
	sys_thread_new(vc->name,handle,(void*)vc,0,0);
	
    }
}

    
void ipvcd_init(void)
{
    sys_thread_new("ipcvd", ipvcd, NULL, 0, 0);
}

#endif /* LWIP_SOCKETS */
