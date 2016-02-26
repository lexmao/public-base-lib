/*
 * (C) 2005 
 * -----------------------------------------------------------------------
 * Author : Stone Li <liqinglin@gmail.com>
 * 
 * -----------------------------------------------------------------------
*/




#include <net/if.h>
#include <netinet/in.h>

#ifdef FreeBSD
#include <net/if_var.h>
#include <netinet/in_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>

#include "queue.h"
#include "mpool.h"
#include "memwatch.h"
#include "socklib.h"

typedef  struct  taskNode{
        int sockfd;
}taskNode;

void printsd(void *task)
{
	char s[128];


	snprintf(s,128,"Hello stone! this pthread id=%ld\n",(unsigned long)pthread_self());
	taskNode *node=(taskNode*)task;
	write(node->sockfd,s,strlen(s));
	read(node->sockfd,s,sizeof(s));
	close(node->sockfd);
	free(task); //hi,this free must be do!!!
	return;
}

void mInterface()
{
	int acceptSocket;
	acceptSocket=initTcpServer (3333);
	if(acceptSocket<=0) {
		printf("socket create error\n");
		exit(0);
	}
	while(1){
                struct sockaddr_in clientAddr;
                size_t             clientAddrLen;
                int connectSocket;
		taskNode *task=NULL;
                clientAddrLen = sizeof( clientAddr );
                memset( &clientAddr, 0, clientAddrLen );
                connectSocket = accept( acceptSocket, ( struct sockaddr * )&clientAddr, &clientAddrLen );
                if(connectSocket<0) continue;

                task=(taskNode*)malloc(sizeof(taskNode));
                task->sockfd=connectSocket;
		manager_addTaskToTaskpool((void*)task);
        }
	return;
}


int main ( int argc, char * argv[] )
{
	int run_mtid,pool_mtid,main_mtid;
	
	managerInit(100,100,80,50,(void*)printsd);	
	main_mtid=managerInterface(mInterface);
	run_mtid=managerRun();
	pool_mtid=managerPool();
	managerDestroy(run_mtid,pool_mtid,main_mtid);

	printf("Main will exit(0)...\n");
        exit(0);

}
