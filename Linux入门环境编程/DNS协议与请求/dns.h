#ifndef DNS_H
#define DNS_H

#include <stdio.h>

struct dns_header
{
    unsigned short id; // usigned char是1个字节
    unsigned short flags;
    unsigned short questions;
    unsigned short answer;
    unsigned short authority;
    unsigned short additional;
};

struct dns_question
{
    int length;
    unsigned short qtype;
    unsigned short qclass;
    unsigned char *name; // 长度+域名
};

struct dns_item
{
    char *domain; // 域名
    char *ip;
};

int dns_create_header(struct dns_header *header);
char *convert_domain(const char *domain);
int dns_create_question(struct dns_question *question, const char *hostname);
int dns_build_request(struct dns_header *header, struct dns_question *question, char *request, int rlen);
int is_pointer(int in);
void dns_parse_name(unsigned char *chunk, unsigned char *ptr, char *out, int *len);
int dns_parse_response(char *buffer, struct dns_item **domains);
int dns_client_commit(const char *domain);

#endif