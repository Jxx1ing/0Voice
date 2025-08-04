#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <errno.h>
#include "server.h"

#define WEBSERVER_ROOTDIR "./http_response_Resource/"

int http_request(struct conn *c)
{
    // ����http_requestʵ���ϲ���������ֻ�ǳ�ʼ��
    memset(c->wbuffer, 0, BUFFER_LENGTH);
    c->wlength = 0;
    c->status = 0;
}

int http_response(struct conn *c)
{
    int filefd = open(WEBSERVER_ROOTDIR "nginx.png", O_RDONLY);
    if (filefd < 0)
    {
        INFO("Failed to open index.html\n");
        return -1;
    }
    struct stat stat_buf;
    if (fstat(filefd, &stat_buf) < 0)
    {
        INFO("Failed to fstat\n");
        close(filefd);
        return -2;
    }

    // ҵ���
    //  ����http_response��Ӧ���ģ��ֲ����� head+body��  ע������û�з���
    if (c->status == 0)
    {
        c->wlength = sprintf(c->wbuffer,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: image/png\r\n"
                             "Accept-Ranges: bytes\r\n"
                             "Content-Length: %ld\r\n"
                             "Date: Mon, 04 Aug 2025 17:48:00 GMT\r\n\r\n",
                             stat_buf.st_size);
        // image/png    ��ʾͼƬ
        // ���� \r\n����ʾͷ��������������body
        c->status = 1;
    }
    else if (c->status == 1)
    {
        // ����body(������һ��html�ļ�)
        int ret = sendfile(c->fd, filefd, NULL, stat_buf.st_size); // �㿽������
        if (ret == -1)
        {
            INFO("errno: %d\n", errno);
        }
        c->status = 2;
    }
    else if (c->status == 2)
    {
        // ����״̬������Ϊ 0��׼����һ�η���������
        memset(c->wbuffer, 0, BUFFER_LENGTH);
        c->wlength = 0;
        c->status = 0;
    }

    close(filefd);

    return c->wlength;

    /*
    ͨ��״̬���ֶಽ���ش��ļ�   0 �� 1 �� 2 �� 0
    status=0������ HTTP ͷ��
    status=1������ sendfile() ֱ�Ӱ��ļ����ݴ����ͻ��ˣ�
    status=2������״̬������Ϊ 0
    */
}

/*
������./webserver, ���������192.168.65.131��2000ʱ�򣬷�����յ�����ӡ��2����ͬ������ԭ���Ǵ�����2��HTTP����
��һ�������û���������������룬���»س���������
�ڶ�������������Զ�������վͼ��
 * �������κ���ҳʱ����������Զ�����վ��Ŀ¼���� GET /favicon.ico ����
����վ�ĸ�Ŀ¼���� http://baidu.com/favicon.ico������ favicon.ico �ļ�����ʹ��ҳ�� HTML ������û����ʽ������±�ǩ:
<link rel="icon" href="favicon.ico" type="image/x-icon">
*/
