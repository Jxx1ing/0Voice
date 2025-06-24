#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NAME_LENGTH 15
#define PHONE_LENGTH 20
#define BUFFER_LENGTH 128
#define MIN_LENGTH 5
#define INFO printf

struct person
{
    char name[NAME_LENGTH];
    char phone[PHONE_LENGTH];
};
// 节点
struct node
{
    struct person *data;
    struct node *next;
    struct node *prev;
};
// 链表
struct contact
{
    struct node *nnode; // 链表头
    int count;
};

// 支持层
// 头插法，item是一个节点,list是双向链表的头节点 两者都是一级指针
// list 是 contacts->nnode, 因此文本替换的时候需要加括号
#define LIST_INSERT(item, list)    \
    do                             \
    {                              \
        (item)->prev = NULL;       \
        (item)->next = (list);     \
        if (list)                  \
            ((list)->prev) = item; \
        (list) = item;             \
    } while (0)

// 考虑删除的是第一个节点的情况
// 这里的删除只是从链表上移除，但内存还在。因此在业务层需要free这个要删除节点
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

// 接口层
// 使用二级指针的目的是修改链表头指针的值，必须传递指针的地址
int person_insert(struct node **ppeople, struct node *ps)
{
    if (!ps)
        return -1;
    LIST_INSERT(ps, *ppeople);
    return 0;
}

int person_delete(struct node **ppeople, struct node *ps)
{
    if (!ps)
        return -1;
    if (!(*ppeople))
        return -2;
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
    // 如果没有匹配到，说明遍历到链表末尾NULL
    return item;
}

int person_traversal(struct node *people)
{
    struct node *item = NULL;
    for (item = people; item != NULL; item = item->next)
    {
        INFO("name: %s,phone: %s\n", item->data->name, item->data->phone);
    }
    return 0;
}

int save_file(struct node *people, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
        return -1;
    while (people)
    {
        fprintf(fp, "name: %s,phone: %s\n", people->data->name, people->data->phone);
        people = people->next;
        fflush(fp); // fprintf内容在缓冲区(不会立即写入磁盘)，fflush将内存写入磁盘（文件）
    }
    fclose(fp);

    return 0;
}

int paraser_token(char *buffer, int length, char *name, char *phone)
{
    if (!buffer)
        return -1;
    if (length < MIN_LENGTH)
        return -2;
    // 使用状态机的思想，提取name 和 phone
    // name: xxx,phone: xxx
    int status = 0, i = 0, j = 0;
    for (i; buffer[i] != ','; i++)
    {
        if (' ' == buffer[i])
            status = 1;
        else if (1 == status)
            name[j++] = buffer[i];
    }

    j = 0;
    status = 0;
    for (i; i < length; i++)
    {
        if (' ' == buffer[i])
            status = 1;
        else if (1 == status)
            phone[j++] = buffer[i];
    }
    INFO("file_token: name: %s,phone: %s\n", name, phone);
    return 0;
}

// 个人感觉：该函数的目的还是使用paraser_token打印用户信息
int load_file(struct node **people, int *count, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
        return -1;
    while (!feof(fp))
    {
        char buffer[BUFFER_LENGTH] = {'\0'};
        fgets(buffer, BUFFER_LENGTH, fp); // 每次读取一行（遇到\n会停止读取），最多读BUFFER_LENGTH-1个
        // 运行到这里，buffer中存储了 name:xxx, phone:xxx
        // 1-使用paraser_token 提取name 和 phone，打印到屏幕
        char name[NAME_LENGTH] = {0};
        char phone[PHONE_LENGTH] = {0};
        int length = strlen(buffer); // 这里buffer长度不是128，因为strlen遇到\0就结束
        if (!paraser_token(buffer, length, name, phone))
            continue;

        // 2-申请内存，将用户信息存入内存
        struct node *p = (struct node *)malloc(sizeof(struct node));
        if (!p)
            return -2;
        memset(p, 0, sizeof(p));
        p->data = (struct person *)malloc(sizeof(struct person));
        if (!p->data)
        {
            free(p);
            return -3;
        }
        memset(p->data, 0, sizeof(struct person));

        memcpy(p->data->name, name, NAME_LENGTH);
        memcpy(p->data->phone, phone, PHONE_LENGTH);
        // 每次将读取到的节点存入链表中
        person_insert(people, p);
        (*count)++;
    }

    fclose(fp);
    return 0;
}

// 业务层
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
    scanf("%s", p->data->name); // 输入直接读入node节点中
    INFO("请输入要插入的phone: \n");
    scanf("%s", p->data->phone);

    // 插入
    if (0 != person_insert(&cts->nnode, p))
    {
        // 运行到这里说明插入失败
        free(p->data);
        free(p);
        return -4;
    }
    cts->count++;
    INFO("插入成功！");

    return 0;
}

int print_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("打印通讯录：\n");
    person_traversal(cts->nnode);

    return 0;
}

int delete_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入要删除的人名: \n");
    char name[NAME_LENGTH] = {0};
    scanf("%s", name);
    struct node *person = person_search(cts->nnode, name);
    if (!person)
    {
        INFO("删除失败，不存在输入的人名\n");
        return -2;
    }
    person_delete(&cts->nnode, person);
    // person_delete只是从链表上移除，但内存还在
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
    struct node *person = person_search(cts->nnode, name);
    if (!person)
    {
        INFO("不存在这个人\n");
        return -2;
    }
    INFO("name: %s,phone: %s\n", person->data->name, person->data->phone);

    return 0;
}

int save_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入保存文件的名称: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    save_file(cts->nnode, filename);

    return 0;
}

int load_entry(struct contact *cts)
{
    if (!cts)
        return -1;
    INFO("请输入导入文件的名称: \n");
    char filename[NAME_LENGTH] = {0};
    scanf("%s", filename);
    load_file(&cts->nnode, &cts->count, filename);

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
