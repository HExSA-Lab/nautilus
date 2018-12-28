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
#include <nautilus/timer.h>



//#define IPVCD_DEBUG(fmt, args...) INFO_PRINT("ipvcd: " fmt, ##args)
#define IPVCD_ERROR(fmt, args...) ERROR_PRINT("ipvcd: " fmt, ##args)
#define IPVCD_INFO(fmt, args...) INFO_PRINT("ipvcd: " fmt, ##args)
#define IPVCD_DEBUG(fmt,args...)

#define PORT 23

#define MQ_SIZE 16*80*25  // 16 screens worth

static uint64_t count=0;

struct ipvc {
    struct nk_char_dev   *cdev;
    int                  shutdown;
    char                 name[32];
    int                  sock;
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
    if (vc->shutdown) {
	rc |= NK_CHARDEV_READABLE | NK_CHARDEV_WRITEABLE;
    }
    return rc;
}

static int do_read(void *state, uint8_t *dest)
{
    struct ipvc *vc = (struct ipvc *) state;

    void *msg;

    //IPVCD_DEBUG("do_read\n");
    if (vc->shutdown) {
	return -1;
    }
    if (!nk_msg_queue_try_pull(vc->in,&msg)) {
	*dest = (uint8_t) (uint64_t) msg;
	IPVCD_DEBUG("do_read success - %c\n",*dest);
	return 1;
    } else {
	IPVCD_DEBUG("do_read no data\n");
	//nk_sched_yield(0);
	return 0;
    }
}
static int do_write(void *state, uint8_t *src)
{
    struct ipvc *vc = (struct ipvc *) state;

    void *msg = (void*)(uint64_t)(*src);

    //IPVCD_DEBUG("do_write of %c\n",*src);
    if (vc->shutdown) {
	return -1;
    }
    if (!nk_msg_queue_try_push(vc->out,msg)) {
	//IPVCD_DEBUG("do_write success\n");
	return 1;
    } else {
	//IPVCD_DEBUG("do_write no space\n");
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

    // finish creating th vc
    IPVCD_DEBUG("handling vc - sock=%d\n",vc->sock);
    
    // we have message queues
    vc->in = nk_msg_queue_create(0,MQ_SIZE,0,0);
    if (!vc) {
	IPVCD_ERROR("Failed to allocate input msg queue\n");
	goto out;
    }
    vc->out = nk_msg_queue_create(0,MQ_SIZE,0,0);
    if (!vc) {
	IPVCD_ERROR("Failed to allocate output msg queue\n");
	goto out;
    }

    // we are a chardev
    vc->cdev = nk_char_dev_register(vc->name,0,&ops,vc);
    if (!vc->cdev) {
	IPVCD_ERROR("Failed to register as chardev\n");
	goto out;
    }

    // we need to be sure we have a non-blocking socket
    // since we do not have select shared with non-LWIP stuff...
    u32_t opt  = fcntl(vc->sock,F_GETFL,0);
    opt |= O_NONBLOCK;
    fcntl(vc->sock,F_SETFL,opt);

    IPVCD_DEBUG("start console\n");
    
    // and we need support on the vc side
    if (nk_vc_start_chardev_console(vc->name)) {
	IPVCD_ERROR("Failed to start chardev console\n");
	goto out;
    }


    int had_data;
    
    // And now we will simply pump bytes both ways
    // burning CPU as we go...
    while (1) {
	char c;
	int rc;
	void *msg=0;
	
	had_data=0;
	rc = read(vc->sock,&c,1);
	// we either got a byte of data or we are in an EAGAIN
	// anything else should cause us to terminate the connection
	if (rc==1) {
	    // pump data 
	    IPVCD_DEBUG("data in %c (%x)\n",c,c);
	    msg = (void*)(uint64_t)c;
	    if (!nk_msg_queue_try_push(vc->in,msg)) { 
		nk_dev_signal((struct nk_dev *)vc->cdev);
	    }
	    IPVCD_DEBUG("pushed input\n");
	    had_data=1;
	} else if (!(rc<0 && errno==EAGAIN)) { 
	    IPVCD_DEBUG("Closing session rc=%d errno=%d\n",rc,errno);
	    goto endsession;
	} // otherwise we are in EGAIN and proceed
	if (!nk_msg_queue_try_pull(vc->out,&msg)) {
	    c = (char)(uint64_t)msg;
	    IPVCD_DEBUG("data out %c (%x)\n",c,c);
	    // the write could do an EGAIN here, in which
	    // case we lose the byte
	    write(vc->sock,&c,1);
	    nk_dev_signal((struct nk_dev *)vc->cdev);
	    IPVCD_DEBUG("pushed output\n");
	    had_data=1;
	}
	if (!had_data) {
	    IPVCD_DEBUG("no data - sleep\n");
	    nk_sleep(1000000); // no data => do nothing for 1ms
	    IPVCD_DEBUG("no data - awaken\n");
	} else {
	    IPVCD_DEBUG("had data\n");
	}
    }

 endsession:
    // the connection is closed or we have some other error
    // we cannot currently destruct a chardev device or
    // a chardev console, so these are leaked at this point
    __sync_fetch_and_or(&vc->shutdown,1);
    nk_vc_stop_chardev_console(vc->name);

    // we are leaking the device...
	
 out:
    if (vc) {
	if (vc->in) { nk_msg_queue_release(vc->in); vc->in=0; }
	if (vc->out) { nk_msg_queue_release(vc->out); vc->out=0; }
	close(vc->sock); vc->sock=-1;
    }

    IPVCD_DEBUG("exiting per-connection thread\n");
    
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
	IPVCD_ERROR("Failed to create socket\n");
	return;
    }


    if (bind(acc_sock,(const struct sockaddr *)&addr,sizeof(addr))<0) {
	IPVCD_ERROR("Failed to bind socket\n");
	return;
    }


    if (listen(acc_sock,5)<0) {
	IPVCD_ERROR("Failed to listen on socket\n");
	return;
    }

    IPVCD_DEBUG("Accepting connections\n");

    
    while (1) {
	src_len=sizeof(src_addr);
	con_sock = accept(acc_sock,(struct sockaddr *)&src_addr,&src_len);
	if (con_sock<0) {
	    IPVCD_ERROR("Failed to accept connection\n");
	    return ;
	}

	IPVCD_DEBUG("New connection\n");
	
	struct ipvc *vc = malloc(sizeof(*vc));
	if (!vc) {
	    IPVCD_ERROR("Cannot allocate for new connection\n");
	    close(con_sock);
	    continue;
	}

	memset(vc,0,sizeof(*vc));
	
	snprintf(vc->name,32,"ipvcd-%lu",__sync_fetch_and_add(&count,1));
	vc->sock = con_sock;
	
	sys_thread_new(vc->name,handle,(void*)vc,0,0);

	IPVCD_DEBUG("Launched session thread %s for new connection\n", vc->name);
	
    }
}

    
void ipvcd_init(void)
{
    sys_thread_new("ipcvd", ipvcd, NULL, 0, 0);
    INFO("inited\n");
}

#endif /* LWIP_SOCKETS */
