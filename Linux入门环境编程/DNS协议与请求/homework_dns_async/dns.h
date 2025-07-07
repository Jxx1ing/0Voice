#ifndef DNS_H
#define DNS_H

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "dns.h"

#define DNS_QTYPE_A htons(1)
#define DNS_QCLASS_IN htons(1)
#define INFO printf
// 定义 DNS 查询类型：主机地址 (A 记录)
#define DNS_HOST 0x01
// 定义 DNS 查询类型：别名记录 (CNAME 记录)
#define DNS_CNAME 0x05
// 定义 DNS 服务器的端口号为 53，这是标准 DNS 协议端口
#define DNS_SERVER_PORT 53
// 定义 DNS 服务器的 IP 地址为 "114.114.114.114"，这是一个公共 DNS 服务器
#define DNS_SERVER_IP "114.114.114.114"

struct dns_header
{
    unsigned short id; // usigned char是1个字节
    unsigned short flags;
    unsigned short questions;
    unsigned short answer;
    unsigned short authority;
    unsigned short additional;
};

struct dns_question
{
    int length;
    unsigned short qtype;
    unsigned short qclass;
    unsigned char *name; // 长度+域名
};

struct dns_item
{
    char *domain; // 域名
    char *ip;
};

int dns_create_header(struct dns_header *header);
char *convert_domain(const char *domain);
int dns_create_question(struct dns_question *question, const char *hostname);
int dns_build_request(struct dns_header *header, struct dns_question *question, char *request, int rlen);
int is_pointer(int in);
void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len);
int dns_parse_response(char *buffer, struct dns_item **domains);
int dns_client_commit(const char *domain);

int dns_create_header(struct dns_header *header)
{
    if (NULL == header)
        return -1;
    memset(header, 0, sizeof(struct dns_header));
    srand(time(NULL));
    header->id = rand();
    header->flags = htons(0x0100); // 0000 0001 0000 0000   //网络字节序是大端序
    header->questions = htons(1);
    return 0;
}

char *convert_domain(const char *domain)
{
    if (!domain)
        return NULL;
    char *temp = strdup(domain);
    if (!temp)
        return NULL;
    char *token = NULL;
    char *result = (char *)malloc(strlen(domain) + 2);
    if (!result)
    {
        free(temp);
        return NULL;
    }
    memset(result, 0, strlen(domain) + 2);
    char *original_result = result;
    // 第一次分割
    token = strtok(temp, ".");
    while (token != NULL)
    {
        int len = strlen(token);
        *result = (char)len; // int类型变为char类型（保留低八位——最多存储1字节的长度 0-255 即char可以表示的长度范围）
                             //*((unsigned char *)result) = (unsigned char)len; 长度前缀本质上是 char 类型的值
        result++;
        strncpy(result, token, len);
        result += len;
        token = strtok(NULL, ".");
    }
    *result = '\0';
    free(temp);
    return original_result;
}

int dns_create_question(struct dns_question *question, const char *hostname)
{
    if (question == NULL || hostname == NULL)
        return -1;
    memset(question, 0, sizeof(struct dns_question));
    question->length = strlen(hostname) + 2; // 改造之后域名的长度：n个点,代替n个长度。再加上第一个点之前的长度 及 结束符0
    question->qtype = DNS_QTYPE_A;
    question->qclass = DNS_QCLASS_IN;
    // 构建查询名 *hotsname = www.baidu.com -> 3www5baidu3com
    question->name = convert_domain(hostname);
    return 0;
}

int dns_build_request(struct dns_header *header, struct dns_question *question, char *request, int rlen)
{
    if (header == NULL || question == NULL || request == NULL)
        return -1;
    memset(request, 0, rlen);
    // header区
    memcpy(request, header, sizeof(struct dns_header));
    int offset = sizeof(struct dns_header);
    // query区
    memcpy(request + offset, question->name, question->length);
    offset += question->length;
    memcpy(request + offset, &question->qtype, sizeof(question->qtype));
    offset += sizeof(question->qtype);
    memcpy(request + offset, &question->qclass, sizeof(question->qclass));
    offset += sizeof(question->qclass);
    return offset;
}

int is_pointer(int in)
{
    // 判断是否是压缩指针
    return ((0xC0 & in) == 0xC0);
}

// type=5时，使用该函数解析域名
void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len)
{
    // chunk: DNS数据包起始地址
    // ptr: 指向域名的指针（可能带压缩） 压缩指针占两个字节
    // out: 保存解析后的域名
    // len：输出字符串out的长度

    int flag = 0, offset = 0;
    // pos 是一个指向 out 中当前可以写入的位置（因为out 可能已经包含了之前的域名）
    char *pos = out + (*len);
    while (1)
    {
        // 循环退出条件
        flag = (int)ptr[0]; // 内存的第一个字节,即2种情况：1-要么是当前段的指针标记2-要么是当前段的长度
        if (flag == 0)
            break; // 遇到0表示域名结束

        if (is_pointer(flag))
        {
            // 情况一——压缩指针
            // 提取压缩指针后14位，即偏移量（前两位是11）
            offset = ((ptr[0] & 0x3F) << 8) | ptr[1];
            ptr = chunk + offset;
            dns_parse_name(chunk, ptr, out, len); // 递归解析
            break;
        }
        else
        {
            // 情况二——非压缩指针
            // 直接解析，解析3www5baidu3com -> www.baidu.com
            // DNS采用长度前缀，第一个字节告诉解析器接下来有多少字节是字符数据
            ptr++; // 跳过长度字节
            memcpy(pos, ptr, flag);
            ptr += flag;  // 更新读取位置
            pos += flag;  // 更新输出位置
            *len += flag; // 更新已经读取的长度
            // 添加 .
            if ((int)ptr[0] != 0)
            {
                memcpy(pos, ".", 1);
                pos += 1;
                *len += 1;
            } // ptr[0] 是下一个段的长度字节
        }
    }
}

// 解析DNS响应报文，提取域名、类型（A记录 or CNAME记录）、TTL以及IP地址
int dns_parse_response(char *buffer, struct dns_item **domains)
{
    // buffer : 完整的DNS响应报文，包括header,querys,answer区
    // dns ： 指针数组，访问每条记录是domains[N]
    unsigned char *ptr = buffer;
    // 跳过id和flag
    ptr += 4;
    // 获取、跳过问题数
    int querys = ntohs(*(unsigned short *)ptr); // unsigned short是2个字节。这句代码含义是读取 ptr 指向的地址开始的 2 字节数据
    ptr += 2;
    // 获取、跳过答案数
    int answers = ntohs(*(unsigned short *)ptr);
    ptr += 2;
    // 跳过剩余header部分
    ptr += 4;
    // 跳过query部分
    int i = 0;
    for (i = 0; i < querys; i++)
    {
        while (1)
        {
            // 3www5baidu3com
            int flag = (int)ptr[0]; // 查询区是完整标签（非压缩指针）
            ptr = ptr + flag + 1;   // +1是长度字节本身
            if (flag == 0)
                break;
        }
        // 跳过qtype 和 qclass
        ptr += 4;
    }

    // 接下来，进入answer区，解析每个答案
    int cnt = 0; // 记录解析的答案数量
    char cname[128], aname[128], netip[4], ip[20];
    int len, type, ttl, datalen;
    struct dns_item *list = (struct dns_item *)calloc(answers, sizeof(struct dns_item));
    if (NULL == list)
        return -1;
    for (i = 0; i < answers; i++)
    {
        bzero(aname, sizeof(aname)); // 清空域名缓冲区
        len = 0;
        // 解析answer区的NAME字段
        dns_parse_name(buffer, ptr, aname, &len);
        ptr += 2; // 这里应该是默认answer区是压缩指针
        // 获取、跳过查询类型
        type = ntohs(*(unsigned short *)ptr);
        ptr += 2;
        // 跳过查询类
        ptr += 2;
        // 获取、跳过生存时间(4个字节)
        ttl = ntohl(*(unsigned int *)ptr);
        ptr += 4;
        // 获取、跳过数据长度
        datalen = ntohs(*(unsigned short *)ptr);
        ptr += 2;

        if (DNS_CNAME == type)
        {
            bzero(cname, sizeof(cname));
            len = 0;
            // 解析CNAME的别名
            dns_parse_name(buffer, ptr, cname, &len);
            ptr = ptr + datalen;
        }
        else if (DNS_HOST == type)
        {
            bzero(ip, sizeof(ip));
            bzero(netip, sizeof(netip));
            if (4 == datalen)
            {
                memcpy(netip, ptr, datalen);
                inet_ntop(AF_INET, netip, ip, sizeof(struct sockaddr));               // 转换为点分十进制
                INFO("%s has address %s\n", aname, ip);                               // 打印域名和 IP
                INFO("\tTime to live: %d minutes, %d seconds\n", ttl / 60, ttl % 60); // 打印 TTL

                // 分配内存并存储域名
                list[cnt].domain = (char *)calloc(strlen(aname) + 1, 1);
                memcpy(list[cnt].domain, aname, strlen(aname));
                // 分配内存并存储ip
                list[cnt].ip = (char *)calloc(strlen(ip) + 1, 1);
                memcpy(list[cnt].ip, ip, strlen(ip));
                cnt++;
            }
            // 跳过数据
            ptr += datalen;
        }
    }

    *domains = list; // 返回解析结果
    ptr += 2;        // 跳过授权区域 和 附加区域
    return cnt;
}

// 函数：执行 DNS 查询
int dns_client_commit(const char *domain)
{
    // 创建 UDP 套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        return -1; // 套接字创建失败
    }

    // 设置 DNS 服务器地址
    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;                       // IPv4
    servaddr.sin_port = htons(DNS_SERVER_PORT);          // 设置端口
    servaddr.sin_addr.s_addr = inet_addr(DNS_SERVER_IP); // 设置 IP

    // 连接到 DNS 服务器(非必须，但是加上使得sendto更可靠。否则连接可能丢失)
    int ret = connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    printf("connect : %d\n", ret); // 打印连接结果

    // 创建 DNS 头部
    struct dns_header header = {0};
    dns_create_header(&header);

    // 创建 DNS 查询
    struct dns_question question = {0};
    dns_create_question(&question, domain);

    // 构建 DNS 请求
    char request[1024] = {0};
    int length = dns_build_request(&header, &question, request, 1024);

    // 发送 DNS 请求
    int slen = sendto(sockfd, request, length, 0, (struct sockaddr *)&servaddr, sizeof(struct sockaddr));
    if (slen < 0)
    {
        perror("sendto failed");
        close(sockfd);
        return -1;
    }

    // 接收 DNS 响应
    char response[1024] = {0};
    struct sockaddr_in addr;
    size_t addr_len = sizeof(struct sockaddr_in);
    int n = recvfrom(sockfd, response, sizeof(response), 0, (struct sockaddr *)&addr, (socklen_t *)&addr_len);

    // 解析 DNS 响应
    struct dns_item *dns_domain = NULL;
    int cnt = dns_parse_response(response, &dns_domain);
    if (n < 0)
    {
        perror("recvfrom failed");
        close(sockfd);
        return -2;
    }

    // 释放解析结果的内存
    for (int i = 0; i < cnt; i++)
    {
        free(dns_domain[i].domain);
        free(dns_domain[i].ip);
    }
    free(dns_domain);

    return n; // 返回接收到的字节数
}

#endif