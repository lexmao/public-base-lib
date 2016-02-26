#ifndef LOCK_H__
#define LOCK_H__

#include  <pthread.h>


struct gs_mutex_s{
        pthread_mutex_t mutex;
        pthread_cond_t cond;
};
typedef struct gs_mutex_s gs_mutex_t;

extern int gs_process_lock_init(gs_mutex_t *g_mutex);
extern int gs_process_lock(gs_mutex_t *g_mutex);
extern int gs_process_trylock(gs_mutex_t *g_mutex);
extern int gs_process_unlock(gs_mutex_t *g_mutex);


#endif
