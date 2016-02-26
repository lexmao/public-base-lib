#include "queue.h"
#include "list.h"
#include "memwatch.h"

taskPool *taskpool=NULL;

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

taskPool *taskPoolInit()
{
	taskpool=(taskPool *)malloc(sizeof(taskPool));
	if(taskpool==NULL) return (taskPool*)NULL;
	taskpool->pool=(taskList*)malloc(sizeof(taskList));

	if(list_init(&(taskpool->pool->head))==NULL) {
		free(taskpool->pool);
		free(taskpool);
		return (taskPool*)NULL;
	}
	taskpool->t_curNum=0;
	thcontrol_init(&(taskpool->control));

	return taskpool;
}

void setTaskPool(int maxTask)
{

	if(taskpool==NULL||maxTask<1) return;
	pthread_mutex_lock(&(taskpool->control.mutex));
	taskpool->t_maxNum=maxTask;		
	pthread_mutex_unlock(&(taskpool->control.mutex));
}

void *taskPoolAdd(void *tasknode)
{
	taskList *tasklist=NULL;

	if(tasknode==NULL) return (void*)NULL;
	pthread_mutex_lock(&(taskpool->control.mutex));

	if(taskpool->t_curNum>=taskpool->t_maxNum){ //oh,to many task more than allow max task
		/* here maybe wait .....*/
		pthread_mutex_unlock(&(taskpool->control.mutex));
		return NULL;
	}
	tasklist=(taskList*)malloc(sizeof(taskList));		
	tasklist->task=tasknode;
	list_add(&tasklist->head,&taskpool->pool->head);//queue
	taskpool->t_curNum++;

	pthread_mutex_unlock(&(taskpool->control.mutex));
	return tasknode;
}

void *taskPoolDel()//del from list head...(queue) return tasknode
{
	void *task=NULL;	
	taskList *listNode;
	size_t offset;
	list_head *ps;
	
	pthread_mutex_lock(&(taskpool->control.mutex));			
	if(taskpool->t_curNum<=0||list_empty(&taskpool->pool->head)) {//pool is empty
		pthread_mutex_unlock(&(taskpool->control.mutex));
		return (void*)NULL;
	}	

	ps=taskpool->pool->head.next;
	if(ps==NULL){
		pthread_mutex_unlock(&(taskpool->control.mutex));
                return (void*)NULL;
	}
	offset=((size_t) &((taskList*)0)->head);
	listNode=(taskList*)((size_t)(char*)ps-offset);
	task=listNode->task;

	list_del(ps,&(taskpool->pool->head));

	taskpool->t_curNum--;
	pthread_mutex_unlock(&(taskpool->control.mutex));

	free(listNode);

	return task;	
}
