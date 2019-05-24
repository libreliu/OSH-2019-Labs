#define MAX_EVENTS 10
#define MAX_CONN 1000
#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define HTTP_READ_BUF 2048
#define HTTP_WRITE_BUF 50000
#define MAX_PATH_LEN 1024
#define MAX_HOST_LEN 1024
#define RWCYCLE_MAX_TRIES 5

#include <sys/stat.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <sys/resource.h>
#include <stdio.h>
#include <signal.h>

#include "libsock.h"

typedef struct {
    enum {
        SOCKFD, FILEFD
    } type;
    enum {
        // HTTP Ones
        NOT_ALLOCATED, WAIT_FOR_REQUEST_METHOD, WAIT_FOR_REQUEST_CONTENT, WAIT_FOR_REQUEST_END, PREPARE_RESPONSE_HEADER, SEND_RESPONSE_HEADER, SEND_RESPONSE_CONTENT,
        // File Ones
        READING_FILEDATA,
        // LISTEN ones
        LISTENING
    } state;
    char read_buf[HTTP_READ_BUF];
    char write_buf[HTTP_WRITE_BUF];
    int read_buf_offset; // marks the end; 
                         // notice that read_buf == fseek(SEEK_BEGIN), therefore no need to maintain two ptr
    int write_buf_offset;
    int write_buf_sent;
    int request_last_scan_offset;
    int resp_code;
    char get_path[MAX_PATH_LEN];
    struct sockaddr_in clnt_addr;
    // Associated fd: socket fd for file; file fd for socket
    int associated_fd;
    int netwrite_ready;
    int fileread_ready;

} conn_info;

static conn_info *conns; 
static int conn_max;

void zero_new_conn(int fd) {
    conns[fd].get_path[0] = '\0';
    // No data in buffer available
    conns[fd].read_buf_offset = 0;
    conns[fd].write_buf_offset = 0;
    conns[fd].request_last_scan_offset = 0;
    conns[fd].associated_fd = 0;
    conns[fd].write_buf_sent = 0;
    conns[fd].resp_code = 0;
    conns[fd].state = NOT_ALLOCATED;
    conns[fd].netwrite_ready = 0;
    conns[fd].fileread_ready = 0;

}

int init() {
    // Get max file descriptior limit
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
        lsock_on_error("getrlimit: nofile");
    }
    LOG("FD Limit: Soft=%d, Hard=%d", rlim.rlim_cur, rlim.rlim_max);

    // Init (conn_info *) array
    conns = malloc(sizeof(conn_info) * rlim.rlim_cur);
    for (int i = 0; i < rlim.rlim_cur; i++) {
        zero_new_conn(i);
    }
    conn_max = rlim.rlim_cur;
}

// May return null if ENAMETOOLONG/EACCES/ENOENT
char *real_path(const char *src) {
    static char path_tmp[MAX_PATH_LEN * 2];
    static char real_ptmp[MAX_PATH_LEN * 2];
    getcwd(path_tmp, sizeof(path_tmp));
    char *ret = realpath(strcat(path_tmp, src), real_ptmp);
    return ret;
}

// Requires canonical path
int path_valid(const char *src) {
    static char path_tmp[MAX_PATH_LEN * 2];
    getcwd(path_tmp, sizeof(path_tmp));
    int len = strlen(path_tmp);
    for (int i = 0; i < len; i++) {
        if (src[i] != path_tmp[i])
            return 0;
    }
    return 1;
}

void do_reject (int fd, const char *reason) {
    zero_new_conn(fd);
    LOG("rejecd client ip=%s, fd=%d due to %s", inet_ntoa(conns[fd].clnt_addr.sin_addr), fd, reason);
    close(fd); // Will automatically remove fd from epoll interest list
}

// Remote closed
void do_close (int fd, const char *reason) {
    zero_new_conn(fd);
    LOG("closed client ip=%s, fd=%d due to %s", inet_ntoa(conns[fd].clnt_addr.sin_addr), fd, reason);
    close(fd); // Will automatically remove fd from epoll interest list
}

/* entire request *should not exceed* the limit of HTTP_READ_BUF */
// read_buf_offset points to the first vacant position in buf (= chars read, in initial condition)
//  HTTP_READ_BUF if full
typedef uint32_t epoll_event_t; // Might be platform dependent, whatever

void do_use_fd(int fd, epoll_event_t evnt, int epollfd) {

    // Update state vars

    if (evnt & EPOLLOUT != 0) {
        if (conns[fd].type == SOCKFD) {
            conns[conns[fd].associated_fd].netwrite_ready = 1;
        }
    }

    if (conns[fd].type == SOCKFD) {
        if (conns[fd].state == WAIT_FOR_REQUEST_METHOD || conns[fd].state == WAIT_FOR_REQUEST_CONTENT
         || conns[fd].state == WAIT_FOR_REQUEST_END) {
            while(1) {
                // Read everything out; if larger, reject
                if (conns[fd].read_buf_offset < HTTP_READ_BUF) {
                    int n = lsock_read(fd, conns[fd].read_buf + conns[fd].read_buf_offset, HTTP_READ_BUF - conns[fd].read_buf_offset);
                    if (n == -1) {
                        if (errno == EAGAIN) {
                            // Good, everything read
                            break;
                        } else {
                            do_reject(fd, "read error");
                            return;
                        }
                    } else if (n == 0) {
                        // Remote site had closed connection, close
                        do_close(fd, "read() return 0");
                        return;
                    } else {
                        conns[fd].read_buf_offset += n;
                        // Continue reading
                    }
                } else {
                    // Buffer full, check if there are no more characters available
                    int m = lsock_read(fd, conns[fd].read_buf, 1);
                    if (m == -1 && errno != EAGAIN) {
                        do_reject(fd, "read error (buffer full)");
                        return;
                    } else if (m != -1 && m != 0) {
                        do_reject(fd, "request too large");
                        return;
                    } else if (m == 0) {
                        // Remote site had closed connection, close
                        do_close(fd, "read() return 0");
                        return;
                    }
                    // Good, no more characters available
                    break;
                }
            }
        }
    struct epoll_event ev;
again:
        switch (conns[fd].state) {
            case WAIT_FOR_REQUEST_METHOD:
                LOG("entered WAIT_FOR_REQUEST_METHOD for fd=%d", fd);
                if (conns[fd].read_buf_offset >= 4) {
                    if (conns[fd].read_buf[0] == 'G' && conns[fd].read_buf[1] == 'E' 
                     && conns[fd].read_buf[2] == 'T' && conns[fd].read_buf[3] == ' ') {
                        conns[fd].state = WAIT_FOR_REQUEST_CONTENT;
                        goto again;
                     } else {
                         // Unsupported Connection Type, reject
                         do_reject(fd, "unsupported request method");
                         return;
                     }
                }
                break;
            case WAIT_FOR_REQUEST_CONTENT:
                LOG("entered WAIT_FOR_REQUEST_CONTENT for fd=%d", fd);
                if (conns[fd].read_buf_offset >= 5) {
                    if (conns[fd].read_buf[4] == ' ') {
                        do_reject(fd, "malformed request");
                    } else {
                        // Search for space
                        int ending_space = 5;
                        for (; ending_space < conns[fd].read_buf_offset; ending_space++) {
                            if (conns[fd].read_buf[ending_space] == '\0') {
                                do_reject(fd, "malformed request(with \0)");
                                return;
                            } else if (conns[fd].read_buf[ending_space] == ' ')
                                break;
                        }
                        if (ending_space == conns[fd].read_buf_offset) {
                            // Wait for more data..
                            return;
                        }
                        // TODO: PATH LENGTH CHECK
                        // Got ending_space, okay to separate path out
                        for (int i = 0; i < ending_space - 4; i++) {
                            conns[fd].get_path[i] = conns[fd].read_buf[i + 4];
                        }
                        conns[fd].get_path[ending_space - 4] = '\0';
                        conns[fd].request_last_scan_offset = ending_space;
                        conns[fd].state = WAIT_FOR_REQUEST_END;
                        goto again;
                    }
                } else {
                    // Waiting for more data..
                    return;
                }
                break;
            case WAIT_FOR_REQUEST_END:
                LOG("entered WAIT_FOR_REQUEST_END for fd=%d", fd);
                // Check if we have two \r\n\r\n, go kmp if bottleneck
                // Start at ending_space + 1
                //for (int i = conns[fd].request_last_scan_offset + 1; i < conns[fd].read_buf_offset; i++) {
                //TODO FIX ABOVE!!
                for (int i = 0; i < conns[fd].read_buf_offset - 3; i++) {
                    if (conns[fd].read_buf[i] == '\r' && conns[fd].read_buf[i+1] == '\n' 
                     && conns[fd].read_buf[i+2] == '\r' && conns[fd].read_buf[i+3] == '\n' ) {
                        // Good, request end!
                        conns[fd].state = PREPARE_RESPONSE_HEADER;
                        goto again;
                    }
                }
                break;
            case PREPARE_RESPONSE_HEADER: {
                LOG("entered PREPARE_RESPONSE_HEADER for fd=%d", fd);
                // 1. Prepare headers and put them into write_buf
                // 2. Open desired file and add into fd
                // Check if we have the file mentioned
                struct stat st;
                int new_fd;
                char *r_path = real_path(conns[fd].get_path);  // canonical path
                int resp_num = 500;
                if (r_path != NULL && (stat(r_path, &st) == -1) ) {
                    // 404
                    resp_num = 404;
                    perror("stat");
                    LOG("stat error, path: %s", r_path);
                } else {
                    if (r_path == NULL) { // EACCES/ENOENT/ENAMETOOLONG, etc
                        resp_num = 500;
                        LOG("not a valid file(EACCES/ENOENT/ENAMETOOLONG)");
                    } else if (!path_valid(r_path)) {
                        resp_num = 500;
                        LOG("not in docroot");
                    } else if (!S_ISREG(st.st_mode)) {  // Not a regular file
                        resp_num = 500;
                        LOG("not a regular file");
                    } else {
                        new_fd = open(r_path, O_RDONLY | O_NONBLOCK);
                        if (new_fd == -1) {
                            resp_num = 500;
                            LOG("open error");
                        } else {
                            conns[fd].associated_fd = new_fd;
                            conns[new_fd].associated_fd = fd; // new fd is cleared by proper close() involved in previous conn
                            conns[new_fd].state = READING_FILEDATA;
                            conns[new_fd].type = FILEFD;

                            resp_num = 200;
                        }
                    }
                }
                if (resp_num == 200) {
                    const char *msg = "HTTP/1.0 200 OK\r\nContent-Length: %d\r\n\r\n";
                    conns[fd].write_buf_offset = sprintf(conns[fd].write_buf, msg, st.st_size);
                } else if (resp_num == 404) {
                    const char *msg = "HTTP/1.0 404 Not Found\r\n\r\n";
                    conns[fd].write_buf_offset = sprintf(conns[fd].write_buf, msg);
                } else {
                    const char *msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n";
                    strcpy(conns[fd].write_buf, msg);
                    conns[fd].write_buf_offset = strlen(msg);
                }

                //ev.events = EPOLLOUT | EPOLLET;
                ev.events = EPOLLOUT;
                ev.data.fd = fd;
                if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd,
                            &ev) == -1) {
                    do_reject(new_fd, "epoll_ctl: CTL_MOD(file_sock)");
                    do_reject(fd, "epoll_ctl: CTL_MOD(net_sock)");
                    return;
                }

                conns[fd].resp_code = resp_num;
                conns[fd].state = SEND_RESPONSE_HEADER;
                goto again;
                break;
            }

            case SEND_RESPONSE_HEADER:
                LOG("entered SEND_RESPONSE_HEADER for fd=%d", fd);
                // Write until we have written everything
                if (conns[fd].write_buf_sent < conns[fd].write_buf_offset) { // check if we need to write
                    int n = lsock_write(fd, conns[fd].write_buf + conns[fd].write_buf_sent, conns[fd].write_buf_offset - conns[fd].write_buf_sent);
                    if (n == -1 && errno == EAGAIN) {
                        // Oops, we need to wait
                        return;
                    } else if (n == -1 && errno == EPIPE || n == 0) {
                        // Remote have closed conn
                        do_close(fd, "remote unexpected closed conn during SEND_RESPONSE_HEADER");
                        return;
                    } else {
                        if (n < conns[fd].write_buf_offset - conns[fd].write_buf_sent) {
                            // Can't write all data in one round, wait for another turn (Definitely got EAGAIN here)
                            conns[fd].write_buf_sent += n;
                            return;
                        } else {// Okay, all header sent!
                            conns[fd].write_buf_sent += n;
                        }
                    }
                }

                // Done sending headers, register for file send if necessary (check resp_code)
                if (conns[fd].resp_code == 200) {
                    // EPOLL DOES NOT SUPPORT REGULAR FILE!!

                    // ev.events = EPOLLIN | EPOLLET;
                    // ev.data.fd = conns[fd].associated_fd;
                    // if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conns[fd].associated_fd,
                    //             &ev) == -1) {
                    //                 // NOTICE REJECT ORDER, since reject will clear variables
                    //     do_reject(conns[fd].associated_fd, "epoll_ctl: file_sock(net_sock)");
                    //     do_reject(fd,"epoll_ctl: net_sock");
                    //     perror("error reason:");
                    //     return;
                    // }

                    conns[fd].write_buf_offset = 0;
                    conns[fd].write_buf_sent   = 0;
                    conns[fd].state = SEND_RESPONSE_CONTENT;
                } else {
                    // We can terminate now
                    do_close(fd, "500/404 normal termination");
                    return;
                }

                break;
            case SEND_RESPONSE_CONTENT:
                LOG("entered SEND_RESPONSE_CONTENT for fd=%d", fd);
                // Read to buf -> write buf -> read to buf -> write buf ...(MAXIMUM_M_TRIES)
                for (int tries = 0; tries < RWCYCLE_MAX_TRIES; tries++) {
                    LOG("%d %d",conns[fd].write_buf_sent, conns[fd].write_buf_offset);
                    int reached_eof = 0;
                    // Test if everything sent
                    if (conns[fd].write_buf_sent == conns[fd].write_buf_offset) {
                        conns[fd].write_buf_sent = conns[fd].write_buf_offset = 0;
                        if (HTTP_WRITE_BUF - conns[fd].write_buf_offset > 0) { // More room avail in buf (possibly duplicated)
                            int n = lsock_read(conns[fd].associated_fd, conns[fd].write_buf + conns[fd].write_buf_offset, HTTP_WRITE_BUF - conns[fd].write_buf_offset);
                            if (n == -1 && errno == EAGAIN) {
                                conns[fd].fileread_ready = 0;
                                return;
                            } else if (n == -1) {
                                // Unknown error; reject
                                do_reject(conns[fd].associated_fd, "read error during SEND_RESPONSE_CONTENT");
                                do_reject(fd, "read error during SEND_RESPONSE_CONTENT");
                                return;
                            } else if (n == 0) {
                                // Reached EOF
                                // The system considers EOF to be a state in which the file descriptor is 'readable'.
                                // But, of course, the EOF condition cannot be read. (Therefore using epoll here is okay)
                                reached_eof = 1;
                            } else {
                                conns[fd].write_buf_offset += n;
                            }
                        }
                    }
                    if (reached_eof && conns[fd].write_buf_sent == conns[fd].write_buf_offset) {
                        do_close(conns[fd].associated_fd, "finished connection");
                        do_close(fd, "finished connection");
                        return;
                    }
                    if (conns[fd].write_buf_offset - conns[fd].write_buf_sent > 0) {
                        int n = lsock_write(fd, conns[fd].write_buf + conns[fd].write_buf_sent, conns[fd].write_buf_offset - conns[fd].write_buf_sent);
                        if (n == -1 && errno == EAGAIN) {
                            conns[fd].netwrite_ready = 0;
                            return;
                        } else if (n == 0 || (n == -1 && errno == EPIPE)) {
                            // Remote closed conn
                            do_close(conns[fd].associated_fd, "remote closed connection during content transfer");
                            do_close(fd, "remote closed connection during content transfer");
                            return;
                        } else {
                            conns[fd].write_buf_sent += n;
                        }
                    }
                }
                
                break;
        }
    } else if (conns[fd].type == FILEFD) {
        conns[conns[fd].associated_fd].fileread_ready = 1;
    }
}

void sig_int_handler(int p) {
    LOG("Closing fd..");
    for (int i = 0; i < conn_max; i++) {
        if (conns[i].state != NOT_ALLOCATED) {
            close(i);
        }
    }
    LOG("Exiting..");
    exit(0);
}

int server_main() {

    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;

    /* Setup listening socket */
    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);

    listen_sock = lsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);
    serv_addr.sin_port = htons(BIND_PORT);
    // Bind
    lsock_bind(listen_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

    lsock_listen(listen_sock, MAX_CONN);

    /* Initialize Conn Structure */
    init();
    conns[listen_sock].state = LISTENING;

    /* Set SIGPIPE to avoid write() hang */
    struct sigaction nact;
    nact.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &nact, NULL) == 0) {
       LOG("SIGPIPE ignored");
    }

    /* Set SIGINT handler */
    nact.sa_handler = sig_int_handler;
    if (sigaction(SIGINT, &nact, NULL) == 0) {
       LOG("SIGINT Handler setup");
    }


    /* Initialize epoll */

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        lsock_on_error("epoll_create1");
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
        lsock_on_error("epoll_ctl: listen_sock");
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            lsock_on_error("epoll_wait");
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == listen_sock) {
                conn_sock = accept(listen_sock,
                                (struct sockaddr *) &clnt_addr, &clnt_addr_size);
                if (conn_sock == -1) {
                    lsock_on_error("accept");
                }
                LOG("Incoming connections from %s, fd=%d", inet_ntoa(clnt_addr.sin_addr), conn_sock);
                lsock_set_nonblocking(conn_sock);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                            &ev) == -1) {
                    lsock_on_error("epoll_ctl: conn_sock");
                }

                /* Initialize Connection Info */
                conns[conn_sock].state = WAIT_FOR_REQUEST_METHOD;
                conns[conn_sock].type  = SOCKFD;
                conns[conn_sock].clnt_addr = clnt_addr;

            } else {
                // Process FD that have inbound traffic
                do_use_fd(events[n].data.fd, events[n].events, epollfd);
            }
        }
    }
}