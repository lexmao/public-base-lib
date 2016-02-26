
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>     /* must come before sys/epoll.h to deal with buglet in glibc-2.3.2 */
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>


#include "epolllib.h"


#define MAX_EVENTS 1024
struct epoll_event events[MAX_EVENTS];

int gs_sys_epoll_init(struct gs_sys_epoll *this, int maxfds)
{
        int i;

        /* unfortunate kludge in epoll.  May be removed eventually. */
        this->maxfds = maxfds;

        this->m_fds_used = 0;
        this->m_fds_alloc = 16;
        this->m_fds = (gs_client_t *)malloc(sizeof(gs_client_t) * this->m_fds_alloc);
        if (!this->m_fds)
                return ENOMEM;

        for (i=this->m_fds_used; i<this->m_fds_alloc; i++) {
                this->m_fds[i].pfn = NULL;
                this->m_fds[i].data = NULL;
        }

        this->epfd = epoll_create(maxfds);

        if (this->epfd == -1)
                return errno;

        return 0;
}

void gs_sys_epoll_shutdown(struct gs_sys_epoll *this)
{
        int i;

        for (i=this->m_fds_alloc-1; (i >= 0) && (this->m_fds_used > 0); i--)
                if (this->m_fds[i].pfn)
                        gs_sys_epoll_del(this, i);

        if (this->m_fds) {
                free(this->m_fds);
                this->m_fds = NULL;
        }

        close(this->epfd);
        this->epfd = -1;
}


int gs_sys_epoll_prepare_fd_for_add(int fd, int pid)
{

        /* FIXME: This is correct right now on Linux. May not be so later on */
        int flags = O_RDWR | O_NONBLOCK;

        (void) pid;
        if (fcntl(fd, F_SETFL, flags) < 0) {
                int err = errno;
                return err;
        }
        return 0;
}

int gs_sys_epoll_add(struct gs_sys_epoll *this,int fd, gs_callback_fn_t pfn, void *data)
{
        int i, n;
        int err;
        struct epoll_event efd;

        if (fd >= this->m_fds_alloc) {
                gs_client_t *newfds;
                n = this->m_fds_alloc * 2;
                if (n < fd + 1)
                        n = fd + 1;

                newfds = (gs_client_t *)realloc(this->m_fds, n * sizeof(gs_client_t));
                if (!newfds)
                        return ENOMEM;

                for (i=this->m_fds_alloc; i<n; i++) {
                        newfds[i].pfn = NULL;
                        newfds[i].data = NULL;
                }

                this->m_fds = newfds;
                this->m_fds_alloc = n;
        }

        /* Looks like we can always specify EPOLLET even if using older epoll driver */
        efd.data.fd = fd;
        efd.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLET;
        if (epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &efd) == -1 ) {
                err = errno;
                return err;
        }

        this->m_fds_used++;

        /* Start out claiming "all ready", and let user program try
         * reading/writing/accept/connecting.  An EWOULDBLOCK should be harmless;
         * the user must then call clearReadiness() to tell us we were wrong.
         * This should handle the case where the fd is already ready for something
         * when we start.  (FIXME: Could call poll() here, instead.  Should we?)
         */

        this->m_fds[fd].pfn = pfn;
        this->m_fds[fd].data = data;


        return 0;
}

int gs_sys_epoll_del(struct gs_sys_epoll *this,int fd)
{
        int err;
        struct epoll_event efd;


        /* Sanity checks */
        if ((fd < 0) || (fd >= this->m_fds_alloc) || (this->m_fds_used == 0)) {
                return EINVAL;
        }

        efd.data.fd = fd;
        efd.events = -1;
        if (epoll_ctl(this->epfd, EPOLL_CTL_DEL, fd, &efd) == -1 ) {
                err = errno;
                return err;
        }

        this->m_fds[fd].pfn = NULL;
        this->m_fds[fd].data = NULL;

        this->m_fds_used--;

        return 0;
}


/**
 Sleep at most timeout_millisec waiting for an I/O readiness event
 on the file descriptors we're watching.
 Call each descriptor's pfn if it's ready.
 @return 0 on success, EWOULDBLOCK if no events ready
 */
int gs_sys_epoll_waitAndDispatchEvents(struct gs_sys_epoll *this, int timeout_millisec)
{
        struct epoll_event *efds = events;
        gs_pollevent_t event;
        int nfds;


	memset(&event,0,sizeof(gs_pollevent_t));

	do{
        	nfds = epoll_wait(this->epfd, efds, MAX_EVENTS /*this->m_fds_used*/, timeout_millisec);

        } while (nfds< 0 && errno == EINTR);


        assert(nfds <= this->m_fds_used);
        for (; nfds > 0; nfds--, efds++) {
                int fd = efds->data.fd;
                if ((fd < 0) || (fd >= this->m_fds_alloc) || (this->m_fds_used == 0) || !this->m_fds[fd].pfn) {
                        /* silently ignore.  Might be stale (aren't time skews fun?) */
                        continue;
                }
                event.fd = fd;
                event.revents = efds->events & (EPOLLIN|EPOLLPRI|EPOLLOUT|EPOLLERR|EPOLLHUP);
                event.client = this->m_fds[fd];
                event.client.pfn(&event);
        }
        return 0;
}



