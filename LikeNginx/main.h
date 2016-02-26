#ifndef MAIN_H__
#define MAIN_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "lock.h" 




struct gs_main_cycle_s{
	gs_mutex_t accept_mutex; // worker will get lock then process accept for new connect
	

	int accept_mutex_held; //if this value is 1 then have other process hold it
	int accept_fd; //master bind and listen socket fd ...will be accept on child process
};
typedef struct gs_main_cycle_s gs_main_cycle_t;


//global variable ...


typedef enum {MASTER_PROCESS,WORKER_PROCESS} process_t; 

process_t gs_process;
extern sig_atomic_t process_shutdown ;
extern sig_atomic_t process_terminate;
extern sig_atomic_t handle_sig_alarm ;
extern sig_atomic_t handle_sig_hup ;
extern sig_atomic_t handle_sig_chld;

#endif
