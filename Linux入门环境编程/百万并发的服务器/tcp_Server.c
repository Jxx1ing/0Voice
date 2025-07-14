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
#define MAX_PORT 100
#define INFO printf

// 是否是监听端口的套接字
int islistenfd(int fd, int *fds)
{

    int i = 0;
    for (i = 0; i < MAX_PORT; i++)
    {
        if (fd == *(fds + i))
            return fd; // 如果是监听端口的套接字，返回这个新连接的套接字
    }

    return 0; // 如果是客户端发送数据的套接字，返回0
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        INFO("param error.\n");
        return -1;
    }

    int port = atoi(argv[1]);      // 端口号，这里是8888~8897
    int sockfds[MAX_EVENTS] = {0}; // 元素是监听套接字。每个sockfd监听当前端口是否有客户端请求连接
    int epfd = epoll_create1(0);   // 创建 epoll 实例

    // 注意与08tcp_epoll.c的写法不同。上一版只监听一个端口号8888，这里需要监听多个端口号，每个套接字绑定一个端口
    for (int i = 0; i < MAX_EVENTS; i++)
    {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0)
        {
            perror("socket failed\n");
            return -2;
        }
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port + i);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        // 避免一个TCP连接关闭后，操作系统将该端口设置为TIME_WAIT状态，从而允许其他连接立即复用该端口
        int reuse = 1;
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));

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

        // INFO("tcp server listen on port : %d\n", port + i);

        struct epoll_event ev;
        ev.events = EPOLLIN; // 监听的事件类型：可读
        ev.data.fd = listen_fd;
        epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev); // 添加监听fd到epoll(默认是边缘触发)

        sockfds[i] = listen_fd;
    }

    // INFO("Epoll server listening ...\n");
    struct epoll_event events[MAX_EVENTS] = {0};
    while (1)
    {
        int nfds = epoll_wait(epfd, events, MAX_EVENTS, 5);
        for (int i = 0; i < nfds; ++i)
        {
            int fd = events[i].data.fd;
            int sockfd = islistenfd(fd, sockfds);
            // 情况1-某个事件的文件描述符是监听套接字，就可以确定有新连接到达
            if (sockfd)
            {
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);
                int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &len);

                // 新连接创建后，采用边缘触发 + 非阻塞的方式读取客户端的数据
                fcntl(client_fd, F_SETFL, O_NONBLOCK);
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev); // epoll_ctl 都是把当前 ev 的内容拷贝到内核
            }
            else
            {
                // 情况2-读取新连接中客户端发送的数据
                int client_fd = events[i].data.fd;
                char buffer[BUFFER_SIZE] = {0};
                while (1)
                {
                    int len = recv(client_fd, buffer, BUFFER_SIZE, 0);
                    if (len == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            // 已经读完数据，退出循环
                            break;
                        }
                        else if (errno == EINTR)
                        {
                            // 被信号中断，继续读取
                            continue;
                        }
                        else
                        {
                            // 真正错误，关闭连接
                            close(client_fd);
                            epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                            break;
                        }
                    }
                    else if (len == 0)
                    {
                        // 客户端关闭连接，清理
                        close(client_fd);
                        epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
                        break;
                    }
                    else
                    {
                        // 正常接收数据
                        INFO("Recv: %.*s, %d byte(s), clientfd: %d\n", len, buffer, len, client_fd);
                    }
                }
            }
        }
    }
    return 0;
}
