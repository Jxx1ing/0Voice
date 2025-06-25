#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define PTHREADCOUNT 10

pthread_mutex_t mutex;

void *threadSum_callback(void *arg)
{
    int *pcount = (int *)arg;
    int i = 0;
    while (i < 100000)
    {
        pthread_mutex_lock(&mutex);
        (*pcount)++;
        i++;
        pthread_mutex_unlock(&mutex);
        usleep(1);
    }
}

int main()
{
    // 10个线程，每个线程+100000，累计加到1000000
    int number = 0;
    // 初始化全局锁
    pthread_mutex_init(&mutex, NULL);
    // 10个线程
    pthread_t tid[PTHREADCOUNT];
    // 创建
    for (int i = 0; i < PTHREADCOUNT; i++)
    {
        pthread_create(&tid[i], NULL, threadSum_callback, &number);
    }

    // 打印结果，观察是否到1000000。间隔1s
    for (int j = 0; j < 10; j++) // 10s是为了粗显比较下SpinLock和MutexLock 的执行效率
    {
        printf("累计数值: %d \n", number);
        sleep(1); // 主程序给子程序执行时间，否则主程序先于子程序执行完
    }

    return 0;
}