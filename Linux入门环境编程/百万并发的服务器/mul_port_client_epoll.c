#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h>

#define MAX_BUFFER 128
#define MAX_EPOLLSIZE (384 * 1024)
#define MAX_PORT 100
// 计算两个 timeval 结构之间的毫秒差（tv1 - tv2）。
#define TIME_SUB_MS(tv1, tv2) ((tv1.tv_sec - tv2.tv_sec) * 1000 + (tv1.tv_usec - tv2.tv_usec) / 1000)
// 客户端是否继续建立连接、处理事件等。通过服务器返回 "quit" 字符串控制终止。
int isContinue = 0;

static int ntySetNonblock(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return flags;
    flags |= O_NONBLOCK; // 设置非阻塞。防止recv() 阻塞线程
    if (fcntl(fd, F_SETFL, flags) < 0)
        return -1;
    return 0;
}

// 当 socket 关闭后，该端口可以立即再次绑定（否则 TCP 的 TIME_WAIT 状态会导致端口暂时不能复用）
static int ntySetReUseAddr(int fd)
{
    int reuse = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
}

int main(int argc, char **argv)
{
    if (argc <= 2)
    {
        printf("Usage: %s ip port\n", argv[0]);
        exit(0);
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int connections = 0; // 当前已建立的 socket 连接数
    char buffer[128] = {0};
    int i = 0, index = 0;

    struct epoll_event events[MAX_EPOLLSIZE];

    int epoll_fd = epoll_create(MAX_EPOLLSIZE);

    strcpy(buffer, " Data From MulClient\n");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);

    struct timeval tv_begin;
    gettimeofday(&tv_begin, NULL);

    while (1)
    {
        // 循环从 port+0 到 port+99 建立连接（控制连接分布到多个端口）
        if (++index >= MAX_PORT)
            index = 0;

        struct epoll_event ev;
        int sockfd = 0;

        // 340000 是一个性能上限测试值（比如压测），控制连接数上限
        if (connections < 340000 && !isContinue)
        {
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1)
            {
                perror("socket");
                goto err;
            }

            addr.sin_port = htons(port + index);

            if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
            {
                perror("connect");
                goto err;
            }
            ntySetNonblock(sockfd);
            ntySetReUseAddr(sockfd);

            sprintf(buffer, "Hello Server: client --> %d\n", connections);
            send(sockfd, buffer, strlen(buffer), 0);

            ev.data.fd = sockfd;
            ev.events = EPOLLIN | EPOLLOUT; // 注册 EPOLLIN（可读） 和 EPOLLOUT（可写）事件到 epoll
            epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &ev);

            connections++;
        }
        // 每 1000 个连接或达到上限后，进行 epoll 处理
        if (connections % 1000 == 999 || connections >= 340000)
        {
            struct timeval tv_cur;
            memcpy(&tv_cur, &tv_begin, sizeof(struct timeval));

            gettimeofday(&tv_begin, NULL);

            int time_used = TIME_SUB_MS(tv_begin, tv_cur);
            printf("connections: %d, sockfd:%d, time_used:%d\n", connections, sockfd, time_used);

            int nfds = epoll_wait(epoll_fd, events, connections, 100);
            for (i = 0; i < nfds; i++)
            {
                int clientfd = events[i].data.fd;

                if (events[i].events & EPOLLOUT) // 表示 socket 可写，继续发数据。
                {
                    sprintf(buffer, "data from %d\n", clientfd);
                    send(clientfd, buffer, strlen(buffer), 0);
                }
                else if (events[i].events & EPOLLIN) // 表示 socket有数据可读
                {
                    char rBuffer[MAX_BUFFER] = {0};
                    ssize_t length = recv(clientfd, rBuffer, MAX_BUFFER, 0);
                    if (length > 0)
                    {
                        printf(" RecvBuffer:%s\n", rBuffer);

                        // 若收到 "quit"，设置 isContinue = 0，中断后续连接
                        if (!strcmp(rBuffer, "quit"))
                        {
                            isContinue = 0;
                        }
                    }
                    // 服务端关闭连接
                    else if (length == 0)
                    {
                        printf(" Disconnect clientfd:%d\n", clientfd);
                        connections--;
                        close(clientfd);
                    }
                    // 错误发生，若不是 EINTR 则关闭连接
                    else
                    {
                        // 如果错误码 不是 EINTR（表示被信号中断，不算致命错误）
                        if (errno == EINTR)
                            continue;
                        // 如果是致命错误，关闭连接
                        printf(" Error clientfd:%d, errno:%d\n", clientfd, errno);
                        close(clientfd);
                    }
                }
                else
                {
                    printf(" clientfd:%d, errno:%d\n", clientfd, errno);
                    close(clientfd);
                }
            }
        }
        // 休眠以降低 CPU 占用
        usleep(1 * 1000);
    }

    return 0;

err:
    printf("error : %s\n", strerror(errno));
    return 0;
}
