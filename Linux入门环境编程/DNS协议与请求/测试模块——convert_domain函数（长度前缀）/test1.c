#include <stdio.h>
#include <stdlib.h>
#include "dns.h"

int main()
{
    const char *example = "www.baidu.com";
    unsigned char *converted = convert_domain(example);
    if (!converted)
    {
        printf("conversion failed\n");
        return 1;
    }

    for (unsigned char *p = converted;;)
    {
        unsigned char len = *p;
        if (len == 0)
        {
            break;
        }

        printf("[%d]", len);
        p++;

        for (int i = 0; i < len; i++)
        {
            putchar(p[i]);
        }

        p += len;
    }
    printf("\n");
}
