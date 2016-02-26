#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "lock.h"


int gs_process_lock_init(gs_mutex_t *g_mutex)
{
	pthread_mutexattr_t attr;

	int ret= -1;

	if(g_mutex== NULL) return ret;

	pthread_mutexattr_init(&attr);
	ret=pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);
	if( ret!=0 )
		return ret;

	return pthread_mutex_init(&(g_mutex->mutex), &attr);

}


int gs_process_lock(gs_mutex_t *g_mutex)
{
	int ret =-1;

	if(g_mutex==NULL) return ret;	

	return pthread_mutex_lock(&(g_mutex->mutex));
}

int gs_process_trylock(gs_mutex_t *g_mutex)
{
        int ret =-1;

        if(g_mutex==NULL) return ret;

        return pthread_mutex_trylock(&(g_mutex->mutex));
}


int gs_process_unlock(gs_mutex_t *g_mutex)
{
	int ret=-1;

	if(g_mutex==NULL) return ret;

	return pthread_mutex_unlock(&(g_mutex->mutex));
}
