#ifndef EPOLLLIB_H__
#define EPOLLLIB_H__ 

#include <assert.h>
#include <poll.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>



struct gs_pollevent_s;

typedef int (*gs_callback_fn_t)(struct gs_pollevent_s *event);


/* the order of the items should be the same as they are processed
 * read before write as we use this later */
typedef enum {
    CON_STATE_CONNECT,            //连接 
    CON_STATE_REQUEST_START,    //开始获取请求 
    CON_STATE_READ,                //处理读 
    CON_STATE_REQUEST_END,        // 请求结束
    CON_STATE_HANDLE_REQUEST,    // 处理请求
    CON_STATE_RESPONSE_START,    // 开始回复 
    CON_STATE_WRITE,            // 处理写 
    CON_STATE_RESPONSE_END,        //回复结束 
    CON_STATE_ERROR,            // 出错 
    CON_STATE_CLOSE                // 连接关闭 
} connection_state_t;




struct gs_client_s {
        /* The callback fn */
        gs_callback_fn_t pfn;

        /* The data to be passed back */
        void *data;
	unsigned int size;
	

	 /** 状态机 ****/
        connection_state_t state;

	/**flag read or write*/
	int is_readable;
	int is_writable;

};

typedef struct gs_client_s gs_client_t;


struct gs_pollevent_s {
        /* the file descriptor that is ready for I/O (same as in struct pollfd). */
        int fd;

        /* the kind of I/O this fd is currently ready for (same as in struct pollfd). */
        short revents;

        /* the object to call to handle I/O on this fd. */
        gs_client_t client;
};

typedef struct gs_pollevent_s gs_pollevent_t;


struct gs_sys_epoll {
        /** max number of fds epoll was told to happen at open time */
        int maxfds;

        /** The fd returned by epoll_create */
        int epfd;


        /** Each fd we watch has an entry in this array. */
        gs_client_t *m_fds;

        /** Number of elements worth of heap allocated for m_fds. */
        int m_fds_alloc;

        /** How many fds we are watching. */
        int m_fds_used;
};


/** Initialize the gs_t. */
int gs_sys_epoll_init(struct gs_sys_epoll *, int maxfds);

/** Release any resouces allocated internally by this gs_t. */
void gs_sys_epoll_shutdown(struct gs_sys_epoll *);

/**
 Sets the flags of fd for use with epoll
 On exit, the fd is ready for use with gs_sys_epoll_add
 Note: Overrides the old flags of fd.
 @param fd - the file descriptor to be used for IO
 @param pid - the pid of the process. Ignored for epoll
*/
int gs_sys_epoll_prepare_fd_for_add(int fd, int pid);

/**
 Add a file descriptor to the set we monitor.
 Caller should already have established a handler for SIGIO.
 @param fd file descriptor to add
 @param pfn function to call when fd is ready
 @param data context pointer to pass to pfn
 */
int gs_sys_epoll_add(struct gs_sys_epoll *, int fd, gs_callback_fn_t pfn, void *data);

/** Remove a file descriptor. */
int gs_sys_epoll_del(struct gs_sys_epoll *, int fd);

/**
 Sleep at most timeout_millisec waiting for an I/O readiness event
 on the file descriptors we're watching, and dispatch events to
 the handler for each file descriptor that is ready for I/O.
 This is included as an example of how to use
 waitForEvents and getNextEvent.  Real programs should probably
 avoid waitAndDispatchEvents and call waitForEvents and getNextEvent
 instead for maximum control over client deletion.
 */
int gs_sys_epoll_waitAndDispatchEvents(struct gs_sys_epoll *, int timeout_millisec);


#endif
