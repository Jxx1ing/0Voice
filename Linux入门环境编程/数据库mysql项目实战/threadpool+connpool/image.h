#ifndef IMAGE_H
#define IMAGE_H

#include <stdio.h>
#include <mysql/mysql.h>
#include <stdlib.h>
#include <string.h>

#define FILE_IMAGE_LENGTH (256 * 1024)
#define SQL_INSERT_IMG_USER "INSERT INTO TBL_USER(U_NAME, U_GENDER, U_IMG) VALUES('King', 'man', ?);"
#define SQL_SELECT_IMG_USER "SELECT U_IMG FROM TBL_USER WHERE U_NAME='King';"

int read_image(const char *filename, char *buffer)
{
	FILE *fp = fopen(filename, "rb");
	if (!fp)
		return -1;
	fseek(fp, 0, SEEK_END);
	int length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fread(buffer, 1, length, fp);
	fclose(fp);
	return length;
}

int write_image(const char *filename, const char *buffer, int length)
{
	FILE *fp = fopen(filename, "wb");
	if (!fp)
	{
		printf("Failed to read image\n");
		return -1;
	}
	fwrite(buffer, 1, length, fp);
	fclose(fp);
	return 0;
}

int mysql_write_image(MYSQL *conn, const char *buffer, int length)
{
	MYSQL_STMT *stmt = mysql_stmt_init(conn);
	mysql_stmt_prepare(stmt, SQL_INSERT_IMG_USER, strlen(SQL_INSERT_IMG_USER));

	MYSQL_BIND bind = {0};
	bind.buffer_type = MYSQL_TYPE_LONG_BLOB;
	bind.buffer = NULL;
	bind.length = NULL;
	bind.is_null = 0;
	mysql_stmt_bind_param(stmt, &bind);
	mysql_stmt_send_long_data(stmt, 0, buffer, length);
	mysql_stmt_execute(stmt);
	mysql_stmt_close(stmt);
	return 0;
}

int mysql_read_image(MYSQL *handle, char *buffer, int length)
{

	if (handle == NULL || buffer == NULL || length <= 0)
		return -1;

	MYSQL_STMT *stmt = mysql_stmt_init(handle);
	int ret = mysql_stmt_prepare(stmt, SQL_SELECT_IMG_USER, strlen(SQL_SELECT_IMG_USER));
	if (ret)
	{
		printf("mysql_stmt_prepare : %s\n", mysql_error(handle));
		return -2;
	}

	MYSQL_BIND result = {0};

	result.buffer_type = MYSQL_TYPE_LONG_BLOB;
	unsigned long total_length = 0;
	result.length = &total_length;

	ret = mysql_stmt_bind_result(stmt, &result);
	if (ret)
	{
		printf("mysql_stmt_bind_result : %s\n", mysql_error(handle));
		return -3;
	}

	ret = mysql_stmt_execute(stmt);
	if (ret)
	{
		printf("mysql_stmt_execute : %s\n", mysql_error(handle));
		return -4;
	}

	ret = mysql_stmt_store_result(stmt);
	if (ret)
	{
		printf("mysql_stmt_store_result : %s\n", mysql_error(handle));
		return -5;
	}

	while (1)
	{
		ret = mysql_stmt_fetch(stmt);
		if (ret != 0 && ret != MYSQL_DATA_TRUNCATED)
			break;

		int start = 0;
		while (start < (int)total_length)
		{
			result.buffer = buffer + start;
			result.buffer_length = 1;
			mysql_stmt_fetch_column(stmt, &result, 0, start);
			start += result.buffer_length;
		}
	}

	mysql_stmt_close(stmt);

	return total_length;
}

#endif