#include "mysql_conn_pool.h"
#include <stdio.h>
#include "string.h"

#define FILE_IMAGE_LENGTH (256 * 1024)

int main()
{
    MysqlPool pool;
    mysql_pool_init(&pool);

    char buffer[FILE_IMAGE_LENGTH] = {0};
    int len = read_image("Ironman.jpg", buffer);

    MYSQL *conn = mysql_pool_get(&pool);
    if (conn)
    {
        mysql_write_image(conn, buffer, len);
        mysql_pool_release(&pool, conn);
    }

    memset(buffer, 0, FILE_IMAGE_LENGTH);
    conn = mysql_pool_get(&pool);
    if (conn)
    {
        int read_len = mysql_read_image(conn, buffer, FILE_IMAGE_LENGTH);
        write_image("a.jpg", buffer, read_len);
        mysql_pool_release(&pool, conn);
    }

    mysql_pool_destroy(&pool);
    return 0;
}