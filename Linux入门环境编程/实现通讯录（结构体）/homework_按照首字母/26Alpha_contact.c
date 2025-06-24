#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NAME_LENGTH 15
#define PHONE_LENGTH 20
#define BUFFER_LENGTH 128
#define MIN_LENGTH 5
#define ALPHA_COUNT 26
#define INFO printf

struct person
{
    char name[NAME_LENGTH];
    char phone[PHONE_LENGTH];
};

struct node
{
    struct person *data;
    struct node *next;
    struct node *prev;
};

struct contact
{
    struct node *heads[ALPHA_COUNT];
    int count;
};

#define LIST_INSERT(item, list)    \
    do                             \
    {                              \
        (item)->prev = NULL;       \
        (item)->next = (list);     \
        if (list)                  \
            ((list)->prev) = item; \
        (list) = item;             \
    } while (0)

#define LIST_REMOVE(item, list)                \
    do                                         \
    {                                          \
        if ((list) == item)                    \
            (list) = (list)->next;             \
        if ((item)->prev)                      \
            (item)->prev->next = (item)->next; \
        if ((item)->next)                      \
            (item)->next->prev = (item)->prev; \
        (item)->prev = (item)->next = NULL;    \
    } while (0)

int get_index_by_name(const char *name)
{
    if (!name || !name[0])
        return -1;

    char ch = name[0];
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    else if (ch >= 'a' && ch <= 'z')
        return ch - 'a';
    else
        return -1;
}

int person_insert(struct node **ppeople, struct node *ps)
{
    if (!ps)
        return -1;
    LIST_INSERT(ps, *ppeople);
    return 0;
}

int person_delete(struct node **ppeople, struct node *ps)
{
    if (!ps || !(*ppeople))
        return -1;
    LIST_REMOVE(ps, *ppeople);
    return 0;
}

struct node *person_search(struct node *people, const char *name)
{
    struct node *item = NULL;
    for (item = people; item != NULL; item = item->next)
    {
        if (!strcmp(item->data->name, name))
        {
            break;
        }
    }
    return item;
}

int person_traversal(struct node *people)
{
    struct node *item = NULL;
    for (item = people; item != NULL; item = item->next)
    {
        INFO("name: %s, phone: %s\n", item->data->name, item->data->phone);
    }
    return 0;
}

int paraser_token(char *buffer, int length, char *name, char *phone)
{
    if (!buffer || length < MIN_LENGTH)
        return -1;

    int status = 0, i = 0, j = 0;
    for (i = 0; buffer[i] != ',' && i < length; i++)
    {
        if (' ' == buffer[i])
            status = 1;
        else if (1 == status)
            name[j++] = buffer[i];
    }

    j = 0;
    status = 0;
    for (; i < length; i++)
    {
        if (' ' == buffer[i])
            status = 1;
        else if (1 == status)
            phone[j++] = buffer[i];
    }

    INFO("加载信息：name: %s, phone: %s\n", name, phone);
    return 0;
}

int insert_entry(struct contact *cts)
{
    if (!cts)
        return -1;

    struct node *p = (struct node *)malloc(sizeof(struct node));
    if (!p)
        return -2;
    memset(p, 0, sizeof(struct node));

    p->data = (struct person *)malloc(sizeof(struct person));
    if (!p->data)
    {
        free(p);
        return -3;
    }
    memset(p->data, 0, sizeof(struct person));

    INFO("请输入要插入的name: \n");
    scanf("%s", p->data->name);
    INFO("请输入要插入的phone: \n");
    scanf("%s", p->data->phone);

    int index = get_index_by_name(p->data->name);
    if (index == -1)
    {
        INFO("姓名非法\n");
        free(p->data);
        free(p);
        return -4;
    }

    person_insert(&cts->heads[index], p);
    cts->count++;
    INFO("插入成功！\n");
    return 0;
}

int print_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("打印通讯录：\n");
    for (int i = 0; i < ALPHA_COUNT; ++i)
    {
        if (cts->heads[i])
        {
            printf("## 首字母 %c ##\n", 'A' + i);
            person_traversal(cts->heads[i]);
        }
    }
    return 0;
}

int delete_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入要删除的人名: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    int index = get_index_by_name(name);
    if (index == -1)
    {
        INFO("非法姓名\n");
        return -2;
    }

    struct node *person = person_search(cts->heads[index], name);
    if (!person)
    {
        INFO("删除失败，不存在此人\n");
        return -3;
    }

    person_delete(&cts->heads[index], person);
    free(person->data);
    free(person);
    return 0;
}

int search_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入要查找的人名\n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);

    int index = get_index_by_name(name);
    if (index == -1)
    {
        INFO("非法姓名\n");
        return -2;
    }

    struct node *person = person_search(cts->heads[index], name);
    if (!person)
    {
        INFO("不存在这个人\n");
        return -3;
    }
    INFO("name: %s, phone: %s\n", person->data->name, person->data->phone);
    return 0;
}

int save_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入保存文件的名称: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);

    FILE *fp = fopen(filename, "w");
    if (!fp)
        return -2;

    for (int i = 0; i < ALPHA_COUNT; ++i)
    {
        struct node *cur = cts->heads[i];
        while (cur)
        {
            fprintf(fp, "name: %s,phone: %s\n", cur->data->name, cur->data->phone);
            cur = cur->next;
        }
    }
    fclose(fp);
    return 0;
}

int load_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入导入文件的名称: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);

    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -2;

    while (!feof(fp))
    {
        char buffer[BUFFER_LENGTH] = {0};
        fgets(buffer, BUFFER_LENGTH, fp);
        int length = strlen(buffer);

        char name[NAME_LENGTH] = {0};
        char phone[PHONE_LENGTH] = {0};
        if (paraser_token(buffer, length, name, phone))
            continue;

        int index = get_index_by_name(name);
        if (index == -1)
            continue;

        struct node *p = (struct node *)malloc(sizeof(struct node));
        if (!p)
            return -3;
        memset(p, 0, sizeof(struct node));

        p->data = (struct person *)malloc(sizeof(struct person));
        if (!p->data)
        {
            free(p);
            return -4;
        }
        memset(p->data, 0, sizeof(struct person));
        memcpy(p->data->name, name, NAME_LENGTH);
        memcpy(p->data->phone, phone, PHONE_LENGTH);

        person_insert(&cts->heads[index], p);
        cts->count++;
    }
    fclose(fp);
    return 0;
}

enum
{
    OPER_INSERT = 1,
    OPER_PRINT,
    OPER_DELETE,
    OPER_SEARCH,
    OPER_SAVE,
    OPER_LOAD
};

void menu_info()
{
    INFO("******************************\n");
    INFO("1.插入\t\t    2.打印\n");
    INFO("3.删除\t\t    4.查找\n");
    INFO("5.保存\t\t    6.加载\n");
    INFO("******************************\n");
}

int main()
{
    struct contact *cts = (struct contact *)malloc(sizeof(struct contact));
    if (!cts)
        return -1;
    memset(cts, 0, sizeof(struct contact));

    while (1)
    {
        menu_info();
        int select = 0;
        scanf("%d", &select);

        switch (select)
        {
        case OPER_INSERT:
            insert_entry(cts);
            break;
        case OPER_PRINT:
            print_entry(cts);
            break;
        case OPER_DELETE:
            delete_entry(cts);
            break;
        case OPER_SEARCH:
            search_entry(cts);
            break;
        case OPER_SAVE:
            save_entry(cts);
            break;
        case OPER_LOAD:
            load_entry(cts);
            break;
        default:
            goto exit;
        }
    }

exit:
    free(cts);
    return 0;
}