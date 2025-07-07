#include <stdio.h>
#include <stdlib.h>
#include "dns.h"

int main(int argc, char *argv[])
{
    // 检查是否提供了域名参数
    if (argc < 2)
        return -1;
    // 调用 DNS 查询函数
    dns_client_commit(argv[1]);
    return 0; // 返回成功
}
