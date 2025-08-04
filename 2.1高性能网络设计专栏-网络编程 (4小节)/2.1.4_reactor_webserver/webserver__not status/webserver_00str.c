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
}

int http_response(struct conn *c)
{
    c->wlength = sprintf(c->wbuffer,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/html\r\n"
                         "Accept-Ranges: bytes\r\n"
                         "Content-Length: 82\r\n"
                         "Date: Tue, 30 Apr 2024 13:16:46 GMT\r\n\r\n"
                         "<html><head><title>0voice.king</title></head><body><h1>King</h1></body></html>\r\n\r\n");
    return c->wlength;
}

/*
���󣺷����ֻ�յ�1��get����û��favicon get����
http_request��֧�� keep-alive   -- "Connection: keep-alive\r\n"
��һ����Ӧ���Ӻ�ر����ӣ��ڶ��ε�favicon get�����޷����ͣ������Ѿ����ˣ�
*/