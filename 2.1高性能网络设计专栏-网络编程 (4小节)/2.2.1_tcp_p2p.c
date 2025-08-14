// tcp_p2p_chat.c
// Build: gcc -O2 -Wall -Wextra -o tcp_p2p_chat tcp_p2p_chat.c
// Usage: ./tcp_p2p_chat <local_ip> <local_port> <remote_ip> <remote_port>

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int set_nonblock(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    if (fl < 0)
        return -1;
    return fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static int set_nodelay(int fd)
{
    int on = 1;
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static int set_reuse(int fd)
{
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        return -1;
#ifdef SO_REUSEPORT
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
#endif
    return 0;
}

static const char *ts()
{
    static char buf[64];
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld",
             tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000);
    return buf;
}

static void perrorx(const char *tag) { fprintf(stderr, "[%s] %s: %s\n", ts(), tag, strerror(errno)); }

int main(int argc, char **argv)
{
    if (argc != 5)
    {
        fprintf(stderr, "Usage: %s <local_ip> <local_port> <remote_ip> <remote_port>\n", argv[0]);
        return 1;
    }

    const char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    const char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);

    signal(SIGINT, on_sigint);
    signal(SIGTERM, on_sigint);

    // --- 监听 socket
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0)
    {
        perrorx("socket(lfd)");
        return 2;
    }
    if (set_reuse(lfd) < 0)
        perrorx("setsockopt(SO_REUSE*)");

    struct sockaddr_in laddr;
    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_port = htons(local_port);
    if (inet_pton(AF_INET, local_ip, &laddr.sin_addr) != 1)
    {
        fprintf(stderr, "Invalid local_ip: %s\n", local_ip);
        return 2;
    }
    if (bind(lfd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perrorx("bind(lfd)");
        return 2;
    }
    if (listen(lfd, 16) < 0)
    {
        perrorx("listen(lfd)");
        return 2;
    }
    if (set_nonblock(lfd) < 0)
        perrorx("set_nonblock(lfd)");

    printf("[%s] Listening on %s:%d\n", ts(), local_ip, local_port);

    // --- 主动连接 socket
    int cfd = socket(AF_INET, SOCK_STREAM, 0);
    if (cfd < 0)
    {
        perrorx("socket(cfd)");
        close(lfd);
        return 2;
    }
    if (set_reuse(cfd) < 0)
        perrorx("setsockopt(SO_REUSE*)");
    if (bind(cfd, (struct sockaddr *)&laddr, sizeof(laddr)) < 0)
    {
        perrorx("bind(cfd)"); // 可忽略失败
    }
    if (set_nonblock(cfd) < 0)
        perrorx("set_nonblock(cfd)");

    struct sockaddr_in raddr;
    memset(&raddr, 0, sizeof(raddr));
    raddr.sin_family = AF_INET;
    raddr.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip, &raddr.sin_addr) != 1)
    {
        fprintf(stderr, "Invalid remote_ip: %s\n", remote_ip);
        close(lfd);
        close(cfd);
        return 2;
    }

    int conn_inflight = 0;
    if (connect(cfd, (struct sockaddr *)&raddr, sizeof(raddr)) == 0)
    {
        conn_inflight = 0;
        printf("[%s] Outgoing connect() succeeded immediately.\n", ts());
    }
    else
    {
        if (errno == EINPROGRESS)
        {
            conn_inflight = 1;
            printf("[%s] Outgoing connect() in progress to %s:%d ...\n",
                   ts(), remote_ip, remote_port);
        }
        else
        {
            perrorx("connect(cfd)");
            conn_inflight = 0;
        }
    }

    int established_fd = -1;
    int active_path = 0; // 1=accept, 2=connect

    // --- 等待 accept 或 connect 完成
    while (!g_stop && established_fd < 0)
    {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        int maxfd = -1;

        FD_SET(lfd, &rfds);
        if (lfd > maxfd)
            maxfd = lfd;
        if (conn_inflight)
        {
            FD_SET(cfd, &wfds);
            if (cfd > maxfd)
                maxfd = cfd;
        }

        struct timeval tv = {0, 200 * 1000};
        int n = select(maxfd + 1, &rfds, &wfds, NULL, &tv);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perrorx("select");
            break;
        }

        if (FD_ISSET(lfd, &rfds))
        {
            struct sockaddr_in peer;
            socklen_t plen = sizeof(peer);
            int afd = accept(lfd, (struct sockaddr *)&peer, &plen);
            if (afd >= 0)
            {
                char ip[64];
                inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
                printf("[%s] accept() from %s:%d\n", ts(), ip, ntohs(peer.sin_port));
                set_nodelay(afd);
                established_fd = afd;
                active_path = 1;
                break;
            }
        }

        if (conn_inflight && FD_ISSET(cfd, &wfds))
        {
            int err = 0;
            socklen_t len = sizeof(err);
            if (getsockopt(cfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
            {
                perrorx("getsockopt(SO_ERROR)");
                conn_inflight = 0;
            }
            else if (err == 0)
            {
                printf("[%s] Outgoing connect() completed to %s:%d\n",
                       ts(), remote_ip, remote_port);
                set_nodelay(cfd);
                established_fd = cfd;
                cfd = -1;
                active_path = 2;
                break;
            }
            else
            {
                fprintf(stderr, "[%s] connect failed: %s\n", ts(), strerror(err));
                conn_inflight = 0;
            }
        }
    }

    if (cfd >= 0)
        close(cfd);
    if (lfd >= 0)
        close(lfd);
    if (established_fd < 0)
    {
        fprintf(stderr, "P2P connection not established.\n");
        return 3;
    }

    printf("[%s] P2P established via %s path. You can now type messages.\n",
           ts(), (active_path == 1 ? "ACCEPT" : "CONNECT"));

    // --- 收发循环（stdin + socket）
    set_nonblock(established_fd);
    set_nonblock(STDIN_FILENO);

    char rbuf[4096];
    char sbuf[4096];
    while (!g_stop)
    {
        fd_set rfds, wfds;
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_SET(established_fd, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int maxfd = established_fd > STDIN_FILENO ? established_fd : STDIN_FILENO;

        struct timeval tv = {0, 200 * 1000};
        int n = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perrorx("select-loop");
            break;
        }

        // 接收对方消息
        if (FD_ISSET(established_fd, &rfds))
        {
            ssize_t m = recv(established_fd, rbuf, sizeof(rbuf) - 1, 0);
            if (m > 0)
            {
                rbuf[m] = '\0';
                printf("[%s] RECV: %s", ts(), rbuf);
            }
            else if (m == 0)
            {
                printf("[%s] Peer closed.\n", ts());
                break;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                perrorx("recv");
                break;
            }
        }

        // 读取 stdin 并发送
        if (FD_ISSET(STDIN_FILENO, &rfds))
        {
            ssize_t m = read(STDIN_FILENO, sbuf, sizeof(sbuf) - 1);
            if (m > 0)
            {
                sbuf[m] = '\0';
                send(established_fd, sbuf, m, 0);
            }
        }
    }

    close(established_fd);
    printf("[%s] Exit.\n", ts());
    return 0;
}
