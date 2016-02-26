#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>

#include "worker.h"
#include "main.h"
#include "lock.h"
#include "epolllib.h"


struct gs_sys_epoll *workers=NULL;
static gs_main_cycle_t *cycle=NULL;
worker_cycle_t worker_cycle;



#define MAXFDS 256
#define TURE	1

static int worker_max_tasks=1024;

int *joblist=NULL;

int connect_io_callback(gs_pollevent_t *event);

 
int gs_worker_process_init(gs_main_cycle_t *c)
{
	workers=(struct gs_sys_epoll*)malloc(sizeof(struct gs_sys_epoll));
	if(workers==NULL) {
		return ENOMEM;		
	}
	gs_sys_epoll_init(workers,MAXFDS);

	cycle=c;


	worker_cycle.worker_max_connects=1000;
	worker_cycle.worker_free_connects=1000;
	worker_cycle.worker_accept_disabled=(worker_cycle.worker_max_connects/8)-worker_cycle.worker_free_connects;

	worker_cycle.worker_accept_available=100;
	worker_cycle.worker_accept_mutex_delay=50000;


	joblist=(int*)malloc(sizeof(int)*worker_max_tasks);
	if(joblist==NULL) return ENOMEM;

	{
		int n;

		for(n=0;n<worker_max_tasks;n++) joblist[n]=-1;
	}

	return;
}

void gs_worker_process_events(gs_main_cycle_t *cycle)
{

	int timer =worker_cycle.worker_accept_mutex_delay;

	gs_sys_epoll_waitAndDispatchEvents(workers, timer);

	return;
}

void gs_worker_process_destory(gs_main_cycle_t *cycle)
{

	if(joblist) {free(joblist);joblist=NULL;}
	return;
}


//***********************eventcallback process function***********************/
int accept_fd_callback(gs_pollevent_t *event)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int fd,infd;
	int accept_available;


	fd=event->fd;
	accept_available=worker_cycle.worker_accept_available;

	do{
		in_len = sizeof in_addr;
		infd = accept (fd, &in_addr, &in_len);

		if (infd == -1){
			
			if (errno == EAGAIN){
				/* We have processed all incoming connections. */
                                break;
			}else if (errno == EWOULDBLOCK){
				accept_available--;
				if(accept_available>0) continue;

			}else { //other error
				break;
			}
			
		}

		connection_set_state(workers,infd,CON_STATE_REQUEST_START);
		worker_cycle.worker_free_connects--;
		accept_available--;
		connection_state_machine(workers,infd);

		
	}while(accept_available>0);

	gs_sys_epoll_del(workers,fd); //delete from accepter epoll
	cycle->accept_mutex_held=0;

	worker_cycle.worker_accept_disabled=(worker_cycle.worker_max_connects/8)-worker_cycle.worker_free_connects;

        gs_process_unlock(&(cycle->accept_mutex));


	

	return 1;
}

int reader_fd_sycle(int fd)
{
	
	int n,len;
	int n_read = 0;


	if(workers->m_fds[fd].is_readable==0) return -1;

	/* OS doesn't *have* to give us any more events on this fd
         * unless we keep reading until a short read or EWOULDBLOCK.
         */

        do{
        	n = read(fd,(char*) workers->m_fds[fd].data+n_read,MAX_BUFFER_LEN);

                if (n > 0){
			workers->m_fds[fd].is_readable=0;
                        n_read += n;

                }else if (n == -1){
                                /* If errno == EAGAIN, that means we have read all data. So go back to the main loop. */

			workers->m_fds[fd].is_readable=0;

                        if (errno == EAGAIN){
				break;
                        }
			if(errno==EINTR){
				/* we have been interrupted before we could read */
            			workers->m_fds[fd].is_readable = 1;
				break;
			}
			if (errno != ECONNRESET) {
				/* expected for keep-alive */
				;
			}
			connection_set_state(workers,fd, CON_STATE_ERROR);
			break;

                 } else if (n == 0){  //对端关闭了连接

			workers->m_fds[fd].is_readable=0;	
                         /* End of file. The remote has closed the connection. */
                          break;
                 }


	} while (n_read<MAX_BUFFER_LEN);

	((char*)(workers->m_fds[fd].data))[n_read]='\0';
	workers->m_fds[fd].size=n_read;

	return n_read;
	
}


int writer_fd_sycle(int fd)
{
        int n;
        int len;
	int n_write=0;



	if(workers->m_fds[fd].is_writable==0) return -1;


        len=workers->m_fds[fd].size;

        while(len>0){
        	n=write(fd,(workers->m_fds[fd].data)+n_write,len);
                if (n>0) {

			workers->m_fds[fd].is_writable=0;
			n_write+=n;  
			len-=n;

		}else if(n==-1){ 

			workers->m_fds[fd].is_writable=0;

			if (errno == EAGAIN){
                                break;
                        }
                        if(errno==EINTR){
                                /* we have been interrupted before we could read */
                                workers->m_fds[fd].is_writable = 1;
                                break;
                        }
                        if (errno != ECONNRESET) {
                                /* expected for keep-alive */
                                ;
                        }
                        connection_set_state(workers,fd, CON_STATE_ERROR);
                        break;


                 }else if(n==0) {
			workers->m_fds[fd].is_writable = 0;
			break;
                 }
	}

	return n_write;

	
}



//** 下面这个callback主要是设置state****/

int connect_io_callback(gs_pollevent_t *event)
{
	int fd;
	int n;
	int len;


	fd=event->fd;

	/****add to job list **********/  
	//joblist_append(srv, con);

	joblist[fd]=fd;


	if((event->revents & EPOLLIN)){ //ready read
		workers->m_fds[fd].is_readable=1;
	
	}//read

	if((event->revents & EPOLLOUT)){ //ready write  
		workers->m_fds[fd].is_writable=1;
	
        }//write


	if (event->revents & ~(EPOLLIN |EPOLLOUT)) {
		printf("Epoll event error .....\n");
		connection_set_state(workers,fd,CON_STATE_ERROR);
	}	

	if(workers->m_fds[fd].state==CON_STATE_READ){
		if(reader_fd_sycle(fd)>0)

		/**检查缓冲数据是否请求完整，设置状态*/
		//if(request_is_full){
			connection_set_state(workers,fd,CON_STATE_REQUEST_END);;
		//}
		//printf("Read[%d] Buffer=[%s]\n",fd,workers->m_fds[fd].data);
	}

	if(workers->m_fds[fd].state==CON_STATE_WRITE){

		if(writer_fd_sycle(fd)>0)

                //if(request_is_full){
                        connection_set_state(workers,fd,CON_STATE_RESPONSE_END);;
                //}
                //printf("write Buffer=[%s]\n",workers->m_fds[fd].data);
	}


	if(workers->m_fds[fd].state==CON_STATE_CLOSE){
		//free this connect

		if(workers->m_fds[fd].data) {
			free(workers->m_fds[fd].data);
			workers->m_fds[fd].data=NULL;
		}
		worker_cycle.worker_free_connects++;
		gs_sys_epoll_del(workers,fd);
		close(fd);
		joblist[fd]=-1;

	}
	
	//connection_state_machine(workers,fd);	

}


int gs_worker_process_cycle(gs_main_cycle_t *c)
{


	/********************************************************
	 *           初始化工作                                 *
         *                                                      *
         ********************************************************/
	
	gs_worker_process_init(c);
	cycle->accept_mutex_held=0;



	/********************************************************
	 *                         main cycle                   * 
         *                                                      *
         *******************************************************/

	for(;;){

		if(worker_cycle.worker_accept_disabled>0){ //have't enough free connect ..don't accept new 
		 	printf("debug....2\n");
			worker_cycle.worker_accept_disabled--;

		}else{
		
			if(cycle->accept_mutex_held==0){ //no worker lock and process this accept fd
				if(gs_process_trylock(&(cycle->accept_mutex))==0){//get lock
					gs_sys_epoll_prepare_fd_for_add(cycle->accept_fd, getpid());
					gs_sys_epoll_add(workers,cycle->accept_fd, accept_fd_callback,NULL);
					cycle->accept_mutex_held=1;

				}	
			}
		}


		
		/**********************************************************
                 *    event process (epoll)  epoll_wait()                 *
                 **********************************************************/
	
		gs_worker_process_events(cycle);



		/***********loop job list****/
		{
			int index=0;

			for(index=0;index<worker_max_tasks;index++){
				if(joblist[index]>0){
					connection_state_machine(workers,index);
				}
			}

		}


		
		
	}//main loop cycle

	
	/**********************************************************
         *        worker destory                                  *
         *                                                        *
         **********************************************************/

	gs_sys_epoll_shutdown(workers);
	gs_worker_process_destory(cycle);
	


	return 0;
}
