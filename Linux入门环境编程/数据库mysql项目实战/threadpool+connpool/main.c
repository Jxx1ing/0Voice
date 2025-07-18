#include "connpool.h"
#include "image.h"
#include "threadpool.h"

int main()
{
    ThreadPool pool = {0};
    ConnPool conn_pool = {0};

    // 初始化线程池
    nThreadPoolCreate(&pool, THREADCOUNT);

    // 初始化数据库连接池
    init_conn_pool(&conn_pool);

    // 假设当前目录下有以下图片文件名
    const char *img_files[] = {
        "0Voice.jpg",
        "Ironman.jpg",
        "OpenAL.png",
    };

    int num_files = sizeof(img_files) / sizeof(img_files[0]);

    for (int i = 0; i < num_files; i++)
    {
        struct nTask *task = malloc(sizeof(struct nTask));
        memset(task, 0, sizeof(struct nTask));
        task->task_func = task_entry;

        ImageTask *img_task = malloc(sizeof(ImageTask));
        memset(img_task, 0, sizeof(ImageTask));
        strcpy(img_task->filename, img_files[i]);
        img_task->conn_pool = &conn_pool;

        task->user_data = img_task;

        nThreadPoolPushTask(&pool, task);
    }

    getchar(); // 等待任务执行完成

    nThreadPoolDestory(&pool, THREADCOUNT);

    return 0;
}
