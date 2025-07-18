#ifndef CONNPOOL_H
#define CONNPOOL_H

#include <mysql/mysql.h>  
#include <pthread.h>      
#include <stdio.h>        

#define CONNECTION_POOL_SIZE 10
#define KING_DB_SERVER_IP "192.168.92.129"
#define KING_DB_SERVER_PORT 3306
#define KING_DB_USERNAME "admin"
#define KING_DB_PASSWORD "12345678"
#define KING_DB_DEFAULTDB "KING_DB"

typedef struct {
    MYSQL *conns[CONNECTION_POOL_SIZE];
    int used[CONNECTION_POOL_SIZE];
    pthread_mutex_t lock;
} ConnPool;

int init_conn_pool(ConnPool *pool)
{
    pthread_mutex_init(&pool->lock, NULL);
    for (int i = 0; i < CONNECTION_POOL_SIZE; i++)
    {
        pool->conns[i] = mysql_init(NULL);
        if (!mysql_real_connect(pool->conns[i],
                                KING_DB_SERVER_IP, KING_DB_USERNAME, KING_DB_PASSWORD,
                                KING_DB_DEFAULTDB, KING_DB_SERVER_PORT, NULL, 0))
        {
            fprintf(stderr, "Connection %d failed: %s\n", i, mysql_error(pool->conns[i]));
            pool->conns[i] = NULL;
        }
        pool->used[i] = 0;
    }
    return 0;
}


MYSQL *get_conn(ConnPool *pool) {
    MYSQL *conn = NULL;
    pthread_mutex_lock(&pool->lock);
    for (int i = 0; i < CONNECTION_POOL_SIZE; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            conn = pool->conns[i];
            break;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return conn;
}

void release_conn(ConnPool *pool, MYSQL *conn) {
    pthread_mutex_lock(&pool->lock);
    for (int i = 0; i < CONNECTION_POOL_SIZE; i++) {
        if (pool->conns[i] == conn) {
            pool->used[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&pool->lock);
}



#endif