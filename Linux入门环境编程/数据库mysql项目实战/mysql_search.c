#include <stdio.h>
#include <mysql/mysql.h>
#include <string.h>

#define INFO printf
#define host "192.168.92.129"
#define user "admin"
#define passwd "12345678"
#define dbName "KING_DB"
#define search "SELECT * FROM TBL_USER"
#define port 3306

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
    if (!mysql_real_connect(mysql, host, user, passwd, dbName, port, NULL, 0))
    {
        INFO("connect failed: %s\n", mysql_error(mysql));
        return -2;
    }

    // 查询
    /*
        mysql_store_result(MYSQL *conn) —— 从 mysql_query 得到的结果集保存到本地内存中
        mysql_num_rows(MYSQL_RES *result) —— 获取结果集中行数
        mysql_num_fields(MYSQL_RES *result) —— 获取结果集中列数
        mysql_fetch_row(MYSQL_RES *result) —— 从结果集中逐行获取数据
        mysql_free_result(MYSQL_RES *result) —— 释放结果集占用的内存
    */
    // 执行查询，数据库返回结果
    if (mysql_query(mysql, search))
    {
        INFO("mysql_query failed: %s\n", mysql_error(mysql));
        return -3;
    }
    // 读取结果
    MYSQL_RES *res = mysql_store_result(mysql);
    if (!res)
    {
        INFO("mysql_store_result: %s\n", mysql_error(mysql));
        return -4;
    }
    int rows = mysql_num_rows(res);
    INFO("rows: %d\n", rows);
    int fields = mysql_num_fields(res);
    INFO("fields: %d\n", fields);
    // 打印一行
    MYSQL_ROW row = NULL;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
        for (int i = 0; i < fields; i++)
        {
            INFO("%s\t", row[i]); // 指针数组,数组中每个指针表示一列
        }
        INFO("\n");
    }
    mysql_free_result(res);

    // 关闭
    mysql_close(mysql);
}