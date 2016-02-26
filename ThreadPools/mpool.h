#ifndef MPOOL_H__
#define MPOOL_H__

#include <pthread.h>
#include "queue.h"
#include "list.h"
#include "memwatch.h"


extern void manager_InitThreadPool(int maxNum,int maxIdleNum,int minIdleNum);
extern void manager_InitTaskPool(int maxTask);
extern void manager_addTaskToTaskpool(void *task);
extern void manager_moveTaskToWorker();
extern pthread_t managerRun();
extern void managerInit(int taskMax,int thMax,int thMaxIdle,int thMinIdle,void *(*func)(void *));
extern void managerDestroy(pthread_t run_mtid,pthread_t pool_mtid,pthread_t main_mtid);
extern pthread_t managerPool();
extern pthread_t managerInterface(void (*mInterface)(void*));
#endif
