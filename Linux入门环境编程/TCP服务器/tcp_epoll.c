#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 1024
#define INFO printf

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        INFO("param error.\n");
        return -1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket failed\n");
        return -2;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        INFO("bind failed.\n");
        return -2;
    }
    if (listen(listen_fd, 5) < 0)
    {
        INFO("listen failed.\n");
        return -3;
    }

    // 使用epoll监听——单线程（区别于tcp_onebyone.c版本）
    int epfd = epoll_create1(0); // 创建 epoll 实例
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev); // 添加监听fd到epoll(默认是边缘触发)
    INFO("Epoll server listening ...\n");
    struct epoll_event events[MAX_EVENTS] = {0};
    while (1)
    {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 5);
        for (int i = 0; i < nfds; ++i)
        {
            int fd = events[i].data.fd;
            if (fd == listen_fd)
            {
                // 情况1-某个事件的文件描述符是监听套接字，就可以确定有新连接到达
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);
                // 新连接创建后，采用边缘触发 + 非阻塞的方式读取客户端的数据
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev); // epoll_ctl 都是把当前 ev 的内容拷贝到内核
            }
            else
            {
                // 情况2-读取新连接中客户端发送的数据
                int client_fd = events[i].data.fd;
                // 使用while循环读取客户端发送的数据，直到读取完
                while (1)
                {
                    char buffer[BUFFER_SIZE] = {0};
                    int len = recv(client_fd, buffer, BUFFER_SIZE, 0);
                    if (len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        // 没有更多数据，等下一次触发
                        break;
                    }
                    else if (len == 0)
                    {
                        // 客户端断开，关闭连接
                        close(client_fd);
                        ev.events = EPOLLIN;
                        ev.data.fd = client_fd;
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, &ev);
                        break;
                        // 如果不break,断开连接后由于while循环会出现访问已经关闭的 fd 的行为
                        // 现象就是不断打印Recv from ...-1 byte.
                    }
                    else
                    {
                        INFO("Recv from %d id: %s, %d byte(s)\n", client_fd, buffer, len);
                    }
                }
            }
        }
    }
    return 0;
}

// 事件驱动模型： 单线程 + epoll 边缘触发 + 非阻塞
/* 注意与tcp.onebyone代码中一个请求一个线程pthread_create的区别：
单线程 epoll 服务器是 顺序执行，不是多线程的并发执行。
程序在一个线程里，依次处理每个有事件的客户端套接字。
*/