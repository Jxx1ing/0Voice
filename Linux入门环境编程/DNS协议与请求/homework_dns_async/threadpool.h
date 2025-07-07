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

// ����ɾ�����ǵ�һ���ڵ�����
// �����ɾ��ֻ�Ǵ��������Ƴ������ڴ滹�ڡ������ҵ�����Ҫfree���Ҫɾ���ڵ�
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

// ��ҵ�����
struct nTask
{
    void (*task_func)(struct nTask *task); // ����ָ�� ��������
    void *user_data;                       // ���ݸ���������ݣ�task_func��������Ҫʹ�ã�

    struct nTask *prev;
    struct nTask *next;
};

// ��Ա
struct nWorker
{
    pthread_t threadid;       // �߳�id
    int terminate;            // ��ֹ
    struct nManager *manager; // �����̳߳ص�ָ��

    struct nWorker *prev;
    struct nWorker *next;
};

// �����ߣ���������������
typedef struct nManager
{
    struct nTask *tasks;     // ��������ͷ�ڵ�
    struct nWorker *workers; // ����������ͷ�ڵ�
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} ThreadPool;

// ÿ���߳�ʵ�ʸɵĻ�
//  task�ṹ��������Ҫ��ӡ��user_data
void task_entry(struct nTask *task)
{
    char *domain = (char *)task->user_data;
    printf("Thread [%lu] is resolving domain: %s\n", pthread_self(), domain);

    dns_client_commit(domain); // ���� DNS ��ѯ

    free(task->user_data); // �ͷ� strdup ���ַ���
    free(task);            // �ͷ�����ṹ��
}

// �߳�����������arg �� worker)
static void *nThreadPoolCallback(void *arg)
{
    struct nWorker *worker = (struct nWorker *)arg;
    // �ں��ʵ�ʱ��ִ������(ѭ��)
    while (1)
    {
        pthread_mutex_lock(&worker->manager->mutex);
        // ��ǰû����Ҫִ�е�����
        while (worker->manager->tasks == NULL)
        {
            // ���Ҫ��ֹ�̳߳� ����������ʱ��worker->terminate = 1
            if (worker->terminate)
                break;
            // ����̳߳�һֱ�ڹ�������ô�ȴ�worker�����ѣ��Զ��������ȴ��������Ѻ��������
            pthread_cond_wait(&worker->manager->cond, &worker->manager->mutex);
        }

        // �������е����˵����Ҫִ�е�����
        // �����Ҫ��ֹ�̳߳� ����������ʱ��worker->terminate = 1
        if (worker->terminate)
        {
            pthread_mutex_unlock(&worker->manager->mutex); // ���������������߳̿��ڻ�ȡ����һ�У��޷��������У�
            break;
        }
        // ִ�����񡪡�ɾ��������е�ͷ�ڵ㣨ʹ��task�Ƿ�ֹͷ�ڵ���Ϣ��ʧ��
        struct nTask *task = worker->manager->tasks;
        LIST_REMOVE(task, worker->manager->tasks);
        pthread_mutex_unlock(&worker->manager->mutex);
        // ���Ĳ��衪������task_funcִ��
        task->task_func(task);
    }
    // һ���߳�(��Ա)������������
    free(worker);
}

int nThreadPoolCreate(ThreadPool *pool, int numWorkers)
{
    if (pool == NULL)
        return -1;
    if (numWorkers < 1)
        numWorkers = 1;
    memset(pool, 0, sizeof(ThreadPool));
    // ��̬��ʼ����
    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    memcpy(&pool->mutex, &blank_mutex, sizeof(pthread_mutex_t));
    // ��ʼ����������
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    memcpy(&pool->cond, &blank_cond, sizeof(pthread_cond_t));
    // ��Ҫ����
    for (int i = 0; i < numWorkers; i++)
    {
        // pthread_create�ĵ�һ������
        struct nWorker *worker = (struct nWorker *)malloc(sizeof(struct nWorker));
        if (worker == NULL)
        {
            perror("malloc worker");
            return -2;
        }
        memset(worker, 0, sizeof(worker));
        worker->manager = pool;
        // �����̣߳���Ա��
        int ret = pthread_create(&worker->threadid, NULL, nThreadPoolCallback, worker);
        if (ret)
        {
            perror("pthread_create");
            free(worker);
            return -3;
        }
        // ���빤��������
        LIST_INSERT(worker, pool->workers);
    }
    return 0;
}

int nThreadPoolPushTask(ThreadPool *pool, struct nTask *task)
{
    pthread_mutex_lock(&pool->mutex);
    LIST_INSERT(task, pool->tasks);   // ��һ����������̳߳�
    pthread_cond_signal(&pool->cond); // ����һ���̣߳���Ա����ִ�й���
    pthread_mutex_unlock(&pool->mutex);
    return 0;
}

int nThreadPoolDestory(ThreadPool *pool, int nWorker)
{
    struct nWorker *worker = NULL;
    for (worker = pool->workers; worker != NULL; worker = worker->next)
    {
        worker->terminate = 1; // ��ֹ����
    }

    pthread_mutex_lock(&pool->mutex);
    pthread_cond_broadcast(&pool->cond); // �㲥���������߳�
    pthread_mutex_unlock(&pool->mutex);

    // ����/��������ͷ��Ϊ��
    pool->workers = NULL;
    pool->tasks = NULL;

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond);
    return 0;
}