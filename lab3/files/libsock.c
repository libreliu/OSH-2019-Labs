#include "libsock.h"

void lsock_on_error(const char *s) {
    perror(s);
	exit(1);
}

int lsock_accept(int fd, struct sockaddr *sa, socklen_t *salenptr) {
	int n;
again:
	if ( (n = accept(fd, sa, salenptr)) < 0) {
        /*  EINTR  
              The accept() function was  interrupted  by  a  signal  that  was
              caught before a valid connection arrived.
         *  ECONNABORTED
              A connection has been aborted.
         */
		if ((errno == ECONNABORTED) || (errno == EINTR))
			goto again;
		else
			lsock_on_error("accept");
	}
	return n;
}

void lsock_bind(int fd, const struct sockaddr *sa, socklen_t salen) {
	if (bind(fd, sa, salen) < 0)
		lsock_on_error("bind");
}

void lsock_connect(int fd, const struct sockaddr *sa, socklen_t salen) {
	if (connect(fd, sa, salen) < 0)
		lsock_on_error("connect");
}

void lsock_listen(int fd, int backlog) {
	if (listen(fd, backlog) < 0)
		lsock_on_error("listen");
}

int lsock_socket(int family, int type, int protocol) {
	int n;

	if ( (n = socket(family, type, protocol)) < 0)
		lsock_on_error("socket");
	return n;
}


ssize_t lsock_read(int fd, void *ptr, size_t nbytes) {
	ssize_t n;

again:
	if ( (n = read(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)
			goto again;
		else
			return -1;
	}
	return n;
}

ssize_t lsock_write(int fd, const void *ptr, size_t nbytes) {
	ssize_t n;

again:
	if ( (n = write(fd, ptr, nbytes)) == -1) {
		if (errno == EINTR)
			goto again;
		else
			return -1;
	}
	return n;
}

void lsock_close(int fd) {
	if (close(fd) == -1)
		lsock_on_error("close");
}

void lsock_set_nonblocking(int fd) {
	int ret;
	if ((ret = fcntl(fd, F_GETFL)) == -1) {
		lsock_on_error("fcntl: getfl");
	}
	ret |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, ret) == -1) {
		lsock_on_error("fcntl: setfl");
	}
}

void lsock_log(const char *str, ...)  {
	va_list aptr;
	va_start(aptr, str);
	printf("[LOG] ");
	vprintf(str, aptr);
	printf("\n");
	va_end(aptr);
}