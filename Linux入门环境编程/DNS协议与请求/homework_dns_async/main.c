#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dns.h"
#include "threadpool.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s domain1 [domain2 ... domainN]\n", argv[0]);
        return -1;
    }

    ThreadPool pool = {0};
    nThreadPoolCreate(&pool, THREADCOUNT);

    for (int i = 1; i < argc; i++)
    {
        struct nTask *task = malloc(sizeof(struct nTask));
        memset(task, 0, sizeof(struct nTask));
        task->task_func = task_entry;
        task->user_data = strdup(argv[i]); // ���������ַ���
        nThreadPoolPushTask(&pool, task);
    }

    getchar(); // �ȴ��û����»س���ȷ������ִ�����
    nThreadPoolDestory(&pool, THREADCOUNT);
    return 0;
}
