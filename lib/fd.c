#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


#include "fd.h"



ssize_t writen ( int fd, void *vptr, size_t n )
{
    ssize_t         nwrite;
    size_t          nleft;
    unsigned char  *ptr = vptr;
    
    nleft   = n;
    
    while ( nleft > 0 ){
        
        if ( ( nwrite = write( fd, ptr, nleft ) ) <= 0 )
        {
            if ( EINTR == errno || EAGAIN == errno )
            {
                /*
                 * call write() again
                 */
                nwrite = 0;
            } else {
                /*
                 * error
                 */
                return( -1 );
            }
        }
        
        nleft  -= nwrite;
        ptr    += nwrite;
    }  /* end of while */
    
    return( n );
    
}  /* end of writen */





int fd_copy(int to,int from)

{
    if (to == from) return 0;
    if (fcntl(from,F_GETFL,0) == -1) return -1;
    close(to);
    if (fcntl(from,F_DUPFD,to) == -1) return -1;
    
    return 0;
}

int fd_move(int to, int from)

{
    if (to == from) return 0;
    if (fd_copy(to,from) == -1) return -1;
    close(from);

    return 0;
}


static int timeoutread(int t,int fd,char *buf,int len)
{
    fd_set rfds;
    struct timeval tv;

    tv.tv_sec = t;
    tv.tv_usec = 0;

    FD_ZERO(&rfds);
    FD_SET(fd,&rfds);

    if (select(fd + 1,&rfds,(fd_set *) 0,(fd_set *) 0,&tv) == -1) return -1;
    if (FD_ISSET(fd,&rfds)) return read(fd,buf,len);

    errno = ETIMEDOUT;
    return -1;
}

static int timeoutwrite(int t,int fd,char *buf,int len)
{
    fd_set wfds;
    struct timeval tv;

    tv.tv_sec = t;
    tv.tv_usec = 0;

    FD_ZERO(&wfds);
    FD_SET(fd,&wfds);

    if (select(fd + 1,(fd_set *) 0,&wfds,(fd_set *) 0,&tv) == -1) return -1;
    if (FD_ISSET(fd,&wfds)) return write(fd,buf,len);

    errno = ETIMEDOUT;
    return -1;
}


int saferead(int fd,int buf,int len)
{

    int r;
    r = timeoutread(1200,fd,buf,len);
    return r;
}

int safewrite(int fd,int buf,int len)
{
    int r;
    r = timeoutwrite(1200,fd,buf,len);
    return r;
}


