#include <stdio.h>
#include <mysql/mysql.h>
#include <string.h>

#define INFO printf
#define host "192.168.92.129"
#define user "admin"
#define passwd "12345678"
#define dbName "KING_DB"
#define port 3306

int main()
{
    // 初始化
    MYSQL *mysql = mysql_init(NULL);
    if (!mysql)
    {
        INFO("mysql_init failed");
    }

    // 连接
    if (!mysql_real_connect(mysql, host, user, passwd, dbName, port, NULL, 0))
    {
        INFO("CONNECT FAILED: %s\n", mysql_error(mysql));
    }

    // 插入
    const char *cmd_insert = "INSERT INTO TBL_USER(U_NAME, U_GENDER) VALUES('Tom', 'Female')";
    if (mysql_real_query(mysql, cmd_insert, strlen(cmd_insert)))
    {
        INFO("TBL_USER INSERT FAILED: %s\n", mysql_error(mysql));
    }

    // 关闭
    mysql_close(mysql);
}