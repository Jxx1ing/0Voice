#include <stdio.h>      // printf、perror
#include <string.h>     // memset、strcpy、strlen 等
#include <stdlib.h>     // atoi、malloc、free 等
#include <unistd.h>     // close、read、write
#include <arpa/inet.h>  // sockaddr_in、inet_pton、htons 等
#include <netinet/in.h> // INADDR_ANY 等
#include <pthread.h>    // pthread_create 用于多线程
#include <errno.h>      // errno
#include <fcntl.h>

#define INFO printf
#define BUFFER_SIZE 1024

// 线程的任务：接收客户端发送的消息
void *client_handler(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg); // 注意释放堆内存
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(client_fd, buffer, sizeof(buffer), 0);
        if (len <= 0) // 默认情况下recv是阻塞的。只要连接还在，recv() 会一直阻塞，不会返回 <= 0
        {
            INFO("Client disconnected.\n"); // 客户端主动断开连接会打印
            break;
        }
        INFO("Received from client: %s, The bytes: %d\n", buffer, len);
        // 这里服务端也可以回传给客户端消息，比如：send(client_fd, buffer, len, 0);
    }
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    // 第一个参数是执行文件，第二个参数是端口号
    if (argc < 2)
    {
        INFO("PARAM ERROR.\n");
        return -1;
    }
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket failed");
        return -2;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1])); // agrv[1]是端口号
    server_addr.sin_addr.s_addr = INADDR_ANY;    // 服务器会在所有可用接口上监听指定端口的连接
    // 0.0.0.0（IPv4 地址），表示“任意地址”

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        return -3;
    }

    if (listen(server_fd, 5) < 0) // 5 是队列长度 服务端接收客户端尝试连接的最大数量
    {
        perror("listen failed");
        return -4;
    }

    INFO("Server listening on port ...\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *client_fd = malloc(sizeof(int)); // 注意这里用堆空间，避免线程冲突
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*client_fd < 0)
        {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, client_fd);
        pthread_detach(tid); // 线程在终止时能够自动释放其资源
    }
}
