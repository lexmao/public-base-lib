#ifndef QUEUE_H_
#define QUEUE_H_
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "list.h"


typedef struct thcontrol{
        pthread_mutex_t mutex;
        pthread_cond_t cond;
}thcontrol;

typedef struct taskList{
	void *task; //this point real task node
	list_head head;
}taskList;

typedef struct taskPool{
	unsigned int t_maxNum;//allow max task number
	unsigned int t_curNum;//current task number
	taskList *pool;
	void (*func)(void*);
	thcontrol control;
}taskPool;

/***follow is threadNode******/

typedef struct threadNode{
	pthread_t tid;
	int busy;
	void *task;
	thcontrol control;//be use awaken this thread and do some task
}threadNode;


typedef struct threadList{
	threadNode *thread;
	list_head head;
}threadList;

typedef struct threadPool{
	unsigned int t_maxNum;//pool allow max thread num
	unsigned int t_maxIdleNum;//allow max idle thread number
	unsigned int t_minIdleNum;//allow min idle thread number
	unsigned int t_idleNum;//current idle thread number
	unsigned int t_workNum;//current working thread number

	void (*fun)(void *);
	threadList *pool;
	thcontrol control;
}threadPool;

extern taskPool *taskpool;
extern threadPool *threadpool;



extern threadPool *ThreadPoolInit();
extern void setThreadPool(int maxNum,int maxIdleNum,int minIdleNum);
extern int createThreadPool();
extern  int increaseThreadPool();
extern int subThreadPool();
extern void addTaskToThreadPools(void *task);


extern taskPool *taskPoolInit();
extern void setTaskPool(int maxTask);
extern void *taskPoolAdd(void *node);
extern void *taskPoolDel();//del from list head...(queue)

#endif 
