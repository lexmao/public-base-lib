#ifndef CONNECT_STATE
#define CONNECT_STATE
#include "main.h"
#include "epolllib.h"



extern void connection_set_state(struct gs_sys_epoll *connects,int fd,connection_state_t state);
extern void print_con_state(struct gs_sys_epoll *connects,int fd);

#endif
