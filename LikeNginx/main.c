/*--------------------------------------------------------------------------
 Copyright 2012, stone li liqinglin@gmail.com/

 This net work server  is free software; you can redistribute it and/or
                         ______________________
			|                      |
		        |Master manager process|
		        |______________________|

                              /   \
                             /     \
                            /       \
                           /         \
               -----------------      -----------------
              | worker process |     | worker process |
              |                |     |                | 
              |  accept con    |     |  accept con    |
              |  do something  |     |  do something  |   ......worker chain base on cpus number
              |  on a con      |     |  on a con      |
              |  EPOLL event   |     |  EPOLL event   |
              |________________|     |________________|

--------------------------------------------------------------------------*/








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



#include "main.h"
#include "epolllib.h"
#include "worker.h"
#include "sock.h"

//global variable
sig_atomic_t process_shutdown=0 ;
sig_atomic_t process_terminate=0;
sig_atomic_t handle_sig_alarm =0;
sig_atomic_t handle_sig_hup =0;
sig_atomic_t handle_sig_chld=0;


//static variable

static int fork_loop=1;


static void signal_handler(int sig) {

	switch(gs_process){
		
		case MASTER_PROCESS:

			switch(sig){
				case SIGTERM:
					process_shutdown=1;break;
				case SIGINT:
					printf("this is master...\n");
					process_terminate=1;break;
				
			}	
			break;


		case WORKER_PROCESS:
			switch(sig){
				case SIGINT:
					printf("this is child\n");
					break;
			}
			break;
	}



	if(sig==SIGCHLD){
		int pid,status;
		handle_sig_chld=0;

		while((pid=waitpid(-1,&status,WNOHANG))>0)
			handle_sig_chld++;
	}

}

static void daemonize(void) {
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	if (0 != fork()) exit(0);
	if (-1 == setsid()) exit(0);
	signal(SIGHUP, SIG_IGN);
	if (0 != fork()) exit(0);
	if (0 != chdir("/")) exit(0);
}


gs_main_cycle_t *gs_main_cycle_init()
{
	gs_main_cycle_t *cycle=NULL;
	int ret=-1;

	/* set share memory arg ...cycle struct */

	cycle=(gs_main_cycle_t *)mmap(NULL, sizeof(gs_main_cycle_t), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	if(cycle== MAP_FAILED) return cycle;

	/*set mutex for all process */
	ret=gs_process_lock_init(&(cycle->accept_mutex));
	if(ret!=0) {
		munmap(cycle,sizeof(gs_main_cycle_t));
		cycle=NULL;
	}
	cycle->accept_mutex_held=0;

	/****/


	return cycle;
}

static int get_child_num(int cpus)
{
	
	return cpus;
}


static void set_signal()
{
        signal(SIGPIPE, SIG_IGN);
        signal(SIGUSR1, SIG_IGN);
        signal(SIGTERM, signal_handler);
        signal(SIGHUP,  signal_handler);
        signal(SIGCHLD,  signal_handler);
        signal(SIGINT,  signal_handler);
}


static void process_signal_return()
{
	if(process_shutdown==1){
		fork_loop=0;
	}	

	if(process_terminate==1){
		;
	}	

	if(handle_sig_hup==1){
		;
	}

	return;
}



int main(int argc,char **argv)
{

	gs_main_cycle_t *cycle=NULL;
	int child=0; //child ==0 means this process zone is parent; and child ==1 menas this is child
	int num_child=0;
	sigset_t zeromask;


/*************************************************************
 *  Master 管理进程工作内容                                  *
 *                                                           *
 *************************************************************/


	/* 参数处理 */
	if(argc!=2) 
	{
		fprintf (stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}
	

	/* config  or cycle 处理*/
	cycle=gs_main_cycle_init();	
	if(cycle==NULL){
		exit(EXIT_FAILURE);
	}

	//daemonize();

	/* 系统资源处理 */
	int cpus=4;

	/* stone_init_cycle 全局环境变量处理*/	

	/* 处理信号相关 --和worker进程通信需要*/
	set_signal();
        sigemptyset(&zeromask);


	/* 网络初始化 listen port */
	cycle->accept_fd=initTcpServer((unsigned int)atoi(argv[1]));

	if (cycle->accept_fd == -1) {
		abort ();
	}
	setnonblocking(cycle->accept_fd);



        /* Master 创建worker 进程 */
	num_child=get_child_num(cpus);

	if(num_child<=0){
		printf("Hi...are you sure don't fork new child?\n");
		printf("This main process will exit...\n");
		exit(0);		
	}


	gs_process=MASTER_PROCESS;
	

	{
		child=0;
		while(!child&&fork_loop){
			if(num_child>0){
				switch(fork()){
					case -1:
						printf("main ...fork error..\n");
						return -1;
					case 0:
						child=1; 
						gs_process=WORKER_PROCESS; 
						break;
					default:
						num_child--;
						break;
				}//switch

			}else{//if num _child<=0

				/****** master process do someting...**************/
				sigsuspend(&zeromask);
				if(handle_sig_chld){ //process worker thread exit signal
					num_child+=handle_sig_chld;
					handle_sig_chld=0;

				}
				process_signal_return();

			}//if
		}//while
	}//

	/****for the child zone....child will do *****/
	if(child){

		printf("Worker thread start work....[%d]\n",getpid());
		gs_worker_process_cycle(cycle);

		_exit(0);
	}





	/***************************************************/
	printf("Master will exit....\n");

	//destory();
	exit(0);

}
