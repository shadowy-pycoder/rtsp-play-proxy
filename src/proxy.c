/*
 * Copyright (C) 2026 shadowy-pycoder
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#ifndef __USE_POSIX
#define __USE_POSIX
#endif
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define FLAG_IMPLEMENTATION
#include <flag.h>
#include <yuarel.h>

#include <hijack_header.h>
#include <proxy.h>

static void print_usage(FILE *stream);
static void print_info(FILE *stream);
static int create_server(int port);
static bool handle_server_epoll(int server_sock, struct sockaddr_in upstream, const char *range);
static int epoll_add(int epfd, int sock, uint32_t events);
static int epoll_del(int epfd, int sock);
static bool setup_socket(int epfd, int sock);
static int setnonblocking(int fd);
static void handle_tunnel(Socket *ffmpeg_socket, Socket *upstream_socket, int ready_sock, uint32_t events);

static void print_usage(FILE *stream)
{
    print_info(stream);
    fprintf(stream, "Usage: %s [OPTIONS] URL\n", PROGRAM_NAME);
    fprintf(stream, "OPTIONS:\n");
    flag_print_options(stream);
}

static void print_info(FILE *stream)
{
    fprintf(stream, " .__ .___. __..__   .__ .        .__             \n"
                    " [__)  |  (__ [__)  [__)| _.  .  [__)._. _ \\./  .\n"
                    " |  \\  |  .__)|     |   |(_]\\_|  |   [  (_)/'\\\\_|\n"
                    "                            ._|               ._|\n");
    fprintf(stream, "%s v%s (built for %s %s)\n", PROGRAM_NAME, VERSION, OS, ARCH);
    fprintf(stream, "GitHub: https://github.com/shadowy-pycoder/rtsp-play-proxy\n\n");
}

static int create_server(int port)
{
    int server_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (server_sock < 0) {
        printf("ERROR: Creating socket failed: %s\n", strerror(errno));
        return -1;
    }
    int enable = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&enable, sizeof(enable)) < 0) {
        printf("ERROR: Setting SO_REUSEADDR option failed: %s\n", strerror(errno));
        return -1;
    }

    int send_buffer_size = SND_BUF_SIZE;
    int recv_buffer_size = RECV_BUF_SIZE;
    if (setsockopt(server_sock, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) < 0) {
        printf("ERROR: Setting SO_SNDBUF option failed: %s\n", strerror(errno));
        return -1;
    }
    if (setsockopt(server_sock, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) < 0) {
        printf("ERROR: Setting SO_RCVBUF option failed: %s\n", strerror(errno));
        return -1;
    }
    struct sockaddr_in server_addr = { 0 };
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_port = htons(port);
    server_addr.sin_family = AF_INET;
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("ERROR: Binding to address failed: %s\n", strerror(errno));
        return -1;
    }
    if (listen(server_sock, MAX_CONNECTIONS) < 0) {
        printf("ERROR: Listening on address failed: %s\n", strerror(errno));
        return -1;
    }
    return server_sock;
}

static int epoll_add(int epfd, int sock, uint32_t events)
{
    struct epoll_event ev;
    ev.data.fd = sock;
    ev.events = events;
    return epoll_ctl(epfd, EPOLL_CTL_ADD, sock, &ev);
}

static int epoll_del(int epfd, int sock)
{
    return epoll_ctl(epfd, EPOLL_CTL_DEL, sock, NULL);
}

static bool setup_socket(int epfd, int sock)
{
    int send_buffer_size = SND_BUF_SIZE;
    int recv_buffer_size = RECV_BUF_SIZE;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size)) < 0) {
        printf("ERROR: Setting SO_SNDBUF option for socket failed: %s\n", strerror(errno));
        return false;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size)) < 0) {
        printf("ERROR: Setting SO_RCVBUF option for socket failed: %s\n", strerror(errno));
        return false;
    }
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char *)&enable, sizeof(enable)) < 0) {
        printf("ERROR: Setting SOL_SOCKET option for socket failed: %s\n", strerror(errno));
        return false;
    }
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&enable, sizeof(enable)) < 0) {
        printf("ERROR: Setting TCP_NODELAY option for socket failed: %s\n", strerror(errno));
        return false;
    }
    int read_timeout = READ_TIMEOUT;
    int write_timeout = WRITE_TIMEOUT;

    if (read_timeout > 0) {
        struct timeval tv;
        tv.tv_sec = read_timeout;
        tv.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
            printf("ERROR: Setting SO_RCVTIMEO option for socket failed: %s", strerror(errno));
            return false;
        }
    }
    if (write_timeout > 0) {
        struct timeval tv;
        tv.tv_sec = write_timeout;
        tv.tv_usec = 0;
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv)) < 0) {
            printf("ERROR: Setting SO_SNDTIMEO option for socket failed: %s", strerror(errno));
            return false;
        }
    }
    if (epoll_add(epfd, sock, EPOLLIN | EPOLLRDHUP | EPOLLET) < 0) {
        printf("ERROR: Adding client socket to epoll failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}

static bool copy_stream(Socket *src, Socket *dst)
{
    unsigned long long int written = 0;
    while (true) {
        ssize_t nr = read(src->sock, src->buf.ptr + src->buf.size, src->buf.cap - src->buf.size);
        if (nr < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                break;
            }
            if (errno == EINTR) continue;
            printf("ERROR: Reading message failed: %s\n", strerror(errno));
            return false;
        } else if (nr == 0) {
#ifdef DEBUG
            printf("DEBUG: %s:EOF\n", src->addr);
#endif
            return false;
        } else {
            src->buf.size += (size_t)nr;
#ifdef DEBUG
            printf("DEBUG: Read %zd bytes from %s\n", nr, src->addr);
#endif
        }
        if (src->range_len > 0 && !hijack_play_header(src)) return true;
        ssize_t nw = write(dst->sock, src->buf.ptr + src->buf.offset, src->buf.size - src->buf.offset);
        if (nw < 0) {
            printf("ERROR: Writing message failed: %s\n", strerror(errno));
            break;
        } else if (nw == 0) {
            break;
        } else {
            src->buf.offset += (size_t)nw;
            if (src->buf.offset >= src->buf.size) {
                src->buf.size = 0;
                src->buf.offset = 0;
            }
            written += nw;
#ifdef DEBUG
            printf("DEBUG: Written %zd bytes to %s\n", nw, dst->addr);
#endif
        }
    }
    if (written)
        printf("INFO: Copied %llu bytes %s -> %s\n", written, src->addr, dst->addr);
    return true;
}

static void handle_tunnel(Socket *ffmpeg_socket, Socket *upstream_socket, int ready_sock, uint32_t events)
{
    Socket *src_sock, *dst_sock;
    if (ready_sock == upstream_socket->sock) {
        src_sock = upstream_socket;
        dst_sock = ffmpeg_socket;
    } else {
        src_sock = ffmpeg_socket;
        dst_sock = upstream_socket;
    }
    if ((events & EPOLLHUP) || (events & EPOLLERR)) {
        src_sock->closed = true;
    } else if (events & EPOLLRDHUP) {
        if (shutdown(src_sock->sock, SHUT_WR) < 0) {
            if (errno != ENOTCONN)
                printf("ERROR: Shutting down failed: %s\n", strerror(errno));
        }
        copy_stream(src_sock, dst_sock);
        src_sock->closed = true;
    } else if (events & EPOLLIN) {
        if (!copy_stream(src_sock, dst_sock) || dst_sock->closed) {
            if (shutdown(src_sock->sock, SHUT_WR) < 0) {
                if (errno != ENOTCONN)
                    printf("ERROR: Shutting down failed: %s\n", strerror(errno));
            }
            src_sock->closed = true;
        }
    }
    return;
}

static bool handle_server_epoll(int server_sock, struct sockaddr_in upstream, const char *range)
{
    struct epoll_event events[MAX_EVENTS] = { 0 };

    int epfd = epoll_create1(0);
    if (epfd < 0) {
        printf("ERROR: Creating epoll file descriptor failed %s\n", strerror(errno));
        close(server_sock);
        return false;
    }
    int upstream_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (upstream_sock < 0) {
        printf("ERROR: Creating socket failed: %s\n", strerror(errno));
        close(upstream_sock);
        close(server_sock);
        return false;
    }
    if (!setup_socket(epfd, upstream_sock)) {
        close(upstream_sock);
        close(server_sock);
        return false;
    }
    if (connect(upstream_sock, (struct sockaddr *)&upstream, (socklen_t)sizeof(upstream)) < 0) {
        printf("ERROR: Connection to upstream failed %s\n", strerror(errno));
        close(upstream_sock);
        close(server_sock);
        return false;
    }
    setnonblocking(upstream_sock);
    Socket upstream_socket = (Socket) { .sock = upstream_sock, .buf = (Buffer) { .size = 0, .offset = 0, .cap = BUF_SIZE }, .closed = false, .range_len = 0 };
    char *temp_ip = inet_ntoa(upstream.sin_addr);
    memset(upstream_socket.addr, 0, ADDR_SIZE);
    snprintf(upstream_socket.addr, sizeof(upstream_socket.addr) - 1, "%s:%d", temp_ip, ntohs(upstream.sin_port));
    struct sockaddr_in client_addr = { 0 };
    client_addr.sin_family = AF_INET;
    socklen_t addr_len = sizeof(client_addr);
    int ffmpeg_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
    if (ffmpeg_sock < 0) {
        printf("ERROR: Accept connection failed: %s\n", strerror(errno));
        close(upstream_sock);
        close(server_sock);
        return false;
    }
    if (!setup_socket(epfd, ffmpeg_sock)) {
        close(upstream_sock);
        close(ffmpeg_sock);
        close(server_sock);
        return false;
    }
    setnonblocking(ffmpeg_sock);
    Socket ffmpeg_socket = (Socket) {
        .sock = ffmpeg_sock,
        .buf = (Buffer) { .size = 0, .offset = 0, .cap = BUF_SIZE },
        .closed = false,
        .range = range,
        .range_len = strlen(range)
    };
    temp_ip = inet_ntoa(client_addr.sin_addr);
    memset(ffmpeg_socket.addr, 0, ADDR_SIZE);
    snprintf(ffmpeg_socket.addr, sizeof(ffmpeg_socket.addr) - 1, "%s:%d", temp_ip, ntohs(client_addr.sin_port));
    int nready;
    while (true) {
        nready = epoll_wait(epfd, events, MAX_EVENTS, EPOLL_TIMEOUT);
        if (nready < 0) {
            if (errno == EINTR) continue;
            printf("ERROR: Waiting for epoll failed: %s\n", strerror(errno));
            goto failed;
        }
        if (nready == 0) goto success;
        for (int i = 0; i < nready; i++) {
            if (events[i].events == 0) continue;
            int sock = events[i].data.fd;
            if (ffmpeg_socket.closed && upstream_socket.closed) {
                if (epoll_del(epfd, ffmpeg_socket.sock) < 0) {
                    printf("ERROR: Removing ffmpeg socket from epoll failed: %s\n", strerror(errno));
                }
                if (epoll_del(epfd, upstream_socket.sock) < 0) {
                    printf("ERROR: Removing upstream socket from epoll failed: %s\n", strerror(errno));
                }
                goto success;
            }
            handle_tunnel(&ffmpeg_socket, &upstream_socket, sock, events[i].events);
        }
    }
success:
    close(upstream_sock);
    close(ffmpeg_sock);
    close(server_sock);
    return true;
failed:
    close(upstream_sock);
    close(ffmpeg_sock);
    close(server_sock);
    return false;
}

static int setnonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main(int argc, char **argv)
{
    bool *help = flag_bool("h", false, "Print this help message and exit");
    char **range = flag_str("R", "", "Time format for Range header (Example: \"clock=20261013T140000Z-\", \"npt=123.45-125\")");
    char **user_agent = flag_str("U", "", "Set custom User-Agent header");
    char **out = flag_str("o", "out.mp4", "Name of the output file");
    uint64_t *duration = flag_uint64("d", 0, "Duration of the output file in seconds");
    uint64_t *port = flag_uint64("p", 0, "Run proxy on specific port (Default: assigned by kernel)");
    if (!flag_parse(argc, argv)) {
        print_usage(stderr);
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        flag_print_error(stderr);
        return EXIT_FAILURE;
    }

    if (*help) {
        print_usage(stdout);
        return EXIT_SUCCESS;
    }

    argc = flag_rest_argc();
    argv = flag_rest_argv();
    if (argc == 0) {
        print_usage(stderr);
        fprintf(stderr, "ERROR: URL is missing\n");
        return EXIT_FAILURE;
    }
    if (argc > 1) {
        print_usage(stderr);
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: too many arguments provided\n");
        return EXIT_FAILURE;
    }
    char range_value[RANGE_SIZE] = { 0 };
    if (*range[0] != '\0') {
        snprintf(range_value, sizeof(range_value) - 1, "Range: %s\r\n", *range);
    }
    char *rtsp_url = argv[0];
    struct yuarel url = { 0 };

    if (yuarel_parse(&url, rtsp_url) < 0 || memcmp(url.scheme, "rtsp", 4) != 0) {
        print_usage(stderr);
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: Could not parse url\n");
        return EXIT_FAILURE;
    }

    if (*port != 0 && *port > UINT16_MAX) {
        print_usage(stderr);
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: Port is out of range\n");
        return EXIT_FAILURE;
    }
    int server_sock = create_server(*port);
    if (server_sock < 0) {
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: Could not start server\n");
        return EXIT_FAILURE;
    }

    int server_port;
    if (*port != 0) {
        server_port = *port;
    } else {
        struct sockaddr_in server_addr = { 0 };
        socklen_t server_addr_len = (socklen_t)sizeof(server_addr);
        if (getsockname(server_sock, (struct sockaddr *)&server_addr, &server_addr_len) < 0) {
            fprintf(stderr, "%s: ", PROGRAM_NAME);
            fprintf(stderr, "ERROR: getsockname failed: %s\n", strerror(errno));
            close(server_sock);
            return EXIT_FAILURE;
        }
        server_port = htons(server_addr.sin_port);
    }
    fprintf(stdout, "%s: ", PROGRAM_NAME);
    fprintf(stdout, "INFO: local server listening on 127.0.0.1:%d\n", server_port);

    char cmd[CMD_MAX_LEN] = { 0 };
    int n = snprintf(cmd, sizeof(cmd) - 1, "ffmpeg -y -nostats -rtsp_transport tcp -timeout 5000000");
    if (*user_agent[0] != '\0') {
        n += snprintf(cmd + n, sizeof(cmd) - 1 - n, " -user_agent \"%s\"", *user_agent);
    }
    n += snprintf(cmd + n, sizeof(cmd) - 1 - n, " -i \"rtsp://");
    if (url.username != NULL && url.password != NULL && *url.username != '\0' && *url.password != '\0')
        n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "%s:%s@", url.username, url.password);
    n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "127.0.0.1:%d", server_port);
    if (url.path != NULL && *url.path != '\0') n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "/%s", url.path);
    if (url.query != NULL && *url.query != '\0') n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "?%s", url.query);
    if (url.fragment != NULL && *url.fragment != '\0') n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "#%s", url.fragment);
    n += snprintf(cmd + n, sizeof(cmd) - 1 - n, "\"");
    n += snprintf(cmd + n, sizeof(cmd) - 1 - n, " -map 0 -c copy");
    if (*duration > 0) n += snprintf(cmd + n, sizeof(cmd) - 1 - n, " -t %lu", *duration);
    n += snprintf(cmd + n, sizeof(cmd) - 1 - n, " %s", *out);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: starting process failed: %s\n", strerror(errno));
        close(server_sock);
        return EXIT_FAILURE;
    }
    if (pid == 0) {
        fprintf(stdout, "%s: ", PROGRAM_NAME);
        fprintf(stdout, "INFO: %s\n", cmd);
        execlp("/bin/sh", "sh", "-c", cmd, (char *)NULL);
    }
    struct sockaddr_in upstream = { 0 };
    struct in_addr upstream_addr = { 0 };
    if (inet_pton(AF_INET, url.host, &upstream_addr) != 1) {
        fprintf(stderr, "%s: ", PROGRAM_NAME);
        fprintf(stderr, "ERROR: failed extracting upstream IP\n");
        close(server_sock);
        return EXIT_FAILURE;
    }
    upstream.sin_addr = upstream_addr;
    upstream.sin_port = htons(url.port ? url.port : 554);
    upstream.sin_family = AF_INET;
    handle_server_epoll(server_sock, upstream, range_value);
    waitpid(pid, NULL, 0);
    fprintf(stdout, "%s: ", PROGRAM_NAME);
    fprintf(stdout, "INFO: Proxy stopped\n");
    return EXIT_SUCCESS;
}
