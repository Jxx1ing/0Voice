#ifndef MYSQL_IMAGE_H
#define MYSQL_IMAGE_H

#include <stdio.h>

#define INFO printf
#define HOST "192.168.92.129"
#define USER "admin"
#define PASSWD "12345678"
#define DBNAME "KING_DB"
#define PORT 3306

#define SQL_SELECT_IMG_USER "SELECT U_IMG FROM TBL_USER WHERE U_NAME='king';"
#define SQL_INSERT_IMG_USER "INSERT INTO TBL_USER(U_NAME, U_GENDER, U_IMG) VALUES('king', 'man', ?);" //?是占位符
#define default (1)
#define image_column (0) // 结果集只有一列，即U_IMG(筛选条件是king的图片)
#define FILE_IMAGE_LENGTH (256 * 1024)
// typedef unsigned char my_bool; // 其他写法：可以用int代替unsigned char 即直接 int is_null = 0;    //有警告

// 文件和内存间读写
int read_image(const char *filepath, char *buffer);
int write_image(char *buffer, const char *filepath, int buffer_length);
// 数据库读写
int mysql_write_image(MYSQL *mysql, char *buffer, int length);
int mysql_read_image(MYSQL *mysql, char *buffer);

#endif // MYSQL_CONN_POOL_H
