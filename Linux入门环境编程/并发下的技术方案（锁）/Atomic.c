#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#define PTHREADCOUNT 10

// 原子操作
int Inc(int *value, int add)
{
    int old;
    __asm__ volatile(           // __asm__ volatile = 内联汇编 + 防优化
        "lock; xaddl %2, %1"    // lock表示指令是原子操作。 xaal 目标操作数 源操作数（交换并加法）
                                // 作用是写回新值，同时返回旧值（通过交换到寄存器）。   执行后：目标操作数 = 原目标操作数+源操作数；源操作数 = 原目标操作数
                                // %1 → *value（内存地址）   %2 → add（寄存器 eax 的值）。
        : "=a"(old)             // 将 eax 寄存器的值赋给 old
        : "m"(*value), "a"(add) // "m" ：内存操作数
                                // add 存入 eax 寄存器
        : "cc", "memory");

    // 返回值确实是旧值old，但是更改的是value值。 即修改的是主函数main中的value值
    return old;
}

void *threadSum_callback(void *arg)
{
    int i = 0;
    while (i < 1000000)
    {
        Inc(arg, 1);
        i++;
    }
}

int main()
{
    // 10个线程，每个线程+100000，累计加到1000000
    int number = 0;
    // 10个线程
    pthread_t tid[PTHREADCOUNT];
    // 创建
    for (int i = 0; i < PTHREADCOUNT; i++)
    {
        pthread_create(&tid[i], NULL, threadSum_callback, &number);
    }

    // 打印结果，观察是否到1000000。间隔1s
    for (int j = 0; j < 10; j++)
    {
        printf("累计数值: %d \n", number);
        sleep(1); // 主程序给子程序执行时间，否则主程序先于子程序执行完
    }

    return 0;
}