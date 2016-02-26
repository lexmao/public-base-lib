#include "queue.h"
#include "list.h"
#include "memwatch.h"

threadPool *workerpool=NULL;

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


threadPool *workerPoolInit()
{
        workerpool=(threadPool *)malloc(sizeof(threadPool));
        if(workerpool==NULL) return (threadPool*)NULL;
        workerpool->pool=(threadList*)malloc(sizeof(threadList));

        if(list_init(&(workerpool->pool->head))==NULL) {
                free(workerpool->pool);
                free(workerpool);
                return (threadPool*)NULL;
        }
        thcontrol_init(&(workerpool->control));

        return workerpool;
}
