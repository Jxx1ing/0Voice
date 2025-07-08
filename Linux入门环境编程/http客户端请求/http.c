#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>
#include <string.h>
#include <stdlib.h>

#define INFO printf
#define PORT 80
#define BUFFER_SZIE 4096

// 将域名转换为 IP 地址（DNS 查询）
char *host_to_ip(const char *hostname)
{
    struct hostent *host = gethostbyname(hostname);
    if (NULL == host)
    {
        INFO("gethostbyname failed.\n");
        return NULL;
    }
    char *ret = inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
    if (NULL == ret)
    {
        INFO("inet_ntoa failed.\n");
        return NULL;
    }
    return ret;
}

// 创建 TCP socket，并连接到目标 IP 的 80 端口
int http_create_socket(char *ip)
{
    // 1- 创建 socket（套接字），用于网络通信的端点。
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd)
    {
        INFO("socket failed.\n");
        return -1;
    }
    // 2- 客户端通过该函数连接到服务器。
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;            // IPv4
    server_addr.sin_port = htons(PORT);          // 端口号，需转为网络字节序
    server_addr.sin_addr.s_addr = inet_addr(ip); // 服务器 IP   将点分十进制的 IPv4 地址字符串（例如 `"192.amples") 转换为 32 位无符号整数（网络字节序）。
    if (-1 == connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        INFO("Connect failed.\n");
        return -2;
    }

    // 3- 设置 socket 的属性，例如将其设置为非阻塞模式。
    int ret = fcntl(sockfd, F_SETFL, O_NONBLOCK); // F_SETFL：设置文件描述符的标志 / O_NONBLOCK（非阻塞模式）
    if (-1 == ret)
    {
        INFO("fcntl failed.\n");
        return -3;
    }
    return sockfd;
}

// 构造并发送 HTTP GET 请求，接收并返回响应内容
char *http_send_request(const char *hostname, const char *resource)
{
    // 1-调用 host_to_ip 获取 IP
    char *ip = host_to_ip(hostname);
    if (NULL == ip)
    {
        INFO("http_send_request: host to ip failed.\n");
        return NULL;
    }
    // 2-调用 http_create_socket 建立连接
    int sockfd = http_create_socket(ip);
    if (sockfd < 0)
    {
        INFO("http_send_request: http_create_socket failed.\n");
        return NULL;
    }
    // 3-构造 GET 请求报文，send发送
    // 注意： 格式化字符串被分成多行，每行是一个独立的字符串字面值，各自用双引号括起来
    char buffer[BUFFER_SZIE] = {0};
    sprintf(buffer,
            "GET %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            resource, hostname);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0)
    {
        INFO("http_send_request: send failed.\n");
        close(sockfd);
        return NULL;
    }

    // 4-使用 select 检查 socket 是否可读
    fd_set read_fds; // select 文件描述符集
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    struct timeval timeout; // 超时设置设置超时为 5 秒
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    char *result = (char *)malloc(sizeof(int)); // 存储http响应
    memset(result, 0, sizeof(int));
    while (1)
    {
        // 使用 select 检查 socket 是否可读
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret <= 0)
        {
            INFO("select error.\n");
            break;
        }
        // 5-使用 recv() 不断接收数据，拼接成完整字符串返回
        if (FD_ISSET(sockfd, &read_fds))
        {
            memset(buffer, 0, sizeof(buffer));
            int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
            if (bytes_received <= 0)
            {
                INFO("recv error.\n");
                break;
            }
            result = realloc(result, (strlen(result) + bytes_received + 1) * sizeof(char));
            strncat(result, buffer, bytes_received); // strncat原字符串的末尾追加数据
        }
    }
    close(sockfd);
    return result;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
        return -1;
    char *response = http_send_request(argv[1], argv[2]);
    INFO("http response: \n%s", response);
    return 0;
}
