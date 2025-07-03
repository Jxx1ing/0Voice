#include <mysql/mysql.h>
#include <stdbool.h>
#include <string.h>
#include "mysql_image.h"

// 读取磁盘（文件/图片）数据到内存
int read_image(const char *filepath, char *buffer)
{
    if (!filepath)
        return -1;
    FILE *fp = fopen(filepath, "rb");
    if (!fp)
    {
        INFO("fopen failed in read_iamge\n");
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    int image_length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    fread(buffer, 1, image_length, fp);
    fclose(fp);
    return image_length;
}

// 读取内存数据存储到磁盘
int write_image(char *buffer, const char *filepath, int buffer_length)
{
    if (!buffer)
        return -1;
    if (!filepath)
        return -2;
    FILE *fp = fopen(filepath, "wb");
    if (!fp)
    {
        INFO("fopen failed in write_image\n");
        return -1;
    }
    fwrite(buffer, 1, buffer_length, fp);
    return buffer_length;
}

// 从数据库中读取图片
int mysql_read_image(MYSQL *mysql, char *buffer)
{
    MYSQL_STMT *stmt = mysql_stmt_init(mysql);
    if (!stmt)
    {
        INFO("mysql_stmt_init failed in mysql_read_image.\n");
        return -1;
    }
    int ret = mysql_stmt_prepare(stmt, SQL_SELECT_IMG_USER, strlen(SQL_SELECT_IMG_USER));
    if (ret)
    {
        INFO("mysql_stmt_prepare failed in mysql_read_image.\n");
        return -2;
    }
    MYSQL_BIND result = {0};                   // MYSQL_BIND是一个结构体
    result.buffer_type = MYSQL_TYPE_LONG_BLOB; // LONGBLOB 是一种用于存储大型二进制数据的 MySQL 数据类型
    unsigned long buffer_length = 0;
    result.length = &buffer_length;
    int is_null = 1;
    result.is_null = &is_null; // 0表示写入图像的数据不是NULL（如果设置为1，表示is_null是NULL，不会向数据库写入）
    mysql_stmt_bind_result(stmt, &result);
    mysql_stmt_execute(stmt);
    /*获取结果
    分为2种情况——1.直接读取完 2.图片数据过大，多次读取
    退出循环的条件是mysql_stmt_fetch返回值不是0(说明下一行没数据可以提取) 且 返回值不是数据截断（说明过大的数据已经被循环提取完成）
    */
    while (1)
    {
        ret = mysql_stmt_fetch(stmt);
        if (ret != 0 && ret != MYSQL_DATA_TRUNCATED)
            break;
        // 运行到这里，说明有提取到数据（一次读取完数据 或者 截断的数据（缓冲区过小））
        int offset = 0;
        while (offset < (int)buffer_length)
        {
            result.buffer = buffer + offset; // 缓冲区指针最新的位置
            result.buffer_length = default;  // mysql_stmt_fetch_column() 每次只会读取 1 字节的数据并存入 result.buffer。
            mysql_stmt_fetch_column(stmt, &result, image_column, offset);
            offset += result.buffer_length;
        }
    }
    mysql_stmt_close(stmt);
    return buffer_length;
}

// 将图片数据插入到数据库UBL_USER表中
int mysql_write_image(MYSQL *mysql, char *buffer, int length)
{
    MYSQL_STMT *stmt = mysql_stmt_init(mysql);
    if (!stmt)
    {
        INFO("mysql_stmt_init failed in mysql_write_image.\n");
        return -1;
    }
    int ret = mysql_stmt_prepare(stmt, SQL_INSERT_IMG_USER, strlen(SQL_INSERT_IMG_USER));
    if (ret)
    {
        INFO("mysql_stmt_prepare failed in mysql_write_image.\n");
        return -2;
    }
    MYSQL_BIND bind = {0};
    bind.buffer_type = MYSQL_TYPE_LONG_BLOB;
    bind.buffer = NULL;
    bind.length = NULL; // 设置为NULL(不使用绑定参数)，因为使用mysql_stmt_send_long_data的buffer参数发送数据
    bind.length = NULL; // 设置为NULL(不使用绑定参数)，因为每次发送的数据块长度由mysql_stmt_send_long_data的length参数指定
    int is_null = 0;    // 数据不是NULL
    bind.is_null = &is_null;
    mysql_stmt_bind_param(stmt, &bind);
    mysql_stmt_send_long_data(stmt, 0, buffer, length);
    mysql_stmt_execute(stmt);
    mysql_stmt_close(stmt);
    return length;
}
