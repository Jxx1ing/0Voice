#include <stdio.h>
#include <string.h>
#include <mysql/mysql.h>
#include "mysql_image.h"

int main()
{
    // 初始化
    MYSQL *mysql = mysql_init(NULL);
    if (!mysql)
    {
        INFO("mysql_init failed");
        return -1;
    }
    // 连接
    if (!mysql_real_connect(mysql, HOST, USER, PASSWD, DBNAME, PORT, NULL, 0))
    {
        INFO("connect failed: %s\n", mysql_error(mysql));
        return -2;
    }

    char buffer[FILE_IMAGE_LENGTH] = {0};
    // 从文件中读取图片数据到buffer
    int len = read_image("ironman.jpg", buffer);
    if (len <= 0)
    {
        INFO("read_image failed\n");
        goto Exit;
    }
    // 从buffer往数据库添加图片
    mysql_write_image(mysql, buffer, len);
    // 从数据库中读取图片到buffer
    memset(buffer, 0, FILE_IMAGE_LENGTH);
    len = mysql_read_image(mysql, buffer);
    if (len <= 0)
    {
        INFO("mysql_read failed\n");
        goto Exit;
    }
    // 从buffer将图片保存到磁盘（保存为a.jpg）
    write_image(buffer, "a.jpg", len);

Exit:
    mysql_close(mysql);
    return 0;
}