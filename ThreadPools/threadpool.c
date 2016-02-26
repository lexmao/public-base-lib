#include <pthread.h>

#include "queue.h"
#include "list.h"
#include "memwatch.h"

threadPool *threadpool=NULL;
static list_head *Pos=NULL;


static void threadExitUnlock(void *tmp_control)
{
	thcontrol *control=(thcontrol*)tmp_control;

	pthread_mutex_unlock((pthread_mutex_t *)&control->mutex);
	pthread_mutex_destroy((pthread_mutex_t *)&control->mutex);
	pthread_cond_destroy((pthread_cond_t*)&control->cond);
printf("Debug exit..............................................................\n");

}

static void threadDo(void *node)
{
	int active=1;
	thcontrol tmp_control;
	int oldtype;

	threadNode *threadnode=NULL;

	if(node==NULL) return;
	threadnode=(threadNode*)node;
	
	tmp_control.mutex=threadnode->control.mutex;
	tmp_control.cond=threadnode->control.cond;
	while(active){

		pthread_testcancel();//Cancelation-point

		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
		pthread_cleanup_push(threadExitUnlock, (void *) &tmp_control);

		pthread_mutex_lock(&(threadnode->control.mutex));

		while(threadnode->busy==0&&threadnode->task==(void*)NULL){ //hi,this add this line code cast two days!!!!
			pthread_cond_wait(&(threadnode->control.cond),&(threadnode->control.mutex));
		}

		taskpool->func((void*)threadnode->task);	
		threadnode->task=(void*)NULL;
                threadnode->busy=0;
		pthread_mutex_unlock(&(threadnode->control.mutex));


		pthread_mutex_lock(&(threadpool->control.mutex));
		threadpool->t_idleNum++;
		threadpool->t_workNum--;
		pthread_mutex_unlock(&(threadpool->control.mutex));

		pthread_cleanup_pop(0);
		pthread_setcanceltype(oldtype, NULL);

		pthread_testcancel();//Cancelation-point
		pthread_cond_signal(&threadpool->control.cond);

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

void setThreadPool(int maxNum,int maxIdleNum,int minIdleNum)
{
	if(threadpool==NULL) return;	
	if(maxIdleNum<1||minIdleNum>maxIdleNum) return;
	if(maxNum<1||maxNum<maxIdleNum) return;
	if(minIdleNum>maxIdleNum) return;
	
	pthread_mutex_lock(&(threadpool->control.mutex));
	threadpool->t_minIdleNum=minIdleNum;
	threadpool->t_maxIdleNum=maxIdleNum;	
	threadpool->t_maxNum=maxNum;
	pthread_mutex_unlock(&(threadpool->control.mutex));

	return;
}

int createThreadPool()
{
	int j;


	if(threadpool==NULL) return 0;
	if(threadpool->t_minIdleNum<=0) return 0;

	for(j=0;j<threadpool->t_minIdleNum;j++){
		threadList *listnode=NULL;

		listnode=(threadList*)malloc(sizeof(threadList));	
		listnode->thread=(threadNode*)malloc(sizeof(threadNode));	

		/*cond and mutex used be pthread_cond_wait and to work*/
		if(pthread_mutex_init(&(listnode->thread->control.mutex),NULL)) return -1;
        	if(pthread_cond_init(&(listnode->thread->control.cond),NULL)) return -1;

		listnode->thread->busy=0;//idle flag
		listnode->thread->task=(void*)NULL;
		listnode->thread->tid=-1;

		/*get task from taskpool and send to thread->task*/
		pthread_create(&(listnode->thread->tid),NULL,(void*)*(threadpool->fun),(void*)(listnode->thread));

		/* format so can not lock list ...*/	
		list_add(&listnode->head,&threadpool->pool->head);//queue
	}
	threadpool->t_idleNum=j;
	return 1;			
}

int increaseThreadPool()
{
	int j,num;


	if((threadpool->t_workNum+threadpool->t_idleNum)>=threadpool->t_maxNum) return 0;

	num=threadpool->t_minIdleNum - threadpool->t_idleNum;

	j=num + threadpool->t_workNum + threadpool->t_idleNum - threadpool->t_maxNum;
	if(j>0) num-=j;

	if(num<=0) return 0;  //oh, idle thread number > allow min thread number, so don't increase	

	for(j=0;j<num;j++){

		threadList *listnode=NULL;

                listnode=(threadList*)malloc(sizeof(threadList));
                listnode->thread=(threadNode*)malloc(sizeof(threadNode));


                /*cond and mutex used be pthread_cond_wait and to work*/
                if(pthread_mutex_init(&(listnode->thread->control.mutex),NULL)) return -1;
                if(pthread_cond_init(&(listnode->thread->control.cond),NULL)) return -1;

                listnode->thread->busy=0;//idle flag
		listnode->thread->task=(void*)NULL;

                /*get task from taskpool and send to thread->task*/
		 pthread_create(&(listnode->thread->tid),NULL,(void*)*(threadpool->fun),(void*)(listnode->thread));

                list_add(&listnode->head,&threadpool->pool->head);//queue
		threadpool->t_idleNum++;
		Pos=threadpool->pool->head.next;

		if( threadpool->t_workNum>= threadpool->t_maxIdleNum) break;
	}
		
	return j;
}

static void *idlePoolDel()//del from list head...(queue) return tasknode
{
        threadNode *task=NULL;
        threadList *listNode;
        size_t offset;
        list_head *ps;

        if(threadpool->t_idleNum<=0||list_empty(&threadpool->pool->head)) {//pool is empty
                return (threadNode*)NULL;
        }

        ps=threadpool->pool->head.next;
        offset=((size_t) &((threadList*)0)->head);
        listNode=(threadList*)((size_t)(char*)ps-offset);
        task=listNode->thread;
        list_del(ps,&(threadpool->pool->head));
        free(listNode);
	//if(ps==Pos) Pos=Pos->next;
	Pos=threadpool->pool->head.next;

        return task;
}

int subThreadPool()
{
	int num,j;
	threadNode *task=NULL;

	num=threadpool->t_idleNum - threadpool->t_maxIdleNum;
	if(num <= 0) return 0; //oh , < allow max number,so don't sub
	for(j=0;j<num;j++){
		task=NULL;
		task=(threadNode*)idlePoolDel();
		if(task==NULL) continue;
		pthread_cancel(task->tid);//kill this thread
		pthread_join(task->tid, NULL);
		task->tid=-1;
        	threadpool->t_idleNum--;
		free(task);	
	}	
}

void addTaskToThreadPools(void *task)
{

	if(task==NULL) return;

	if(threadpool==NULL||list_empty(&threadpool->pool->head)) {
		return;
	}

	if(Pos==NULL) Pos=threadpool->pool->head.next;

	while(Pos){

		threadList *threadlist;
		threadNode *thread;
                size_t offset;

                offset=((size_t) &((threadList*)0)->head);
                threadlist=(threadList*)((size_t)(char*)Pos-offset);
		thread=threadlist->thread;	
		if(thread->busy==0&&thread->task==(void*)NULL){ //ok find empty and add task to this node

			pthread_mutex_lock(&(thread->control.mutex));
			thread->busy=1;
			thread->task=(void*)task;
			pthread_mutex_unlock(&(thread->control.mutex));

			pthread_mutex_lock(&(threadpool->control.mutex));
			threadpool->t_workNum++;
			threadpool->t_idleNum--;
			increaseThreadPool();;
//	printf("maxNum=%d;workNum=%d;idleNum=%d\n",threadpool->t_maxNum,threadpool->t_workNum,threadpool->t_idleNum);
			pthread_mutex_unlock(&(threadpool->control.mutex));

			pthread_cond_signal(&thread->control.cond);//only broadcast one thread

			Pos=Pos->next;
			break;

		}
		Pos=Pos->next;
	}

	return;
}
