
/*
 *Author : Stone Li <liqinglin@gmail.com>
 
 */


#ifndef FD_H
#define FD_H

extern int fd_copy();
extern int fd_move();
extern int saferead(int fd,char *buf,int len);
extern int safewrite(int fd,char *buf,int len) ;
extern int ssize_t writen ( int fd, void *vptr, size_t n );



#endif
