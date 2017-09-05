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

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/time.h>
    #include <sys/socket.h> 
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include "http.c"
#include "des.c"


#define IP_BUFFER_SIZE 128

static char *_dnspod_server = "119.29.29.29";   /* dnspod服务地址 */
static int _dnspod_port = 80;

static int dns_pod(char * dn, char * local_ip, int encrypt, \
        int key_id, char * key, char *ip_out, int *ttl);


int aiengine_getaddrinfo(const char *hostname, const char *service, \
        const struct addrinfo *hints, struct addrinfo **result) {
    char ip[IP_BUFFER_SIZE];
    int isIP = 1;
    int i = 0;
    int ret = 0;

    isIP = 1;
    for(i=0; i<strlen(hostname); i++) {
        /* 含有,':',则认为传入的是IPv6地址 */
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


static int _connect() {
    struct in_addr inaddr;
    struct sockaddr_in addr;
    int sockfd;
#ifdef _WIN32
    int timeout = 15000;
#else
    struct timeval timeout = {15, 0};
#endif

    inaddr.s_addr = inet_addr(_dnspod_server);
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_dnspod_port);
    addr.sin_addr = inaddr;

    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        return -1;
    }

#ifdef _WIN32
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
#else
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

    if(connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        return -1;
    }
    return sockfd;
}

static void free_ips(char **ips, int count) {
    int i = 0;
    for(i=0; i< count; i++) {
        char * ip = (char *)(((char **)(ips))[i]);
        free(ip);
    }
    free(ips);
}

static void hex_2_bin(char *hex, size_t size, char *bin) {
    size_t i;
    char a, b;
    for(i=0; i < size; i+=2) {
        a = hex[i];
        a = (a >= '0' && a <= '9') ? (a - '0') : 
            (
             (a >= 'A' && a <= 'F') ? (a - 'A' + 10) : (a - 'a' + 10)
            );

        b = hex[i+1];
        b = (b >= '0' && b <= '9') ? (b - '0') : 
            (
             (b >= 'A' && b <= 'F') ? (b - 'A' + 10) : (b - 'a' + 10)
            );
        bin[i/2] = (a<<4) + b;
    }
}

static void bin_2_hex(char *bin, size_t size, char *hex, int upper) {
    size_t i;
    char a;
    for(i=0; i < size; i++) {
        a = (bin[i] >> 4) & 0x0F;
        hex[i*2] = (a >= 0 && a <= 9) ? (a + '0') :
            (upper ? (a - 10 + 'A') : (a - 10 + 'a'));

        a = (bin[i]) & 0x0F;
        hex[i*2+1] = (a >= 0 && a <= 9) ? (a + '0') :
            (upper ? (a - 10 + 'A') : (a - 10 + 'a'));
    }
}

static void parse_data(char *data, char * key, int *count, char ***ips, int *ttl) {
    const int STATUS = 0;
    const int HEADER = 1;
    const int BODY = 2;
    int state = STATUS;
    char * line = 0;
    char * body = 0;
    char * body_bin = 0;
    char * body_de = 0;
    size_t body_de_size = 0;
    char * ip_str = 0;
    char * ttl_str = 0;
    char * tok = 0;
    char * str_dup = 0;
    void * tmp = 0;

    if(count) *count = 0;
    if(ips) *ips = 0;
    if(ttl) *ttl = 0;

    line = strtok(data, "\n");
    while(line != 0) {
        if(state == STATUS) {
            if(strstr(line, "200 ") == 0) {
                return;
            }
            state = HEADER;
            goto next;
        }
        if(state == HEADER) {
            if(strstr(line, ":") != 0) {
                goto next;
            }
            state = BODY;
        }
        if(state == BODY) {
            body = line + strlen(line) +1;
            break;
        }
next:
        line = strtok(0, "\n");
    }

    if(!body || strlen(body) == 0) {
        return;
    }

    /* key != NULL, 表示使用加密功能 */
    if(key) {
        body_bin = malloc(strlen(body)/2+1);
        if(!body_bin) return;
        hex_2_bin(body, strlen(body)/2+1, body_bin);
        des_ecb_pkcs5(body_bin, strlen(body)/2, key, &body_de, &body_de_size, 'd');
        free(body_bin);
        body_de[body_de_size] = 0;
        body = body_de;
    }

    printf("body = %s\n", body);

    ip_str = strtok(body, ",");
    ttl_str = strtok(0, ",");

    if(ip_str) {
        tok = strtok(ip_str, ";");
        printf("tok = %s\n", tok);
        while(tok != 0) {
            if(count) {
                (*count)++;
                if(ips) {
                    if(*count == 1) {
                        *ips = malloc((*count)*sizeof(char *));
                        if(!(*ips)) {
                            *count = 0;
                            goto next_tok;
                        }
                    }
                    else {
                        tmp = realloc(*ips, (*count)*sizeof(char *));
                        if(!tmp) {
                            (*count)--;
                            goto next_tok;
                        }
                        *ips = tmp;
                    }
                    str_dup = malloc(strlen(tok) +1);
                    strcpy(str_dup, tok);
                    ((char **)(*ips))[(*count)-1] = str_dup;
                }
            }
next_tok:
            tok = strtok(0, ";");
            printf("tok = %s\n", tok);
        }
    }

    if(ttl_str && strlen(ttl_str)) {
        if(ttl) *ttl = atoi(ttl_str);
    }

    if(body_de) free(body_de);
}

static int dns_pod(char * dn, char * local_ip, int encrypt, int key_id, char * key, char *ip_out, int *ttl) {
    int sock = -1;
    char *fmt;
    char *dn_en = 0;
    char *dn_en_hex = 0;
    size_t dn_en_size = 0;
    char buff[4096];
    char * data = 0;
    int total, nsend, sended, nread, readed;
    int count;
    char **ips = 0;
    int ttl_rsp;
    int ret = -1;
    char *tmp = 0;

    sock = _connect();
    if(sock < 0) {
        return -1;
    }

    if(local_ip && strlen(local_ip) > 0) {
        if(ttl) if(encrypt)
                fmt = "GET /d?dn=%s&ip=%s&id=%d&ttl=1 HTTP/1.1\r\n" "\r\n";
            else
                fmt = "GET /d?dn=%s&ip=%s&ttl=1 HTTP/1.1\r\n" "\r\n";
        else if(encrypt)
                fmt = "GET /d?dn=%s&ip=%s&id=%d HTTP/1.1\r\n" "\r\n";
            else
                fmt = "GET /d?dn=%s&ip=%s HTTP/1.1\r\n" "\r\n";

        if(encrypt) {
            des_ecb_pkcs5(dn, strlen(dn), key, &dn_en, &dn_en_size, 'e');
            if(!dn_en) goto ERR;
            dn_en_hex = malloc(dn_en_size*2+1);
            if(!dn_en_hex) goto ERR;
            memset(dn_en_hex, 0x00, dn_en_size*2+1); 
            bin_2_hex(dn_en, dn_en_size, dn_en_hex, 1); 
            snprintf((char *)buff, sizeof(buff), (const char *)fmt, dn_en_hex, local_ip, key_id);
        }
        else {
            snprintf(buff, sizeof(buff), fmt, dn, local_ip);
        }
    }
    else {
        if(ttl) if(encrypt) 
                fmt = "GET /d?dn=%s&id=%d&ttl=1 HTTP/1.1\r\n" "\r\n";
            else 
                fmt = "GET /d?dn=%s&ttl=1 HTTP/1.1\r\n" "\r\n";
        else if(encrypt) 
                fmt = "GET /d?dn=%s&id=%d HTTP/1.1\r\n" "\r\n";
            else 
                fmt = "GET /d?dn=%s HTTP/1.1\r\n" "\r\n";

        if(encrypt) {
            des_ecb_pkcs5(dn, strlen(dn), key, &dn_en, &dn_en_size, 'e');
            if(!dn_en) goto ERR;
            dn_en_hex = malloc(dn_en_size*2+1);
            if(!dn_en_hex) goto ERR;
            memset(dn_en_hex, 0x00, dn_en_size*2+1); 
            bin_2_hex(dn_en, dn_en_size, dn_en_hex, 1); 
            snprintf(buff, sizeof(buff), fmt, dn_en_hex, key_id);
        }
        else {
            snprintf(buff, sizeof(buff), fmt, dn);
        }
    }

    printf("send: %s\n", buff);

    total = strlen(buff);
    sended = 0;
    while(sended < total) {
        nsend = send(sock, buff+sended, total-sended, 0);
        if(nsend < 0) {
            goto ERR;
        }
        sended +=nsend;
    }

    readed = 0;
    data = 0;
    while(1) {
        memset(buff, 0x00, sizeof(buff));
        nread = recv(sock, buff, sizeof(buff), 0);
        if(nread <= 0) {
            break;
        }
        if(data) {
            tmp = realloc(data, readed + nread); 
            if(!tmp) goto ERR;
            data = tmp;
        }
        else {
            data = malloc(nread);
            if(!data) goto ERR;
        }
        memcpy(data+readed, buff, nread);
        readed +=nread;
    }

    printf("recv data = %s\n, readed = %d\n", data, readed);
  
    if(encrypt) {
        parse_data(data, key, &count, &ips, &ttl_rsp);
    }
    else {
        parse_data(data, 0,  &count, &ips, &ttl_rsp);
    }

    printf("count = %d\n", count);
    memset(ip_out, 0x00, IP_BUFFER_SIZE);
    if(ips && count > 0) {
       strncpy(ip_out, (char *)(((char**)(ips))[0]), IP_BUFFER_SIZE-1);
    }
    if(ips) free_ips(ips, count);
    if(ttl) *ttl = ttl_rsp;
    if(count <= 0) goto ERR;
    ret = 0;

ERR:
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    if(dn_en) free(dn_en);
    if(dn_en_hex) free(dn_en_hex);
    if(data) free(data);
    return ret;
}

#ifdef __TEST

void test_hex_2_bin() {
    char * test_hex = "ABCDEF0123456789abcdef";
    char bin[40];
    char hex[100];
    int i;

    memset(bin, 0x00, sizeof(bin));
    hex_2_bin(test_hex, strlen(test_hex), bin);
    for(i=0; i<40; i++) {
        printf("%02X ", (uint8_t) bin[i]);
    }

    memset(hex, 0x00, sizeof(hex));
    bin_2_hex(bin, 11, hex, 1);
    hex[22] = 0;
    printf("hex = %s\n", hex);
    
    memset(hex, 0x00, sizeof(hex));
    bin_2_hex(bin, 11, hex, 0);
    hex[22] = 0;
    printf("hex = %s\n", hex);
}

int test_dns_pod() {
    int ttl = 0;
    char ip[IP_BUFFER_SIZE];
    char *key = "weijianliao";
    int key_id = 1;
    int ret = 0;

#ifdef _WIN32
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(1,1),&wsadata)==SOCKET_ERROR) {
        return -1;
    }
#endif

    memset(ip, 0x00, sizeof(ip));
    ret = dns_pod("www.google.com", 0, 0, key_id, key, ip, 0);
    printf("ret = %d\n", ret);
    printf("ip = %s\n", ip);
    printf("\n");
    
    return 0;
}

int test_des() {
    char *input = "helloworldwhatab";
    char *key = "chivox.com";
    char *data2 = 0;
    size_t outsize = 0;
    char * data3 = 0;
    size_t data3size = 0;
    size_t i;

    printf("input = %s\n", input);
    des_ecb_pkcs5(input, strlen(input), key, &data2, &outsize, 'e');

    printf("outsize = %ld\n", outsize);
    for(i = 0; i < outsize; i++) {
        printf("%02X ", (uint8_t) (data2[i]));
    }
    printf("\n");

    des_ecb_pkcs5(data2, outsize, key, &data3, &data3size, 'd'); 
    printf("data3size = %ld\n", data3size);
    for(i = 0; i < data3size; i++) {
        printf("%02X ", (uint8_t) (data3[i]));
    }

    data3[data3size] = 0;
    printf("data3 = %s\n", data3);
    printf("\n");

    free(data2);
    free(data3);
    return 0;
}

int main() {
    test_dns_pod();
    return 0;
}
#endif
