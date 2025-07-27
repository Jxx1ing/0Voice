#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/epoll.h>

#define BUFFER_SIZE 1024

int main()
{

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
    servaddr.sin_port = htons(2000);              // 0-1023,

    if (-1 == bind(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)))
    {
        printf("bind failed: %s\n", strerror(errno));
    }

    listen(sockfd, 10);
    printf("listen finshed: %d\n", sockfd); // fd是3 监听描述符

    struct sockaddr_in clientaddr;
    socklen_t len = sizeof(clientaddr);

    // 使用poll进行网络监听
    struct pollfd fds[1024] = {0}; // 文件描述符，监听的事件（输入），实际发生的事件（输出，由内核填充）
    fds[sockfd].fd = sockfd;       // 将监听文件描述符(int类型)和数组索引对应起来,下面的clientfd也是同理
    fds[sockfd].events = POLLIN;   // 可读事件

    int maxfd = sockfd;

    while (1)
    {
        int nready = poll(fds, maxfd + 1, -1); // 数组, 数组元素数量, 超过时间(-1表示阻塞等待)

        if (fds[sockfd].revents & POLLIN)
        {
            int clientfd = accept(sockfd, (struct sockaddr *)&clientaddr, &len);
            printf("accept finshed: %d\n", clientfd);

            fds[clientfd].fd = clientfd;
            fds[clientfd].events = POLLIN;

            if (clientfd > maxfd) // 判断大小的原因是 clientfd可能被赋予之前回收的fd
                maxfd = clientfd;
        }

        int i = 0;
        for (int i = sockfd + 1; i <= maxfd; i++)
        {
            if ((fds[i].revents & POLLIN) != 0) // revents是一个位掩码（bitmask）
            {
                // 说明revents包含POLLIN这一位
                char buffer[BUFFER_SIZE] = {0};
                int count = recv(i, buffer, 1024, 0);
                if (count == 0)
                {
                    // disconnect
                    printf("client disconnect: %d\n", i);
                    close(i);
                    fds[i].fd = -1;
                    fds[i].events = 0;

                    continue; // 注意不是break。不退出循环，因为只是这个客户端断开连接，还需要监听其他客户端
                }
                printf("RECV: %s\n", buffer);
                count = send(i, buffer, count, 0);
                printf("SEND: %d\n", count);
            }
        }
    }
}