// mysql_conn_pool.h
#ifndef MYSQL_CONN_POOL_H
#define MYSQL_CONN_POOL_H

#include <mysql/mysql.h>
#include <pthread.h>

#define MAX_CONNECTION 10

#define KING_DB_SERVER_IP "192.168.92.129"
#define KING_DB_SERVER_PORT 3306
#define KING_DB_USERNAME "admin"
#define KING_DB_PASSWORD "12345678"
#define KING_DB_DEFAULTDB "KING_DB"

typedef struct
{
    MYSQL *conns[MAX_CONNECTION];
    int used[MAX_CONNECTION];
    pthread_mutex_t lock;
} MysqlPool;

void mysql_pool_init(MysqlPool *pool);
MYSQL *mysql_pool_get(MysqlPool *pool);
void mysql_pool_release(MysqlPool *pool, MYSQL *conn);
void mysql_pool_destroy(MysqlPool *pool);

int read_image(const char *filename, char *buffer);
int write_image(const char *filename, const char *buffer, int length);
int mysql_write_image(MYSQL *conn, const char *buffer, int length);
int mysql_read_image(MYSQL *handle, char *buffer, int length);

#endif // MYSQL_CONN_POOL_H