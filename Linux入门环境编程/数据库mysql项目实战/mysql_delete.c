#include <stdio.h>
#include <mysql/mysql.h>
#include <string.h>

#define INFO printf
#define host "192.168.92.129"
#define user "admin"
#define passwd "12345678"
#define dbName "KING_DB"
#define show "SELECT * FROM TBL_USER"
#define delete "CALL PROC_DELETE_USER('Bob')"
#define port 3306

int print_mysql(MYSQL *handle)
{
    MYSQL_RES *res = mysql_store_result(handle);
    if (!res)
    {
        INFO("mysql_store_result: %s\n", mysql_error(handle));
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
}

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
    // 删除前查询
    if (mysql_real_query(mysql, show, strlen(show)))
    {
        INFO("show failed: %s\n", mysql_error(mysql));
        return -3;
    }
    // 删除前打印
    print_mysql(mysql);
    // 删除
    if (mysql_real_query(mysql, delete, strlen(delete)))
    {
        INFO("delete failed: %s\n", mysql_error(mysql));
        return -4;
    }
    INFO("*****************************************\n");
    // 删除后查询
    if (mysql_real_query(mysql, show, strlen(show)))
    {
        INFO("show failed: %s\n", mysql_error(mysql));
        return -5;
    }
    // 删除后打印
    print_mysql(mysql);
    // 关闭
    mysql_close(mysql);
}