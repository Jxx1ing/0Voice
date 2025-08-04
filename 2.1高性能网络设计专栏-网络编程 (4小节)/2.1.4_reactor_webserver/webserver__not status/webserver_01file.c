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
    // 这里http_request实质上不发送请求，只是初始化
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

    // 读取文件内容拼接到响应体(把 HTML 文件内容读进 c->wbuffer 里，拼接在响应头后面（c->wbuffer + c->wlength）)
    int count = read(filefd, c->wbuffer + c->wlength, BUFFER_LENGTH - c->wlength); // BUFFER_LENGTH - c->wlength为了不越界
    c->wlength += count;
    /*
    缺点：只适用于发送小文件，对于一次发送不完的大文件，未使用woffset记录发送了多少，还剩下多少。
    后续使用sendfile(拷贝响应文件的大小，而不是一个buffer大小)
    */

    /*
        为什么这个版本的基础上，要改成状态机（好处是什么）？
        整体上没有做到严格的分层
1. I/O 和业务逻辑耦合得比较紧
        这版代码中send_cb 里直接调用了 http_response(&conn_list[fd]) 来构造和填充发送内容，
        这意味着业务逻辑（HTTP协议相关的报文生成）和 I/O 发送行为混杂在一起，没明显分离）    -- 重点
2. 状态管理不够细致
        这版代码的状态只是简单用来切换一次发送，
        没有明显区分“发送头”、“发送体”、“发送完成”等多阶段，没有利用状态机明确管理每个阶段的流程。
    */

    close(filefd);
    return c->wlength;
}

/*
现象：服务端只收到1个get请求，没有favicon get请求
http_request不支持 keep-alive   -- "Connection: keep-alive\r\n"
第一次响应连接后关闭连接，第二次的favicon get请求无法发送（连接已经断了）
*/