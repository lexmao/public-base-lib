#include "socklib.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <syslog.h>

#ifdef Linux
#include <time.h>
#endif

#include <net/if.h>
#include <netinet/in.h>

#ifdef FreeBSD
#include <net/if_var.h>
#include <netinet/in_var.h>
#endif

#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>


int initTcpServer ( unsigned short int localPort )
{
    struct sockaddr_in sin;
    int                i;
    int                acceptSocket;

    acceptSocket        = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
    i                   = 1;
    if ( -1 == setsockopt( acceptSocket, SOL_SOCKET, SO_REUSEADDR, &i, sizeof( i ) ) )
    {
        perror( "setsockopt( SO_REUSEADDR ) error" );
        exit( EXIT_FAILURE );
    }
    memset( &sin, 0, sizeof( sin ) );
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port        = localPort;
    bind( acceptSocket, ( struct sockaddr * )&sin, sizeof( sin ) );
    listen( acceptSocket, 512);
    return( acceptSocket );

}  /* end of initTcpServer */
