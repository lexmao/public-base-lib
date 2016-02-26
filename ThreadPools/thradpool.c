/*
 * (C) 2005 
 * -----------------------------------------------------------------------
 * Author : Stone Li <liqinglin@gmail.com>
 * 
 * -----------------------------------------------------------------------
*/



#include <pthread.h>

#include "queue.h"
#include "list.h"
#include "memwatch.h"

threadPool *threadpool=NULL;

void threadExitUnlock(void *mutex)
{
	pthread_mutex_unlock((pthread_mutex_t *)mutex);
}


static void threadDo(void *node)
{
	int active=1;
	threadNode *threadnode=NULL;
	void (*exitfun)(void*)=threadExitUnlock;

	threadnode=(threadNode*)node;

	while(active){

		pthread_testcancel();//Cancelation-point

		pthread_cond_wait(&(threadnode->control.cond),&(threadnode->control.mutex));

		pthread_cleanup_push(exitfun, (void *) &(threadnode->control.mutex));

		pthread_testcancel();//Cancelation-point

		taskpool->func((void*)threadnode->task);	
		threadnode->busy=0;
		free(threadnode->task);
		threadnode->task=(void*)NULL;
		pthread_mutex_unlock(&(threadnode->control.mutex));
		
		pthread_cleanup_pop(0);

		pthread_testcancel();//Cancelation-point
	}
}

static int thcontrol_init(thcontrol *control)
{
        if(pthread_mutex_init(&(control->mutex),NULL)) return -1;
        if(pthread_cond_init(&(control->cond),NULL)){
                pthread_mutex_destroy(&(control->mutex));
                return -1;
        }
        return 0;
}

static void thcontrol_destroy(thcontrol *control)
{
        pthread_mutex_destroy(&(control->mutex));
        pthread_cond_destroy(&(control->cond));
        return;
}

threadPool *ThreadPoolInit()
{
	threadpool=(threadPool *)malloc(sizeof(threadPool));
        if(threadpool==NULL) return (threadPool*)NULL;
        threadpool->pool=(threadList*)malloc(sizeof(threadList));

        if(list_init(&(threadpool->pool->head))==NULL) {
                free(threadpool->pool);
                free(threadpool);
                return (threadPool*)NULL;
        }
        thcontrol_init(&(threadpool->control));
	threadpool->fun=threadDo;
	threadpool->t_workNum=0;

        return threadpool;
}

void setThreadPool(int maxNum,int minNum,int idleNum)
{
	if(threadpool==NULL) return;	
	if(maxNum<1||minNum>maxNum) return;
	if(minNum>maxNum) return;
	if(idleNum<minNum||idleNum>maxNum) return;
	
	pthread_mutex_lock(&(threadpool->control.mutex));
	threadpool->t_minIdleNum=minNum;
	threadpool->t_maxIdleNum=maxNum;	
	threadpool->t_idleNum=idleNum;
	threadpool->t_workNum=0;
	pthread_mutex_unlock(&(threadpool->control.mutex));

	return;
}

int createThreadPool()
{
	int j;


	if(threadpool==NULL) return 0;
	if(threadpool->t_idleNum<=0) return 0;

	for(j=0;j<threadpool->t_maxIdleNum;j++){
		threadList *listnode=NULL;

		listnode=(threadList*)malloc(sizeof(threadList));	
		listnode->thread=(threadNode*)malloc(sizeof(threadNode));	
		listnode->thread->task=NULL;


		/*cond and mutex used be pthread_cond_wait and to work*/
		if(pthread_mutex_init(&(listnode->thread->control.mutex),NULL)) return -1;
        	if(pthread_cond_init(&(listnode->thread->control.cond),NULL)) return -1;

		listnode->thread->busy=0;//idle flag
		listnode->thread->task=(void*)NULL;

		/*get task from taskpool and send to thread->task*/
		pthread_create(&(listnode->thread->tid),NULL,(void*)*(threadpool->fun),(void*)(listnode->thread));

		/* format so can not lock list ...*/	
		list_add(&listnode->head,&threadpool->pool->head);//queue

	}
	return 1;			
}

int increaseThreadPool()
{
	int j,num;



	num=threadpool->t_minIdleNum - threadpool->t_idleNum;
	if(num<=0) return 0;  //oh, idle thread number > allow min thread number, so don't increase	

	for(j=0;j<num;j++){

		threadList *listnode=NULL;

                listnode=(threadList*)malloc(sizeof(threadList));
                listnode->thread=(threadNode*)malloc(sizeof(threadNode));
                listnode->thread->task=NULL;


                /*cond and mutex used be pthread_cond_wait and to work*/
                if(pthread_mutex_init(&(listnode->thread->control.mutex),NULL)) return -1;
                if(pthread_cond_init(&(listnode->thread->control.cond),NULL)) return -1;

                listnode->thread->busy=0;//idle flag

                /*get task from taskpool and send to thread->task*/
                pthread_create(&(listnode->thread->tid),NULL,(void*)*(threadpool->fun),(void*)(listnode->thread->task));

		pthread_mutex_lock(&(threadpool->control.mutex));
                list_add(&listnode->head,&threadpool->pool->head);//queue
		pthread_mutex_lock(&(threadpool->control.mutex));
	}
		
	return j+1;
}

void *idlePoolDel()//del from list head...(queue) return tasknode
{
        void *task=NULL;
        threadList *listNode;
        size_t offset;
        list_head *ps;

        pthread_mutex_lock(&(threadpool->control.mutex));
        if(threadpool->t_idleNum<=0||list_empty(&threadpool->pool->head)) {//pool is empty
                pthread_mutex_unlock(&(threadpool->control.mutex));
                return (void*)NULL;
        }

        ps=threadpool->pool->head.next;
        offset=((size_t) &((threadList*)0)->head);
        listNode=(threadList*)((size_t)(char*)ps-offset);
        task=listNode->thread->task;

        list_del(ps,&(threadpool->pool->head));
        threadpool->t_idleNum--;
        pthread_mutex_unlock(&(threadpool->control.mutex));
        free(listNode);

        return task;
}

int subThreadPool()
{
	int num,j;
	threadNode *task=NULL;

	num=threadpool->t_idleNum - (threadpool->t_maxIdleNum-thread->t_minIdleNum);
	if(num <= 0) return 0; //oh , < allow max number,so don't sub

	for(j=0;j<num;j++){
		task=NULL;
		task=(threadNode*)idlePoolDel();
		if(task==NULL) continue;
		pthread_cancel(task->tid);//kill this thread
		pthread_join(task->tid, NULL);
		task->tid=-1;
		free(task);	
	}	
}

void addTaskToPool(void *task)
{
	void *task=NULL;
        threadList *listNode;
        size_t offset;
        list_head *ps;

	while(!list_empty(&threadpool->pool->head)){
		pthread_mutex_lock(&(threadpool->control.mutex));

		ps=threadpool->pool->head.next;
		offset=((size_t) &((threadList*)0)->head);
        	listNode=(threadList*)((size_t)(char*)ps-offset);
		if(listNode->thread->task==NULL&&listNode->thread->busy==0){
        		listNode->thread->task=(void*)task;
			listNode->thread->busy=1;
			threadpool->t_workNum++;
			threadpool->t_idleNum--;
			pthread_cond_broadcast(&listNode->thread->control.cond);
		}
		pthread_mutex_unlock(&(threadpool->control.mutex));		
		increaseThreadPool();	
		subThreadPool();
	}					
}
