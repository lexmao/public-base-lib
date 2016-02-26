#include "connect_state.h"
#include "worker.h"
#include "epolllib.h"
#include "task_auth.h"

void connection_set_state(struct gs_sys_epoll *connects,int fd,connection_state_t state)
{
	if(fd<0) return;
	if(!connects) return;
	if(!connects->m_fds) return;	

	connects->m_fds[fd].state=state;	
}


void connection_state_machine(struct gs_sys_epoll *connects,int fd)
{
	connection_state_t state;	
	int done=0;

	if(!connects) return;
        if(!connects->m_fds) return;



	while(done==0){
//		print_con_state(connects,fd);
		state=connects->m_fds[fd].state;
		switch(state){
			case CON_STATE_REQUEST_START: 


				/**add event to epolls**/
				gs_sys_epoll_prepare_fd_for_add(fd, getpid());
                        	gs_sys_epoll_add(connects,fd, connect_io_callback,NULL);


				/*init data buffer */
				if(connects->m_fds[fd].data==NULL){
                        		connects->m_fds[fd].data=(char*)malloc(MAX_BUFFER_LEN);
                        		memset(connects->m_fds[fd].data,sizeof(char),MAX_BUFFER_LEN);
                        		connects->m_fds[fd].size=0;
                		}

				/**set this connect state to CON_STATE_READ*/
				connection_set_state(connects, fd,CON_STATE_READ);
				connects->m_fds[fd].is_readable=0;
				connects->m_fds[fd].is_writable=0;		

				/*****process greet funciton to client ************/
				{
			
					server_greet(NULL,0,connects->m_fds[fd].data,&(connects->m_fds[fd].size));
					write(fd,connects->m_fds[fd].data,connects->m_fds[fd].size);
					
				}

				break;

			case CON_STATE_READ:

				if(reader_fd_sycle(fd)>0)
				connection_set_state(connects, fd,CON_STATE_REQUEST_END);

				break;
	
			case CON_STATE_REQUEST_END:
				{

					int r;
					char output[128]={0};
					int outlen;

					r=command_parse(connects->m_fds[fd].data,connects->m_fds[fd].size,output,&outlen);
					
					connection_set_state(connects, fd, CON_STATE_HANDLE_REQUEST);
					strncpy(connects->m_fds[fd].data,output,outlen);
					connects->m_fds[fd].size=outlen;
						
				}
				break;

			case  CON_STATE_HANDLE_REQUEST:

				{
				
					connection_set_state(connects, fd, CON_STATE_RESPONSE_START);
				}
				break;

			case CON_STATE_RESPONSE_START:
				connection_set_state(connects, fd, CON_STATE_WRITE);
				break;
			case CON_STATE_WRITE:

				if(writer_fd_sycle(fd)>0)
                                connection_set_state(connects, fd,CON_STATE_RESPONSE_END);
                                break;
			case  CON_STATE_RESPONSE_END:
				connection_set_state(connects, fd,CON_STATE_CLOSE);
				break;
			case CON_STATE_CLOSE:
				if(connects->m_fds[fd].data) {
                        		free(connects->m_fds[fd].data);
                        		connects->m_fds[fd].data=NULL;
                		}
				worker_cycle.worker_free_connects++;
                		gs_sys_epoll_del(connects,fd);
                		close(fd);
				joblist[fd]=-1;


				break;
			default:

				break;		
		}//switch

		if(state==connects->m_fds[fd].state){ //如果在以上处理过程中state没有变化，所以需要等待IO EVENT到来，退出
			done=1;
		}
	}//while
			
}
void print_con_state(struct gs_sys_epoll *connects,int fd)
{
	int state;

	state=connects->m_fds[fd].state;	

	switch(state){
		case CON_STATE_CONNECT:
			printf("Currenty this fd[%d] state is CON_STATE_CONNECT\n",fd);
			break;
		case CON_STATE_REQUEST_START:
			printf("Currenty this fd[%d] state is ON_STATE_REQUEST_START\n",fd);
                        break;
		case CON_STATE_READ:
			printf("Currenty this fd[%d] state is CON_STATE_READ\n",fd);
                        break;
		case  CON_STATE_REQUEST_END:
			printf("Currenty this fd[%d] state is CON_STATE_REQUEST_END\n",fd);
                        break;
		case CON_STATE_HANDLE_REQUEST:
			printf("Currenty this fd[%d] state is CON_STATE_HANDLE_REQUEST\n",fd);
                        break;
		case CON_STATE_RESPONSE_START:
			printf("Currenty this fd[%d] state is CON_STATE_RESPONSE_START\n",fd);
                        break;
		case  CON_STATE_WRITE:
			printf("Currenty this fd[%d] state is CON_STATE_WRITE\n",fd);
                        break;
		case CON_STATE_RESPONSE_END:
			printf("Currenty this fd[%d] state is CON_STATE_RESPONSE_END\n",fd);
                        break;
		case  CON_STATE_ERROR:
			printf("Currenty this fd[%d] state is CON_STATE_ERROR\n",fd);
                        break;
		case CON_STATE_CLOSE:
			printf("Currenty this fd[%d] state is CON_STATE_CLOSE\n",fd);
                        break;
		default:
			printf("Currenty this fd[%d] state is ???????\n",fd);
                        break;
	}

	return;


}
