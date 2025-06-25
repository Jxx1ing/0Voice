#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#define PTHREADCOUNT 10
#define NUMBER 100000

bool CAS(int *addr, int expected_val, int new_val)
{
  bool success;
  __asm__ volatile(
      "lock; cmpxchgl %2, %1\n\t" // 注意如果没有换行或者分隔符，编译器把它拼成了非法的汇编指令！——"lock; cmpxchgl %2, %1sete %0"
      "sete %0"                   // 上一条输出结果比较结果是否成功存入%0( %0代表第一个输出数即后面的success)
      : "=q"(success)             //=q代表8位输出寄存器，保存成功标志     输出
      : "m"(*addr),               //+m代表内存变量（memory operand）    输入
        "a"(expected_val),        //+a代表寄存器（EAX）                 输入
        "r"(new_val)              // r代表任意通用寄存器，只读           输入
      : "memory", "cc");

  return success;
}

// 使用CAS实现原子加法
void atomic_add(int *addr, int add)
{
  int old_value;
  do
  {
    old_value = *addr;
  } while (!CAS(addr, old_value, old_value + add));
  // 如果相等，CAS返回1。则atomic_add执行一次 +1 的操作。结束。
  // 如果不相等， CAS返回0。 则atomic_add开始第二次循环，使得old_value = *addr。接下来 +1，结束。
  // 具体含义是，当多个线程对临界区+1时，如果达不到该线程的预期值old_value，那么old_value更新为这个临界区最新值
}

/*
CAS + 自旋 模式的核心思想：预期值过时就重新获取，直到成功更新

举个多线程下的例子
假设 *addr == 100，并发两个线程：
**************************************************************************
线程 A：
old_value = 100
尝试 CAS(addr, 100, 101)（成功）

线程 B：
old_value = 100
尝试 CAS(addr, 100, 101)（失败，因为此时已经变成了 101）
所以线程 B 会重新读取 old_value = 101，再试一次 CAS(addr, 101, 102)（成功）
**************************************************************************
🧠 所以：最终值是 102，两次 +1 操作都被顺利执行，只不过线程 B 因为竞争失败，多重试了一次。
*/

void *thread_callback(void *arg)
{
  for (int j = 0; j < NUMBER; j++)
  {
    atomic_add(arg, 1); // arg是临界区的地址
    // printf("%d\n", *(int *)arg); //调试
  }
}

int main()
{
  int count = 0; // 临界区
  pthread_t tid[PTHREADCOUNT];

  for (int i = 0; i < PTHREADCOUNT; i++)
  {
    // 开10个线程，每个线程+100000。（atomic_add对临界区每次只加1）
    pthread_create(&tid[i], NULL, thread_callback, &count);
  }

  // 打印结果，每隔1s
  for (int k = 0; k < 10; k++)
  {
    printf("累计数值：%d\n", count);
    sleep(1);
  }
}