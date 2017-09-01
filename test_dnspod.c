#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include "http_parser.h"

static const char * _dns_server = "119.29.29.29";
static const int _dns_port = 80;

static int _connect() {
    struct in_addr inaddr;
    struct sockaddr_in addr;
    int sock;

    inaddr.s_addr = inet_addr(_dns_server);
    memset(&addr, 0x00, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_dns_port);
    addr.sin_addr = inaddr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        return -1;
    }

    if(connect(sock, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        return -1;
    }

    return sock;
}

void free_ips(char **ips, int count) {
    int i = 0;
    for(i=0; i< count; i++) {
        char * ip = (char *)(((char **)(ips))[i]);
        free(ip);
    }
    free(ips);
}


int parse_data(char *data, int *count, char ***ips, int *ttl) {
    const int STATUS = 0;
    const int HEADER = 1;
    const int BODY = 2;
    int pos = 0;
    int state = STATUS;
    char * line = 0;
    char * body = 0;
    char * ip_str = 0;
    char * ttl_str = 0;
    char * tok = 0;
    char * str_dup = 0;

    if(count) *count = 0;
    if(ips) *ips = 0;
    if(ttl) *ttl = 0;

    line = strtok(data, "\n");
    while(line != 0) {
        if(state == STATUS) {
            if(strstr(line, "200 ") == 0) {
                return -1;
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

    if(body && strlen(body) > 0) {
        printf("body = %s\n", body);

        ip_str = strtok(body, ",");
        printf("ip_str = %s\n", ip_str);
        ttl_str = strtok(0, ",");
        printf("ttl_str = %s\n", ttl_str);

        if(ip_str) {
            tok = strtok(ip_str, ";");
            printf("tok = %s\n", tok);
            while(tok != 0) {
                if(count) {
                    (*count)++;
                    
                    if(*count == 1) {
                        *ips = malloc((*count)*sizeof(char *));
                    }
                    else {
                        printf("realloc\n");
                        *ips = realloc(*ips, (*count)*sizeof(char *));
                        printf("realloc\n");
                    }
                    str_dup = malloc(strlen(tok) +1);
                    strcpy(str_dup, tok);
                    ((char **)(*ips))[(*count)-1] = str_dup;
                    
                }

                tok = strtok(0, ";");
                printf("tok = %s\n", tok);
            }

            printf("*count = %d\n", *count);

            int i = 0;
            for(i=0; i< *count; i++) {
                char * ip = (char *)(((char **)(*ips))[i]);
                printf("ip = %s\n", ip);
            }
        }

        return 0;
    }

    return -1;
}

static int _dns_pod(char * dn, char * ip, int ttl) {
    int sock;
    char *fmt;
    char buff[4096];
    char * data;
    int total, nsend, sended, nread, readed;
    int count;
    char **ips;
    int ttl_rsp;

    sock = _connect();
    if(sock < 0) {
        return -1;
    }

    if(ip && strlen(ip) > 0) {
        if(ttl) {
            fmt = "GET /d?dn=%s&ip=%s&ttl=1 HTTP/1.1\r\n"
                "\r\n";
        }
        else {
            fmt = "GET /d?dn=%s&ip=%s HTTP/1.1\r\n"
                "\r\n";
        }
        snprintf(buff, sizeof(buff)-1, fmt, dn, ip, _dns_server);
    }
    else {
        if(ttl) {
            fmt = "GET /d?dn=%s&ttl=1 HTTP/1.1\r\n"
                "\r\n";
        }
        else {
            fmt = "GET /d?dn=%s HTTP/1.1\r\n"
                "\r\n";
        }
        snprintf(buff, sizeof(buff)-1, fmt, dn, _dns_server);
    }

    printf("send: %s\n", buff);

    total = strlen(buff);
    sended = 0;
    while(sended < total) {
        nsend = send(sock, buff+sended, total-sended, 0);
        if(nsend < 0) {
            return -1;
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
            data = realloc(data, readed + nread); 
        }
        else {
            data = malloc(nread);
        }
        memcpy(data+readed, buff, nread);
        readed +=nread;
    }

    printf("recv data = %s\n, readed = %d\n", data, readed);
    
    parse_data(data, &count, &ips, &ttl_rsp);

    free_ips(ips, count);

    free(data);
    return 0;
}

int main() {
    _dns_pod("www.baidu.com", 0, 1);
}
