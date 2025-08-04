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
    // 这里http_request实质上不发送请求，只是初始化
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
现象：服务端只收到1个get请求，没有favicon get请求
http_request不支持 keep-alive   -- "Connection: keep-alive\r\n"
第一次响应连接后关闭连接，第二次的favicon get请求无法发送（连接已经断了）
*/