#include "mpool.h"
#include "queue.h"

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

void manager_InitThreadPool(int maxNum,int maxIdleNum,int minIdleNum)
{
	if(ThreadPoolInit()==NULL) {
		printf("Can't format threadpool...\n");
		exit(120);
	}
	setThreadPool(maxNum,maxIdleNum,minIdleNum);
	createThreadPool();
}

void manager_InitTaskPool(int maxTask)
{
	if(taskPoolInit()==NULL){
		printf("Can't format taskpool...\n");
                exit(120);
	}
	setTaskPool(maxTask);
	return;
	
}

void manager_addTaskToTaskpool(void *task)
{

	pthread_mutex_lock(&(taskpool->control.mutex));
	while(taskpool->t_curNum>=taskpool->t_maxNum){
		pthread_cond_wait(&(taskpool->control.cond),&(taskpool->control.mutex));
	}
	pthread_mutex_unlock(&(taskpool->control.mutex));

	taskPoolAdd(task);

}

void manager_moveTaskToWorker()
{
	void *task=NULL;
	int idlenum;	

	if(list_empty(&taskpool->pool->head)) return;

	pthread_mutex_lock(&(threadpool->control.mutex));
	while(threadpool->t_idleNum<=0){
		pthread_cond_wait(&(threadpool->control.cond),&(threadpool->control.mutex));
	}
	pthread_mutex_unlock(&(threadpool->control.mutex));

	task=(void*)taskPoolDel();
	pthread_cond_signal(&taskpool->control.cond);
	if(task==NULL) return;
	addTaskToThreadPools(task);

}

static void managerDoMoveTaskToWorker()
{
	int oldtype;

	while(1) {

		pthread_testcancel();	

		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);		
		manager_moveTaskToWorker();	
		pthread_setcanceltype(oldtype, NULL);

		pthread_testcancel();
	}
}

static void managerThreadNum()
{
	int oldtype;
	int flag=0;

	while(1){
		sleep(30);
		if(flag==3) flag=0;
		pthread_testcancel();
		pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
		pthread_mutex_lock(&(threadpool->control.mutex));
		if((threadpool->t_idleNum - threadpool->t_maxIdleNum)>0) flag++;
		if(flag==3)
			subThreadPool();
		else flag--;
		pthread_mutex_unlock(&(threadpool->control.mutex));
		pthread_setcanceltype(oldtype, NULL);
                pthread_testcancel();
	}
}

void managerInit(int taskMax,int thMax,int thMaxIdle,int thMinIdle,void *(*func)(void *))
{
	manager_InitThreadPool(thMax,thMaxIdle,thMinIdle);
	manager_InitTaskPool(taskMax);
	taskpool->func=(void*)func;

}

pthread_t managerRun()
{
	pthread_t tid;

	pthread_create(&tid,NULL,(void*)managerDoMoveTaskToWorker,(void*)NULL);

	return tid;
}

pthread_t managerPool()
{
	pthread_t tid;

        pthread_create(&tid,NULL,(void*)managerThreadNum,(void*)NULL);

        return tid;
}


void managerDestroy(pthread_t run_mtid,pthread_t pool_mtid,pthread_t main_mtid)
{
	int worknum=0;

	pthread_join(main_mtid,(void**)NULL);
	do{
		pthread_mutex_lock(&(threadpool->control.mutex));
        	worknum=threadpool->t_workNum;
        	pthread_mutex_unlock(&(threadpool->control.mutex));
        	if(worknum>0) sleep(1);

	}while(worknum>0);
	
	pthread_cancel(run_mtid);	
	pthread_join(run_mtid,(void**)NULL);
	
	pthread_cancel(pool_mtid);
        pthread_join(pool_mtid,(void**)NULL);

	thcontrol_destroy(&threadpool->control);
	thcontrol_destroy(&taskpool->control);
	if(taskpool!=NULL) free(taskpool);
	if(threadpool!=NULL) free(threadpool);
}

pthread_t managerInterface(void (*mInterface)(void*))
{
	pthread_t tid;

	pthread_create(&tid,NULL,(void*)mInterface,(void*)NULL);

	return tid;
}


