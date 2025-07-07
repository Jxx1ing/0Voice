#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h> //free

#define INFO printf
#define THREADCOUNT 20
#define LIST_INSERT(item, list)    \
    do                             \
    {                              \
        (item)->prev = NULL;       \
        (item)->next = (list);     \
        if (list)                  \
            ((list)->prev) = item; \
        (list) = item;             \
    } while (0)

// 考虑删除的是第一个节点的情况
// 这里的删除只是从链表上移除，但内存还在。因此在业务层需要free这个要删除节点
#define LIST_REMOVE(item, list)                \
    do                                         \
    {                                          \
        if ((list) == item)                    \
            (list) = (list)->next;             \
        if ((item)->prev)                      \
            (item)->prev->next = (item)->next; \
        if ((item)->next)                      \
            (item)->next->prev = (item)->prev; \
        (item)->prev = (item)->next = NULL;    \
    } while (0)

// 办业务的人
struct nTask
{
    void (*task_func)(struct nTask *task); // 函数指针 函数参数
    void *user_data;                       // 传递给任务的数据（task_func函数中需要使用）

    struct nTask *prev;
    struct nTask *next;
};

// 柜员
struct nWorker
{
    pthread_t threadid;       // 线程id
    int terminate;            // 终止
    struct nManager *manager; // 管理线程池的指针

    struct nWorker *prev;
    struct nWorker *next;
};

// 管理者（锁，条件变量）
typedef struct nManager
{
    struct nTask *tasks;     // 任务链表头节点
    struct nWorker *workers; // 工作者链表头节点
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadPool;

// 每个线程实际干的活
//  task结构体中有需要打印的user_data
void task_entry(struct nTask *task)
{
    char *domain = (char *)task->user_data;
    printf("Thread [%lu] is resolving domain: %s\n", pthread_self(), domain);

    dns_client_commit(domain); // 发起 DNS 查询

    free(task->user_data); // 释放 strdup 的字符串
    free(task);            // 释放任务结构体
}

// 线程启动函数（arg 是 worker)
static void *nThreadPoolCallback(void *arg)
{
    struct nWorker *worker = (struct nWorker *)arg;
    // 在合适的时机执行任务(循环)
    while (1)
    {
        pthread_mutex_lock(&worker->manager->mutex);
        // 当前没有需要执行的任务
        while (worker->manager->tasks == NULL)
        {
            // 如果要终止线程池 即后面销毁时有worker->terminate = 1
            if (worker->terminate)
                break;
            // 如果线程池一直在工作，那么等待worker被唤醒（自动解锁、等待……唤醒后持有锁）
            pthread_cond_wait(&worker->manager->cond, &worker->manager->mutex);
        }

        // 程序运行到这里，说明有要执行的任务
        // 但如果要终止线程池 即后面销毁时有worker->terminate = 1
        if (worker->terminate)
        {
            pthread_mutex_unlock(&worker->manager->mutex); // 避免死锁（其他线程卡在获取锁那一行，无法继续运行）
            break;
        }
        // 执行任务——删除任务队列的头节点（使用task是防止头节点信息丢失）
        struct nTask *task = worker->manager->tasks;
        LIST_REMOVE(task, worker->manager->tasks);
        pthread_mutex_unlock(&worker->manager->mutex);
        // 核心步骤——调用task_func执行
        task->task_func(task);
    }
    // 一个线程(柜员)完成任务后，销毁
    free(worker);
}

int nThreadPoolCreate(ThreadPool *pool, int numWorkers)
{
    if (pool == NULL)
        return -1;
    if (numWorkers < 1)
        numWorkers = 1;
    memset(pool, 0, sizeof(ThreadPool));
    // 静态初始化锁
    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mutex, &blank_mutex, sizeof(pthread_mutex_t));
    // 初始化条件变量
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));
    // 主要步骤
    for (int i = 0; i < numWorkers; i++)
    {
        // pthread_create的第一个参数
        struct nWorker *worker = (struct nWorker *)malloc(sizeof(struct nWorker));
        if (worker == NULL)
        {
            perror("malloc worker");
            return -2;
        }
        memset(worker, 0, sizeof(worker));
        worker->manager = pool;
        // 创建线程（柜员）
        int ret = pthread_create(&worker->threadid, NULL, nThreadPoolCallback, worker);
        if (ret)
        {
            perror("pthread_create");
            free(worker);
            return -3;
        }
        // 加入工作者链表
        LIST_INSERT(worker, pool->workers);
    }
    return 0;
}

int nThreadPoolPushTask(ThreadPool *pool, struct nTask *task)
{
    pthread_mutex_lock(&pool->mutex);
    LIST_INSERT(task, pool->tasks);   // 将一个任务放入线程池
    pthread_cond_signal(&pool->cond); // 唤醒一个线程（柜员），执行工作
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int nThreadPoolDestory(ThreadPool *pool, int nWorker)
{
    struct nWorker *worker = NULL;
    for (worker = pool->workers; worker != NULL; worker = worker->next)
    {
        worker->terminate = 1; // 终止条件
    }

    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond); // 广播唤醒所有线程
    pthread_mutex_unlock(&pool->mutex);

    // 任务/工作链表头置为空
    pool->workers = NULL;
    pool->tasks = NULL;

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    return 0;
}