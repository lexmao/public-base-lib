#ifndef WORKER_H__
#define WORKER_H__
#include "epolllib.h"

struct worker_cycle_s{
	unsigned int worker_max_connects;  //this worker can process max connects
	unsigned int worker_free_connects;  //worker current free connects

	/*accept һ�������Ӻ� ����worker_accept_disabled= worker_max_connects/8 - worker_free_connects
         *if worker_accept_disabled >0 then means too many connects process ..so will delay accept ..
         */ 
	int worker_accept_disabled ;//
	int worker_accept_available; //����ÿ��acceptʱ�������
	int worker_accept_mutex_delay; //���� epoll wait��ʱ��
	

		

};

#define MAX_BUFFER_LEN    1024

typedef struct worker_cycle_s worker_cycle_t;


extern worker_cycle_t worker_cycle;
extern int gs_worker_process_cycle();
extern int writer_fd_sycle(int fd);
extern int reader_fd_sycle(int fd);
extern int *joblist;

extern int connect_io_callback(gs_pollevent_t *event);

#endif
