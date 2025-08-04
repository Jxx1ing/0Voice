#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <errno.h>
#include "server.h"

int http_request(struct conn *c)
{
    // ����http_requestʵ���ϲ���������ֻ�ǳ�ʼ��
    memset(c->wbuffer, 0, BUFFER_LENGTH);
    c->wlength = 0;
}

int http_response(struct conn *c)
{

    int filefd = open("../http_response_Resource/index.html", O_RDONLY);

    struct stat stat_buf;
    fstat(filefd, &stat_buf);

    c->wlength = sprintf(c->wbuffer,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html\r\n"
                         "Accept-Ranges: bytes\r\n"
                         "Content-Length: %ld\r\n"
                         "Date: Tue, 30 Apr 2024 13:16:46 GMT\r\n\r\n",
                         stat_buf.st_size);

    // ��ȡ�ļ�����ƴ�ӵ���Ӧ��(�� HTML �ļ����ݶ��� c->wbuffer �ƴ������Ӧͷ���棨c->wbuffer + c->wlength��)
    int count = read(filefd, c->wbuffer + c->wlength, BUFFER_LENGTH - c->wlength); // BUFFER_LENGTH - c->wlengthΪ�˲�Խ��
    c->wlength += count;
    /*
    ȱ�㣺ֻ�����ڷ���С�ļ�������һ�η��Ͳ���Ĵ��ļ���δʹ��woffset��¼�����˶��٣���ʣ�¶��١�
    ����ʹ��sendfile(������Ӧ�ļ��Ĵ�С��������һ��buffer��С)
    */

    /*
        Ϊʲô����汾�Ļ����ϣ�Ҫ�ĳ�״̬�����ô���ʲô����
        ������û�������ϸ�ķֲ�
1. I/O ��ҵ���߼���ϵñȽϽ�
        ��������send_cb ��ֱ�ӵ����� http_response(&conn_list[fd]) ���������䷢�����ݣ�
        ����ζ��ҵ���߼���HTTPЭ����صı������ɣ��� I/O ������Ϊ������һ��û���Է��룩    -- �ص�
2. ״̬������ϸ��
        �������״ֻ̬�Ǽ������л�һ�η��ͣ�
        û���������֡�����ͷ�����������塱����������ɡ��ȶ�׶Σ�û������״̬����ȷ����ÿ���׶ε����̡�
    */

    close(filefd);
    return c->wlength;
}

/*
���󣺷����ֻ�յ�1��get����û��favicon get����
http_request��֧�� keep-alive   -- "Connection: keep-alive\r\n"
��һ����Ӧ���Ӻ�ر����ӣ��ڶ��ε�favicon get�����޷����ͣ������Ѿ����ˣ�
*/