/*
 * (C) 1995 Todd Vierling
 * -----------------------------------------------------------------------
 * Rewrite : scz
 * Version : 1.12 2005-02-24 12:55
 * Compile : For x86/Linux RedHat_8 2.4.18-14
 *         : gcc -DLinux -static -Wall -pipe -O3 -s -o datapipe datapipe.c
 *         :
 *         : For x86/FreeBSD 4.5-RELEASE
 *         : gcc -DFreeBSD -static -Wall -pipe -O3 -s -o datapipe datapipe.c
 *         :
 *         : For SPARC/Solaris 8
 *         : gcc -DSparc -Wall -pipe -O3 -s -o datapipe datapipe.c -lsocket -lnsl
 *         :
 *         : For AIX 4.3.3.0
 *         : gcc -DAix -Wall -pipe -O3 -s -o datapipe datapipe.c
 *         :
 *         : (strip datapipe or gcc -s)
 *         : mcs -d datapipe (only for Solaris)
 *         :
 *         : If you want to debug this code, compiling it with "-DDEBUG -g" without "-O3"
 *         :
 * Create  : 1995
 * Modify  : 2005-02-24 12:55
 * -----------------------------------------------------------------------
 * The only thing they can't take from us are our minds. !H
 */

/*
 * �����Ѿ�daemon�����ܶ�perror()ʵ���������壬����ֻ�Ǳ���������һЩ����
 * ȥ�ȽϹ���Ĵ��룬��Ϊ�Ҳ���ֻдһ�����������뷨��Ҫ�������Ƭ�Ρ�
 *
 * ��1.00�������������ͼ֧��FTP������������PORT���PASV��Ӧ��֧��FTP��
 * �������ڶ�FTP��֧��Ӱ��Ч�ʣ���Ǿ��Ա�Ҫ����Ҫ����������ָ��-f��
 *
 * 1.12������������һ�´�����
 */

/************************************************************************
 *                                                                      *
 *                               Head File                              *
 *                                                                      *
 ************************************************************************/

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

/************************************************************************
 *                                                                      *
 *                               Macro                                  *
 *                                                                      *
 ************************************************************************/

#define VERSION             "1.12 2005-02-24 12:55"

#if defined(Sparc) || defined(Solaris)
#define INADDR_NONE         0xFFFFFFFF
#endif

#ifdef Linux
#define POLLRDNORM          0x040
#endif

typedef void Sigfunc ( int );

/************************************************************************
 *                                                                      *
 *                            Function Prototype                        *
 *                                                                      *
 ************************************************************************/

static int                  Accept
(
    int                     s,
    struct sockaddr        *addr,
    size_t                 *addrlen
);
static void                 Atexit
(
    void                 ( *func ) ( void )
);
static void                 Bind
(
    int                     s,
    struct sockaddr        *addr,
    size_t                  addrlen
);
static void                 daemon_init
(
    char                   *workdir,
    mode_t                  mask
);
static void                 doFtpRelay
(
    int                     connectSocket,
    int                     remoteSocket
);
static void                 doTcpRelay
(
    int                     connectSocket,
    int                     remoteSocket
);
static int                  Dup2
(
    int                     oldfd,
    int                     newfd
);
static pid_t                Fork
(
    void
);
static void                 Getsockname
(
    int                     s,
    struct sockaddr        *name,
    size_t                 *namelen
);
static void                 init_signal
(
    void
);
static int                  initTcpServer
(
    unsigned short int      localPort
);
static void                 Listen
(
    int                     s,
    int                     backlog
);
static void                 on_terminate
(
    int                     signo
);
static Sigfunc *            PrivateSignal
(
    int                     signo,
    Sigfunc                *func
);
static unsigned int         resolveHost
(
    char                   *host
);
static Sigfunc *            Signal
(
    int                     signo,
    Sigfunc                *func
);
static int                  Socket
(
    int                     family,
    int                     type,
    int                     protocol
);
static int                  tcp_connect
(
    unsigned int            ip,
    unsigned short int      port,
    unsigned int            timeout
);
static void                 tcprelay
(
    unsigned int            remoteHost,
    unsigned short int      remotePort
);
static void                 terminate
(
    void
);
static void                 usage
(
    char                   *arg
);
static ssize_t              writen
(
    int                     fd,
    void                   *vptr,
    size_t                  n
);

/************************************************************************
 *                                                                      *
 *                            Static Global Var                         *
 *                                                                      *
 ************************************************************************/

static int          acceptSocket    = -1;
static int          connectSocket   = -1;
static int          remoteSocket    = -1;
static unsigned int supportftp      = 0;

/************************************************************************/

static int Accept ( int s, struct sockaddr *addr, size_t *addrlen )
{
    int n;

    n   = accept( s, addr, addrlen );
    if ( -1 == n )
    {
        perror( "accept error" );
        exit( EXIT_FAILURE );
    }
    return( n );
}  /* end of Accept */

static void Atexit ( void ( * func ) ( void ) )
{
    if ( 0 != atexit( func ) )
    {
        perror( "atexit error" );
        exit( EXIT_FAILURE );
    }
    return;
}  /* end of Atexit */

static void Bind ( int s, struct sockaddr *addr, size_t addrlen )
{
    if ( -1 == bind( s, ( const struct sockaddr * )addr, addrlen ) )
    {
        perror( "bind error" );
        exit( EXIT_FAILURE );
    }
    return;
}  /* end of Bind */

static void daemon_init ( char *workdir, mode_t mask )
{
    int i, j;

    chdir( "/tmp" );
    if ( 0 != Fork() )
    {
        exit( EXIT_SUCCESS );
    }
    setsid();
    Signal( SIGHUP, SIG_IGN );
    if ( 0 != Fork() )
    {
        exit( EXIT_SUCCESS );
    }
    chdir( workdir );
    umask( mask );
    if ( -1 == ( j = open( "/dev/null", O_RDWR ) ) )
    {
        perror( "open error" );
        exit( EXIT_FAILURE );
    }
    Dup2( j, 0 );
    Dup2( j, 1 );
    Dup2( j, 2 );
    j   = getdtablesize();
    for ( i = 3; i < j; i++ )
    {
        close( i );
    }
    return;
}  /* end of daemon_init */

static void doFtpRelay ( int connectSocket, int remoteSocket )
{
    unsigned char       buf[1025];
    unsigned int        num[6];
    struct pollfd       fds[4];
    int                 c;
    struct sockaddr_in  localAddr0, localAddr1, localAddr;
    size_t              localAddrLen0, localAddrLen1, localAddrLen;
    unsigned int        portRemoteIp, pasvRemoteIp, Ip;
    unsigned short int  portRemotePort, portLocalPort, pasvRemotePort, pasvLocalPort, Port;
    int                 portAcceptSocket, portConnectSocket, portRemoteSocket;
    int                 pasvAcceptSocket, pasvConnectSocket, pasvRemoteSocket;

    portAcceptSocket    = -1;
    portConnectSocket   = -1;
    portRemoteSocket    = -1;
    pasvAcceptSocket    = -1;
    pasvConnectSocket   = -1;
    pasvRemoteSocket    = -1;
    portRemoteIp        = 0;
    pasvRemoteIp        = 0;
    portRemotePort      = 0;
    portLocalPort       = 0;
    pasvRemotePort      = 0;
    pasvLocalPort       = 0;
    localAddrLen0       = sizeof( localAddr0 );
    localAddrLen1       = sizeof( localAddr1 );
    memset( &localAddr0, 0, localAddrLen0 );
    memset( &localAddr1, 0, localAddrLen1 );
    Getsockname( connectSocket, ( struct sockaddr * )&localAddr0, &localAddrLen0 );
    Getsockname( remoteSocket, ( struct sockaddr * )&localAddr1, &localAddrLen1 );
    for ( c = 0; c < 4; c++ )
    {
        fds[c].fd       = -1;
        fds[c].events   = POLLRDNORM;
    }
    fds[0].fd           = connectSocket;
    fds[1].fd           = remoteSocket;
    for ( ; ; )
    {
        c   = poll( fds, 4, -1 );
        if ( c < 0 )
        {
            perror( "poll error" );
            goto doFtpRelay_exit;
        }
        else if ( c == 0 )
        {
            fprintf( stderr, "poll() failed for timeout.\n" );
            goto doFtpRelay_exit;
        }
        if ( fds[0].revents & ( POLLRDNORM | POLLERR ) )
        {
            c   = read( connectSocket, buf, sizeof( buf ) - 1 );
            if ( c < 0 )
            {
                goto doFtpRelay_exit;
            }
            else if ( 0 == c )
            {
                goto doFtpRelay_exit;
            }
            if ( 0 == strncasecmp( buf, "PORT ", 5 ) )
            {
                buf[c]              = '\0';
                sscanf( buf + 5, "%u,%u,%u,%u,%u,%u", num, num + 1, num + 2, num + 3, num + 4, num + 5 );
                portRemoteIp        = htonl( ( num[0] << 24 ) + ( num[1] << 16 ) + ( num[2] << 8 ) + num[3] );
                portRemotePort      = htons( num[4] * 256 + num[5] );
                portAcceptSocket    = initTcpServer( 0 );
                fds[2].fd           = portAcceptSocket;
                localAddrLen        = sizeof( localAddr );
                memset( &localAddr, 0, localAddrLen );
                Getsockname( portAcceptSocket, ( struct sockaddr * )&localAddr, &localAddrLen );
                portLocalPort       = localAddr.sin_port;
                Ip                  = ntohl( localAddr1.sin_addr.s_addr );
                Port                = ntohs( portLocalPort );
                num[0]              = ( Ip   / 0x1000000 ) % 0x100;
                num[1]              = ( Ip   / 0x10000   ) % 0x100;
                num[2]              = ( Ip   / 0x100     ) % 0x100;
                num[3]              = ( Ip   / 0x1       ) % 0x100;
                num[4]              = ( Port / 0x100     ) % 0x100;
                num[5]              = ( Port / 0x1       ) % 0x100;
                sprintf( buf, "PORT %u,%u,%u,%u,%u,%u\r\n", num[0], num[1], num[2], num[3], num[4], num[5] );
                c                   = strlen( buf );
            }
            if ( writen( remoteSocket, buf, c ) < 0 )
            {
                goto doFtpRelay_exit;
            }
        }
        if ( fds[1].revents & ( POLLRDNORM | POLLERR ) )
        {
            c   = read( remoteSocket, buf, sizeof( buf ) - 1 );
            if ( c < 0 )
            {
                goto doFtpRelay_exit;
            }
            else if ( 0 == c )
            {
                goto doFtpRelay_exit;
            }
            if ( 0 == strncasecmp( buf, "227 Entering Passive Mode (", 27 ) )
            {
                buf[c]              = '\0';
                sscanf( buf + 27, "%u,%u,%u,%u,%u,%u", num, num + 1, num + 2, num + 3, num + 4, num + 5 );
                pasvRemoteIp        = htonl( ( num[0] << 24 ) + ( num[1] << 16 ) + ( num[2] << 8 ) + num[3] );
                pasvRemotePort      = htons( num[4] * 256 + num[5] );
                pasvAcceptSocket    = initTcpServer( 0 );
                fds[3].fd           = pasvAcceptSocket;
                localAddrLen        = sizeof( localAddr );
                memset( &localAddr, 0, localAddrLen );
                Getsockname( pasvAcceptSocket, ( struct sockaddr * )&localAddr, &localAddrLen );
                pasvLocalPort       = localAddr.sin_port;
                Ip                  = ntohl( localAddr0.sin_addr.s_addr );
                Port                = ntohs( pasvLocalPort );
                num[0]              = ( Ip   / 0x1000000 ) % 0x100;
                num[1]              = ( Ip   / 0x10000   ) % 0x100;
                num[2]              = ( Ip   / 0x100     ) % 0x100;
                num[3]              = ( Ip   / 0x1       ) % 0x100;
                num[4]              = ( Port / 0x100     ) % 0x100;
                num[5]              = ( Port / 0x1       ) % 0x100;
                sprintf
                (
                    buf,
                    "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",
                    num[0],
                    num[1],
                    num[2],
                    num[3],
                    num[4],
                    num[5]
                );
                c                   = strlen( buf );
            }
            if ( writen( connectSocket, buf, c ) < 0 )
            {
                goto doFtpRelay_exit;
            }
        }
        if ( fds[2].revents & ( POLLRDNORM | POLLERR ) )
        {
            localAddrLen        = sizeof( localAddr );
            memset( &localAddr, 0, localAddrLen );
            portConnectSocket   = Accept( portAcceptSocket, ( struct sockaddr * )&localAddr, &localAddrLen );
            close( portAcceptSocket );
            portAcceptSocket    = -1;
            fds[2].fd           = -1;
            portRemoteSocket    = tcp_connect( portRemoteIp, portRemotePort, 75 );
            if ( portRemoteSocket >= 0 )
            {
                doTcpRelay( portConnectSocket, portRemoteSocket );
                shutdown( portRemoteSocket, SHUT_RDWR );
                close( portRemoteSocket );
                portRemoteSocket    = -1;
            }
            shutdown( portConnectSocket, SHUT_RDWR );
            close( portConnectSocket );
            portConnectSocket   = -1;
        }
        if ( fds[3].revents & ( POLLRDNORM | POLLERR ) )
        {
            localAddrLen        = sizeof( localAddr );
            memset( &localAddr, 0, localAddrLen );
            pasvConnectSocket   = Accept( pasvAcceptSocket, ( struct sockaddr * )&localAddr, &localAddrLen );
            close( pasvAcceptSocket );
            pasvAcceptSocket    = -1;
            fds[3].fd           = -1;
            pasvRemoteSocket    = tcp_connect( pasvRemoteIp, pasvRemotePort, 75 );
            if ( pasvRemoteSocket >= 0 )
            {
                doTcpRelay( pasvConnectSocket, pasvRemoteSocket );
                shutdown( pasvRemoteSocket, SHUT_RDWR );
                close( pasvRemoteSocket );
                pasvRemoteSocket    = -1;
            }
            shutdown( pasvConnectSocket, SHUT_RDWR );
            close( pasvConnectSocket );
            pasvConnectSocket   = -1;
        }
    }  /* end of for */

doFtpRelay_exit:

    if ( -1 != portAcceptSocket )
    {
        close( portAcceptSocket );
        portAcceptSocket    = -1;
    }
    if ( -1 != portConnectSocket )
    {
        close( portConnectSocket );
        portConnectSocket   = -1;
    }
    if ( -1 != portRemoteSocket )
    {
        close( portRemoteSocket );
        portRemoteSocket    = -1;
    }
    if ( -1 != pasvAcceptSocket )
    {
        close( pasvAcceptSocket );
        pasvAcceptSocket    = -1;
    }
    if ( -1 != pasvConnectSocket )
    {
        close( pasvConnectSocket );
        pasvConnectSocket   = -1;
    }
    if ( -1 != pasvRemoteSocket )
    {
        close( pasvRemoteSocket );
        pasvRemoteSocket    = -1;
    }
    return;
}  /* end of doFtpRelay */

static void doTcpRelay
(
    int connectSocket,
    int remoteSocket
)
{
    unsigned char   buf[1024];
    struct pollfd   fds[2];
    int             c;

    fds[0].fd       = connectSocket;
    fds[0].events   = POLLRDNORM;
    fds[1].fd       = remoteSocket;
    fds[1].events   = POLLRDNORM;
    for ( ; ; )
    {
        c   = poll( fds, 2, -1 );
        if ( c < 0 )
        {
            perror( "poll error" );
            goto doTcpRelay_exit;
        }
        else if ( 0 == c )
        {
            fprintf( stderr, "poll() failed for timeout\n" );
            goto doTcpRelay_exit;
        }
        if ( fds[0].revents & ( POLLRDNORM | POLLERR ) )
        {
            c   = read( connectSocket, buf, sizeof( buf ) );
            if ( c < 0 )
            {
                goto doTcpRelay_exit;
            }
            else if ( 0 == c )
            {
                goto doTcpRelay_exit;
            }
            if ( writen( remoteSocket, buf, c ) < 0 )
            {
                goto doTcpRelay_exit;
            }
        }
        if ( fds[1].revents & ( POLLRDNORM | POLLERR ) )
        {
            c   = read( remoteSocket, buf, sizeof( buf ) );
            if ( c < 0 )
            {
                goto doTcpRelay_exit;
            }
            else if ( 0 == c )
            {
                goto doTcpRelay_exit;
            }
            if ( writen( connectSocket, buf, c ) < 0 )
            {
                goto doTcpRelay_exit;
            }
        }
    }  /* end of for */

doTcpRelay_exit:

    return;
}  /* end of doTcpRelay */

static int Dup2 ( int oldfd, int newfd )
{
    int fd;

    fd  = dup2( oldfd, newfd );
    if ( -1 == fd )
    {
        perror( "dup2 error" );
        exit( EXIT_FAILURE );
    }
    return( fd );
}  /* end of Dup2 */

static pid_t Fork ( void )
{
    pid_t pid;

    pid = fork();
    if ( -1 == pid )
    {
        perror( "fork error" );
        exit( EXIT_FAILURE );
    }
    return( pid );
}  /* end of Fork */

static void Getsockname ( int s, struct sockaddr *name, size_t *namelen )
{
    if ( -1 == getsockname( s, name, namelen ) )
    {
        perror( "getsockname error" );
        exit( EXIT_FAILURE );
    }
    return;
}  /* end of Getsockname */

static void init_signal ( void )
{
    unsigned int    i;

    Atexit( terminate );
    for ( i = 1; i < 9; i++ )
    {
        Signal( i, on_terminate );
    }
    Signal( SIGTERM, on_terminate );
    Signal( SIGALRM, on_terminate );
    return;
}  /* end of init_signal */

static int initTcpServer ( unsigned short int localPort )
{
    struct sockaddr_in  sin;
    int                 i;
    int                 acceptSocket;

    acceptSocket        = Socket( PF_INET, SOCK_STREAM, IPPROTO_TCP );
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
    Bind( acceptSocket, ( struct sockaddr * )&sin, sizeof( sin ) );
    Listen( acceptSocket, 256 );
    return( acceptSocket );
}  /* end of initTcpServer */

static void Listen( int s, int backlog )
{
    if ( -1 == listen( s, backlog ) )
    {
        perror( "listen error" );
        exit( EXIT_FAILURE );
    }
    return;
}  /* end of Listen */

static void on_terminate ( int signo )
{
    /*
     * ��ʾ�ã����Ƽ����źž����ʹ��fprintf()
     */
    fprintf( stderr, "\nsigno = %d\n", signo );
    exit( EXIT_SUCCESS );
}  /* end of on_terminate */

static Sigfunc * PrivateSignal
(
    int         signo,
    Sigfunc    *func
)
{
    struct sigaction    act, oact;

    act.sa_handler  = func;
    sigemptyset( &act.sa_mask );
    act.sa_flags    = 0;
    if ( signo == SIGALRM )
    {
#ifdef  SA_INTERRUPT
        /*
         * SunOS 4.x
         */
        act.sa_flags   |= SA_INTERRUPT;
#endif
    }
    else
    {
#ifdef  SA_RESTART
        /*
         * SVR4, 4.4BSD
         */
        act.sa_flags   |= SA_RESTART;
#endif
    }
    if ( sigaction( signo, &act, &oact ) < 0 )
    {
        return( SIG_ERR );
    }
    return( oact.sa_handler );
}  /* end of PrivateSignal */

static unsigned int resolveHost ( char *host )
{
    struct hostent *he;
    unsigned int    ip;

    if ( ( he = gethostbyname( host ) ) == NULL )
    {
        ip  = inet_addr( host );
        if ( INADDR_NONE == ip )
        {
            fprintf( stderr, "Unknown host %s\n", host );
            exit( EXIT_FAILURE );
        }
    }
    else
    {
        memcpy( &ip, he->h_addr_list[0], sizeof( unsigned int ) );
    }
    return( ip );
}  /* end of resolveHost */

static Sigfunc * Signal ( int signo, Sigfunc *func )
{
    Sigfunc    *sigfunc;

    if ( ( sigfunc = PrivateSignal( signo, func ) ) == SIG_ERR )
    {
        perror( "signal" );
        exit( EXIT_FAILURE );
    }
    return( sigfunc );
}  /* end of Signal */

static int Socket ( int family, int type, int protocol )
{
    int n;

    if ( ( n = socket( family, type, protocol ) ) < 0 )
    {
        perror( "socket error" );
        exit( EXIT_FAILURE );
    }
    return( n );
}  /* end of Socket */

static int tcp_connect
(
    unsigned int        ip,
    unsigned short int  port,
    unsigned int        timeout
)
{
    struct sockaddr_in  sin;
    int                 s, flags, error_value, n;
    fd_set              rset, wset;
    struct timeval      tv;
    size_t              error_value_len;

    memset( &sin, 0, sizeof( sin ) );
    sin.sin_family      = AF_INET;
    sin.sin_addr.s_addr = ip;
    sin.sin_port        = port;
    error_value         = 0;
    error_value_len     = sizeof( error_value );
    if ( ( s = socket( PF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        perror( "socket error" );
        return( -1 );
    }
    if ( ( flags = fcntl( s, F_GETFL, 0 ) ) < 0 )
    {
        perror( "fcntl( F_GETFL ) error" );
        goto tcp_connect_1;
    }
    if ( fcntl( s, F_SETFL, flags | O_NONBLOCK ) < 0 )
    {
        perror( "fcntl( F_SETFL ) error" );
        goto tcp_connect_1;
    }
    if ( ( n = connect( s, ( struct sockaddr * )&sin, sizeof( sin ) ) ) < 0 )
    {
        if ( errno != EINPROGRESS )
        {
            /*
             * connect error: Connection refused
             *
             * perror( "connect error" );
             */
            goto tcp_connect_1;
        }
    }
    if ( n == 0 )
    {
        goto tcp_connect_0;
    }
    FD_ZERO( &rset );
    FD_SET( s, &rset );
    wset                = rset;
    tv.tv_sec           = timeout;
    tv.tv_usec          = 0;
    n                   = select( s + 1, &rset, &wset, NULL, timeout ? &tv : NULL );
    if ( n < 0 )
    {
        perror( "select error" );
        goto tcp_connect_1;
    }
    else if ( 0 == n )
    {
        /*
         * fprintf( stderr, "select returned with timeout.\n" );
         */
        goto tcp_connect_1;
    }
    else if ( FD_ISSET( s, &rset ) || FD_ISSET( s, &wset ) )
    {
        if ( getsockopt( s, SOL_SOCKET, SO_ERROR, &error_value, &error_value_len ) < 0 )
        {
            goto tcp_connect_1;
        }
    }
    else
    {
        fprintf( stderr, "some error occur in tcp_connect()\n" );
        goto tcp_connect_1;
    }

tcp_connect_0:

    if ( fcntl( s, F_SETFL, flags ) < 0 )
    {
        perror( "fcntl( F_SETFL ) error" );
        goto tcp_connect_1;
    }
    if ( 0 != error_value )
    {
        goto tcp_connect_1;
    }
    n                   = 1;
    setsockopt( s, SOL_SOCKET, SO_KEEPALIVE, &n, sizeof( n ) );
    return( s );

tcp_connect_1:

    close( s );
    return( -1 );
}  /* end of tcp_connect */

static void tcprelay
(
    unsigned int        remoteHost,
    unsigned short int  remotePort
)
{
    struct sockaddr_in  clientAddr;
    size_t              clientAddrLen;
    pid_t               pid;

    for ( ; ; )
    {
        clientAddrLen   = sizeof( clientAddr );
        memset( &clientAddr, 0, clientAddrLen );
        connectSocket   = Accept
        (
            acceptSocket,
            ( struct sockaddr * )&clientAddr,
            &clientAddrLen
        );
        pid             = Fork();
        if ( 0 == pid )
        {
            close( acceptSocket );
            acceptSocket    = -1;
            pid             = Fork();
            if ( 0 == pid )
            {
                if ( ( remoteSocket = tcp_connect( remoteHost, remotePort, 75 ) ) >= 0 )
                {
                    if ( 0 == supportftp )
                    {
                        doTcpRelay( connectSocket, remoteSocket );
                    }
                    else
                    {
                        doFtpRelay( connectSocket, remoteSocket );
                    }
                    shutdown( remoteSocket, SHUT_RDWR );
                    close( remoteSocket );
                    remoteSocket    = -1;
                    shutdown( connectSocket, SHUT_RDWR );
                }
            }
            close( connectSocket );
            connectSocket   = -1;
            exit( EXIT_SUCCESS );
        }
        else
        {
            close( connectSocket );
            connectSocket   = -1;
            if ( pid != waitpid( pid, NULL, 0 ) )
            {
                perror( "waitpid error" );
            }
        }
    }  /* end of for */
    return;
}  /* end of tcprelay */

static void terminate ( void )
{
    if ( -1 != remoteSocket )
    {
        close( remoteSocket );
        remoteSocket    = -1;
    }
    if ( -1 != connectSocket )
    {
        close( connectSocket );
        connectSocket   = -1;
    }
    if ( -1 != acceptSocket )
    {
        close( acceptSocket );
        acceptSocket    = -1;
    }
    return;
}  /* end of terminate */

static void usage ( char *arg )
{
    fprintf
    (
        stderr,
        "Usage: %s [-h] [-v] [-d] [-f] [-l localPort] [-x remoteHost] [-y remotePort]\n",
        arg
    );
    exit( EXIT_FAILURE );
}  /* end of usage */

static ssize_t writen ( int fd, void *vptr, size_t n )
{
    ssize_t         nwrite;
    size_t          nleft;
    unsigned char  *ptr = vptr;

    nleft   = n;
    while ( nleft > 0 )
    {
        if ( ( nwrite = write( fd, ptr, nleft ) ) <= 0 )
        {
            if ( EINTR == errno || EAGAIN == errno )
            {
                /*
                 * call write() again
                 */
                nwrite = 0;
            }
            else
            {
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

int main ( int argc, char * argv[] )
{
    int                 c;
    struct rlimit       res;
    unsigned int        remoteHost  = 0;
    unsigned short int  remotePort  = 0;
    unsigned short int  localPort   = 0;
    unsigned int        daemon      = 0;

    if ( 1 == argc )
    {
        usage( argv[0] );
    }
    opterr          = 0;
    while ( ( c = getopt( argc, argv, ":hdfl:vx:y:" ) ) != EOF )
    {
        switch ( c )
        {
        case 'd':
            daemon++;
            break;
        case 'f':
            supportftp++;
            break;
        case 'l':
            localPort   = htons( ( unsigned short int )strtoul( optarg, NULL, 0 ) );
            break;
        case 'x':
            remoteHost  = resolveHost( optarg );
            break;
        case 'y':
            remotePort  = htons( ( unsigned short int )strtoul( optarg, NULL, 0 ) );
            break;
        case 'v':
            fprintf( stderr, "%s ver "VERSION"\n", argv[0] );
            return( EXIT_SUCCESS );
        case 'h':
        case '?':
        default :
            usage( argv[0] );
            break;
        }  /* end of switch */
    }  /* end of while */
    argc           -= optind;
    argv           += optind;
    if ( 0 == remoteHost || 0 == remotePort || 0 == localPort )
    {
        fprintf( stderr, "Checking your [-l localPort] [-x remoteHost] [-y remotePort]\n" );
        return( EXIT_FAILURE );
    }
    if ( ( c = sysconf( _SC_OPEN_MAX ) ) < 0 )
    {
        fprintf( stderr, "sysconf failed\n" );
    }
    else
    {
        fprintf( stderr, "sysconf( _SC_OPEN_MAX ) = %d\n", c );
    }
    memset( &res, 0, sizeof( struct rlimit ) );
    if ( getrlimit( RLIMIT_NOFILE, &res ) < 0 )
    {
        perror( "getrlimit RLIMIT_NOFILE" );
    }
    else
    {
        fprintf
        (
            stderr,
            "rlim_cur = %d rlim_max = %d\n",
            ( int )res.rlim_cur,
            ( int )res.rlim_max
        );
    }
#ifdef Aix
    res.rlim_cur    = c;
#else
    res.rlim_cur    = res.rlim_max;
#endif
    if ( setrlimit( RLIMIT_NOFILE, &res ) < 0 )
    {
        perror( "setrlimit RLIMIT_NOFILE" );
    }
    else
    {
        fprintf( stderr, "Changing ... ...\n" );
    }
    memset( &res, 0, sizeof( struct rlimit ) );
    if ( getrlimit( RLIMIT_NOFILE, &res ) < 0 )
    {
        perror( "getrlimit RLIMIT_NOFILE" );
    }
    else
    {
        fprintf
        (
            stderr,
            "rlim_cur = %d rlim_max = %d\n",
            ( int )res.rlim_cur,
            ( int )res.rlim_max
        );
    }
    if ( daemon )
    {
        daemon_init( "/", 0 );
    }
    init_signal();
    acceptSocket    = initTcpServer( localPort );
    tcprelay( remoteHost, remotePort );
    return( EXIT_SUCCESS );
}  /* end of main */

/************************************************************************/


