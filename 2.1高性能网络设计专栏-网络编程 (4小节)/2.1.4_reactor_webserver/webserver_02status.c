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
    c->status = 0;
}

int http_response(struct conn *c)
{
    int filefd = open(WEBSERVER_ROOTDIR "index.html", O_RDONLY);
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

    // 业务层
    //  生成http_response响应报文（分步发送 head+body）  注意这里没有发送
    if (c->status == 0)
    {
        c->wlength = sprintf(c->wbuffer,
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "Accept-Ranges: bytes\r\n"
                             "Content-Length: %ld\r\n"
                             "Date: Mon, 04 Aug 2025 17:48:00 GMT\r\n\r\n",
                             stat_buf.st_size);
        // text/html    表示文本数据/格式是html
        // 最后的 \r\n，表示头部结束，后面是body
        c->status = 1;
    }
    else if (c->status == 1)
    {
        // 发送body(这里是一个html文件)
        int ret = sendfile(c->fd, filefd, NULL, stat_buf.st_size); // 零拷贝技术
        if (ret == -1)
        {
            INFO("errno: %d\n", errno);
        }
        c->status = 2;
    }
    else if (c->status == 2)
    {
        // 清理状态，重置为 0（准备下一次发来的请求）
        memset(c->wbuffer, 0, BUFFER_LENGTH);
        c->wlength = 0;
        c->status = 0;
    }

    close(filefd);

    return c->wlength;

    /*
    通过状态机分多步返回大文件   0 → 1 → 2 → 0
    status=0：发送 HTTP 头；
    status=1：调用 sendfile() 直接把文件内容传给客户端；
    status=2：清理状态，重置为 0
    */
}

/*
当运行./webserver, 浏览器输入192.168.65.131：2000时候，服务端收到（打印）2份相同的请求。
（注意：在http_request中,"Connection: keep-alive\r\n"的前提下）

原因是触发了2次HTTP请求
第一次请求：用户按下在浏览器输入，按下回车主动请求
第二次请求：浏览器自动请求网站图标
 * 当访问任何网页时，浏览器会自动向网站根目录发起 GET /favicon.ico 请求
从网站的根目录（如 http://baidu.com/favicon.ico）加载 favicon.ico 文件，即使网页的 HTML 代码中没有显式添加以下标签:
<link rel="icon" href="favicon.ico" type="image/x-icon">
*/
