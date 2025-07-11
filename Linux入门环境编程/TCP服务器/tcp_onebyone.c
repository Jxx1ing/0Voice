#include <stdio.h>      // printf��perror
#include <string.h>     // memset��strcpy��strlen ��
#include <stdlib.h>     // atoi��malloc��free ��
#include <unistd.h>     // close��read��write
#include <arpa/inet.h>  // sockaddr_in��inet_pton��htons ��
#include <netinet/in.h> // INADDR_ANY ��
#include <pthread.h>    // pthread_create ���ڶ��߳�
#include <errno.h>      // errno
#include <fcntl.h>

#define INFO printf
#define BUFFER_SIZE 1024

// �̵߳����񣺽��տͻ��˷��͵���Ϣ
void *client_handler(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg); // ע���ͷŶ��ڴ�
    char buffer[BUFFER_SIZE];
    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        int len = recv(client_fd, buffer, sizeof(buffer), 0);
        if (len <= 0) // Ĭ�������recv�������ġ�ֻҪ���ӻ��ڣ�recv() ��һֱ���������᷵�� <= 0
        {
            INFO("Client disconnected.\n"); // �ͻ��������Ͽ����ӻ��ӡ
            break;
        }
        INFO("Received from client: %s, The bytes: %d\n", buffer, len);
        // ��������Ҳ���Իش����ͻ�����Ϣ�����磺send(client_fd, buffer, len, 0);
    }
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    // ��һ��������ִ���ļ����ڶ��������Ƕ˿ں�
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
    server_addr.sin_port = htons(atoi(argv[1])); // agrv[1]�Ƕ˿ں�
    server_addr.sin_addr.s_addr = INADDR_ANY;    // �������������п��ýӿ��ϼ���ָ���˿ڵ�����
    // 0.0.0.0��IPv4 ��ַ������ʾ�������ַ��

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind failed");
        return -3;
    }

    if (listen(server_fd, 5) < 0) // 5 �Ƕ��г��� ����˽��տͻ��˳������ӵ��������
    {
        perror("listen failed");
        return -4;
    }

    INFO("Server listening on port ...\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int *client_fd = malloc(sizeof(int)); // ע�������öѿռ䣬�����̳߳�ͻ
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*client_fd < 0)
        {
            perror("accept failed");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, client_handler, client_fd);
        pthread_detach(tid); // �߳�����ֹʱ�ܹ��Զ��ͷ�����Դ
    }
}
