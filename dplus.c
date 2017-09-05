/*
 * dnspod域名解析服务
 * reflink: https://www.dnspod.cn/
 * TODO: 测试在长时间未连接到服务器，是否能够超时返回,超时时间是多少
 * TODO: 测试在长时间未收到数据时，是否能够超时返回，超时时间是多少
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#ifndef NULL
#define NULL 0
#endif

#include "http.c"
#include "des.c"

#define HTTPDNS_DEFAULT_SERVER "119.29.29.29"
#define HTTPDNS_DEFAULT_PORT   80
#define HTTP_DEFAULT_DATA_SIZE 1024
#define INVALID_DES_ID -1
#define DES_KEY_SIZE 8
#define IP_BUFFER_SIZE 128

static uint32_t des_id = INVALID_DES_ID;
static char des_key[DES_KEY_SIZE] = { 0 };
static uint32_t des_used = 0;
/*
int aiengine_getaddrinfo(const char *hostname, const char *service, \
        const struct addrinfo *hints, struct addrinfo **result) {
    char ip[IP_BUFFER_SIZE];
    int isIP = 1;
    int i = 0;
    int ret = 0;

    isIP = 1;
    for(i=0; i<strlen(hostname); i++) {
        if(hostname[i] == ':') {
            isIP = 1;
            break;
        }
        if(hostname[i] != '.' && hostname[i] < '0' && hostname[i] > '9') {
            isIP = 0;
        }
    }

    if(isIP) {
        return getaddrinfo(hostname, service, hints, result);
    }

    memset(ip, 0x00, sizeof(ip));
    ret = dns_pod((char *)hostname, 0, 0, 0, 0, ip, 0);
    if(ret < 0 || strlen(ip) == 0) {
        return getaddrinfo(hostname, service, hints, result);
    }
    return getaddrinfo(ip, service, hints, result);
}
*/
static int strchr_num(const char *str, char c) {
        int count = 0;
            while (*str){
                        if (*str++ == c){
                                        count++;
                                                }
                            }
                return count;
}

struct host_info {
    /* host address type: AF_INET or AF_INET6 */
    int h_addrtype;

    /*length of address in bytes:
        sizeof(struct in_addr) or sizeof(struct in6_addr)
    */
    int h_length;

    /* length of addr list */
    int addr_list_len;
    /* list of addresses */
    char **h_addr_list;
};

static void host_info_clear(struct host_info *host) {
    int i;
    for (i = 0; i < host->addr_list_len; i++) {
        if (host->h_addr_list[i]) {
            free(host->h_addr_list[i]);
        }
    }
    free(host->h_addr_list);
    free(host);
}

static struct host_info *http_query(const char *node, time_t *ttl) {
    int i, ret, sockfd;
    struct host_info *hi;
    char http_data[HTTP_DEFAULT_DATA_SIZE + 1];
    char *http_data_ptr, *http_data_ptr_head;
    char *comma_ptr;
    char * node_en;

#ifdef WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    sockfd = make_connection(HTTPDNS_DEFAULT_SERVER, HTTPDNS_DEFAULT_PORT);
    if (sockfd < 0) {
#ifdef WIN32
        WSACleanup();
#endif
        return NULL;
    }

    if (des_used) {
        node_en = des_encode_hex((char *)node, strlen(node), des_key); 
        snprintf(http_data, HTTP_DEFAULT_DATA_SIZE, "/d?dn=%s&ttl=1&id=%d", node_en, des_id);
        free(node_en);
    }
    else
        snprintf(http_data, HTTP_DEFAULT_DATA_SIZE, "/d?dn=%s&ttl=1", node);
    http_data[HTTP_DEFAULT_DATA_SIZE] = 0;

    printf("http_data = %s\n", http_data);

    ret = make_request(sockfd, HTTPDNS_DEFAULT_SERVER, http_data);
    if (ret < 0){
#ifdef WIN32
        closesocket(sockfd);
        WSACleanup();
#else
        close(sockfd);
#endif
        return NULL;
    }

    ret = fetch_response(sockfd, http_data, HTTP_DEFAULT_DATA_SIZE);
#ifdef WIN32
    closesocket(sockfd);
#else
    close(sockfd);
#endif
    if (ret < 0) {
#ifdef WIN32
        WSACleanup();
#endif
        return NULL;
    }

    if (des_used) {
        http_data_ptr = des_decode_hex(http_data, des_key, NULL); 
        if (NULL == http_data_ptr) {
#ifdef WIN32
            WSACleanup();
#endif
            return NULL;
        }
        http_data_ptr_head = http_data_ptr;
    }
    else {
        http_data_ptr = http_data;
    }

    comma_ptr = strchr(http_data_ptr, ',');
    if (comma_ptr != NULL) {
        sscanf(comma_ptr + 1, "%ld", ttl);
        *comma_ptr = '\0';
    }
    else {
        *ttl = 0;
    }

    hi = (struct host_info *)malloc(sizeof(struct host_info));
    if (hi == NULL) {
        fprintf(stderr, "malloc struct host_info failed\n");
        if(des_used) {
            free(http_data_ptr_head);
        }
#ifdef WIN32
        WSACleanup();
#endif
        return NULL;
    }

    /* Only support IPV4 */
    hi->h_addrtype = AF_INET;
    hi->h_length = sizeof(struct in_addr);
    hi->addr_list_len = strchr_num(http_data_ptr, ';') + 1;
    hi->h_addr_list = (char **)calloc(hi->addr_list_len, sizeof(char *));
    if (hi->h_addr_list == NULL) {
        fprintf(stderr, "calloc addr_list failed\n");
        free(hi);
        goto error;
    }

    for (i = 0; i < hi->addr_list_len; ++i) {
        char *addr;
        char *ipstr = http_data_ptr;
        char *semicolon = strchr(ipstr, ';');
        if (semicolon != NULL) {
            *semicolon = '\0';
            http_data_ptr = semicolon + 1;
        }

        addr = (char *)malloc(sizeof(struct in_addr));
        if (addr == NULL) {
            fprintf(stderr, "malloc struct in_addr failed\n");
            host_info_clear(hi);
            goto error;
        }
        printf("ipstr = %s\n", ipstr);
        ret = inet_pton(AF_INET, ipstr, addr);
        if (ret <= 0) {
            fprintf(stderr, "invalid ipstr:%s\n", ipstr);
            free(addr);
            host_info_clear(hi);
            goto error;
        }

        hi->h_addr_list[i] = addr;
    }

    if (des_used)
        free(http_data_ptr_head);

#ifdef WIN32
    WSACleanup();
#endif
    return hi;

error:
#ifdef WIN32
    WSACleanup();
#endif
    if (des_used)
        free(http_data_ptr_head);

    return NULL;
}

#ifdef __TEST
void test_http_query() {
    time_t ttl;
    struct host_info *hi;
    int i;
    hi = http_query("www.baidu.com", &ttl);
    printf("hi = %p\n", (void *)hi);

    if(hi) {
        for (i = 0; i < hi->addr_list_len; i++) {
            printf("hi->h_addr_list[%d] = %p\n", i, hi->h_addr_list[i]);
            printf("hi->h_addr_list[%d] = %s\n", i, hi->h_addr_list[i]);
        }
        free(hi);
    }
    printf("\n");
}

int main() {
    test_http_query();
    return 0;
}

#endif
