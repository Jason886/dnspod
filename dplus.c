/*
 * dnspod域名解析服务
 * github: https://github.com/Jason886/dnspod_plus.git
 * reflink: https://www.dnspod.cn/
 * TODO: 测试在长时间未连接到d+服务器，是否能够超时返回,超时时间是多少
 * TODO: 测试在长时间未收到d+数据时，是否能够超时返回，超时时间是多少
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#include "http.c"
#include "des.c"

#define HTTPDNS_DEFAULT_SERVER "119.29.29.29"
#define HTTPDNS_DEFAULT_PORT   80
#define HTTP_DEFAULT_DATA_SIZE 1024
#define DES_KEY_SIZE 8

static uint32_t des_id = -1;
static char des_key[DES_KEY_SIZE] = { 0 };
static uint32_t des_used = 0;

struct host_info {
    int h_addrtype; /* host address type: AF_INET or AF_INET6 */
    int h_length; /* length of address in bytes: 
                     sizeof(struct in_addr) or sizeof(struct in6_addr) */
    int addr_list_len; /* length of addr list */
    char **h_addr_list; /* list of addresses */
};

static int 
strchr_num(const char *str, char c) {
    int count = 0;
    while (*str){
        if (*str++ == c){
            count++;
        }
    }
    return count;
}

static int 
is_address(const char *s) {
    unsigned char buf[sizeof(struct in6_addr)];
    int r;
    r = inet_pton(AF_INET, s, buf);
    if (r <= 0) {
        r = inet_pton(AF_INET6, s, buf);
        return (r > 0);
    }
    return 1;
}

static int 
is_integer(const char *s) {
    if (*s == '-' || *s == '+')
        s++;
    if (*s < '0' || '9' < *s)
        return 0;
    s++;
    while ('0' <= *s && *s <= '9')
        s++;
    return (*s == '\0');
}

static void 
host_info_clear(struct host_info *host) {
    int i;
    for (i = 0; i < host->addr_list_len; i++) {
        if (host->h_addr_list[i]) {
            free(host->h_addr_list[i]);
        }
    }
    free(host->h_addr_list);
    free(host);
}

static struct host_info *
http_query(const char *node, time_t *ttl) {
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

static struct addrinfo *
malloc_addrinfo(int port, uint32_t addr, int socktype, int proto) {
    struct addrinfo *ai;
    struct sockaddr_in *sa_in;
    size_t socklen;
    socklen = sizeof(struct sockaddr);
    
    ai = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
    if (!ai)
        return NULL;
    
    ai->ai_socktype = socktype;
    ai->ai_protocol = proto;
    
    ai->ai_addr = (struct sockaddr *)calloc(1, sizeof(struct sockaddr));
    if (!ai->ai_addr) {
        free(ai);
        return NULL;
    };
    
    ai->ai_addrlen = socklen;
    ai->ai_addr->sa_family = ai->ai_family = AF_INET;
    
    sa_in = (struct sockaddr_in *)ai->ai_addr;
    sa_in->sin_port = port;
    sa_in->sin_addr.s_addr = addr;
    
    return ai;
}

static int 
fillin_addrinfo_res(struct addrinfo **res, struct host_info *hi,
        int port, int socktype, int proto) {
    int i;
    struct addrinfo *cur, *prev = NULL;
    for (i = 0; i < hi->addr_list_len; i++) {
        struct in_addr *in = ((struct in_addr *)hi->h_addr_list[i]);
        cur = malloc_addrinfo(port, in->s_addr, socktype, proto);
        if (cur == NULL) {
            if (*res)
                freeaddrinfo(*res);
            return EAI_MEMORY;
        }
        if (prev)
            prev->ai_next = cur;
        else
            *res = cur;
        prev = cur;
    }
    
    return 0;
}

int
dp_getaddrinfo(const char *node, const char *service,
    const struct addrinfo *hints, struct addrinfo **res) {
    struct host_info *hi = NULL;
    int port = 0, socktype, proto, ret = 0;
    time_t ttl;

    *res = NULL;
    printf("!!!! node = %s\n", node);

    if (node == NULL)
        return EAI_NONAME;

    if (is_address(node) || (hints && (hints->ai_flags & AI_NUMERICHOST)))
        goto sys_dns;

    if (hints && hints->ai_family != PF_INET
        && hints->ai_family != PF_UNSPEC
        && hints->ai_family != PF_INET6) {
        goto sys_dns;
    }
    if (hints && hints->ai_socktype != SOCK_DGRAM
        && hints->ai_socktype != SOCK_STREAM
        && hints->ai_socktype != 0) {
        goto sys_dns;
    }

    /*
    * 首先使用HttpDNS向D+服务器进行请求,
    * 如果失败则调用系统接口进行解析，该结果不会缓存
    */
    hi = http_query(node, &ttl);
    if (NULL == hi) {
        goto sys_dns;
    }

    socktype = (hints && hints->ai_socktype) ? hints->ai_socktype : SOCK_STREAM;
    if (hints && hints->ai_protocol)
        proto = hints->ai_protocol;
    else {
        switch (socktype) {
        case SOCK_DGRAM:
            proto = IPPROTO_UDP;
            break;
        case SOCK_STREAM:
            proto = IPPROTO_TCP;
            break;
        default:
            proto = 0;
            break;
        }
    }

    if (service != NULL && service[0] == '*' && service[1] == 0)
        service = NULL;
    
    if (service != NULL) {
        if (is_integer(service))
            port = htons(atoi(service));
        else {
            struct servent *servent;
            char *pe_proto;
#ifdef WIN32
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
            switch (socktype){
            case SOCK_DGRAM:
                pe_proto = "udp";
                break;
            case SOCK_STREAM:
                pe_proto = "tcp";
                break;
            default:
                pe_proto = "tcp";
                break;
            }
            servent = getservbyname(service, pe_proto);
            if (servent == NULL) {
#ifdef WIN32
                WSACleanup();
#endif
                return EAI_SERVICE;
            }
            port = servent->s_port;
#ifdef WIN32
            WSACleanup();
#endif
        }
    }
    else {
        port = htons(0);
    }

    ret = fillin_addrinfo_res(res, hi, port, socktype, proto);
    printf("http_dns: ret = %d, node = %s\n", ret, node);
    return ret;

sys_dns:
    ret = getaddrinfo(node, service, hints, res);
    printf("sys_dns: ret = %d, node = %s\n", ret, node);
    return ret;
}

#ifdef __TEST
void test(int argc, char *argv[]) {
    struct addrinfo hints;
    struct addrinfo *ailist;
    int ret;
    char *node;

    if(argc < 2) {
        node = "www.baidu.com";
    }
    else {
        node = argv[1];
    }

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    ret = dp_getaddrinfo(node, NULL, &hints, &ailist);

    printf("ret = %d\n", ret);
    if(ailist) freeaddrinfo(ailist);
}

int main(int argc, char *argv[]) {
    test(argc, argv);
    return 0;
}

#endif
