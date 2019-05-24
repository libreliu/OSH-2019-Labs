#ifndef LIBSOCK_INCLUDED
#define LIBSOCK_INCLUDED

#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

void lsock_on_error(const char *s);
int lsock_accept(int fd, struct sockaddr *sa, socklen_t *salenptr);
void lsock_bind(int fd, const struct sockaddr *sa, socklen_t salen);
void lsock_connect(int fd, const struct sockaddr *sa, socklen_t salen);
void lsock_listen(int fd, int backlog);
int lsock_socket(int family, int type, int protocol);
ssize_t lsock_read(int fd, void *ptr, size_t nbytes);
ssize_t lsock_write(int fd, const void *ptr, size_t nbytes);
void lsock_close(int fd);
void lsock_set_nonblocking(int fd);
void lsock_log(const char *str, ...);

#define LOG(fmt, ...) lsock_log(fmt, ##__VA_ARGS__)
//#define LOG(fmt, ...) 

#endif