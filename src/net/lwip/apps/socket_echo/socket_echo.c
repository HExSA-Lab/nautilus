
#include "socket_echo.h"

#include "lwip/opt.h"

#if LWIP_SOCKET

#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <nautilus/nautilus.h>
#include <nautilus/naut_string.h>


#define PORT 7

static void sock_echo(void *arg)
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
	while (read(con_sock,&c,1)==1) {
	    write(con_sock,&c,1);
	}
	close(con_sock);
    }
}

void socket_echo_init(void)
{
  sys_thread_new("sock_echo", sock_echo, NULL, 0, 0);
}

#endif /* LWIP_SOCKETS */
