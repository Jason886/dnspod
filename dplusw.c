#ifdef WIN32

static size_t strlen_w(LPCWSTR widestr) {
    int i = 0;
    if(!widestr) return 0;
    while(widestr[i] != 0) {
        i++;
    }
    return i;
}

static LPCWSTR strdup_w(LPCWSTR widestr) {
    int len;
    LPCWSTR ret = 0;
    if(!widestr) return NULL;
    len = strlen(widestr);
    if(len > 0) {
        ret = malloc((len+1)*sizeof(wchar));
        memset(ret, 0x00, (len+1)*sizeof(wchar));
        memcpy(ret, widestr, len*sizeof(wchar));
    }
    return ret;
}

static char * 
widechar_to_byte(LPCWSTR widestr) {
    char * str = 0;
    int len;
    len = WideCharToMultiByte(CP_UTF8,
                        0,
                        widestr,
                        -1,
                        NULL,
                        0,
                        NULL,
                        NULL);
    if(len > 0) {
        str = malloc(len+1);
        memset(str, 0x00, len+1);
        WideCharToMultiByte(CP_UTF8,
                            0,
                            widestr,
                            -1,
                            str,
                            len,
                            NULL,
                            NULL);
    }
    return str;
}

static struct addrinfoW *
malloc_addrinfo_w(int port, uint32_t addr, int socktype, int proto) {
    struct addrinfoW *ai;
    struct sockaddr_in *sa_in;
    size_t socklen;
    socklen = sizeof(struct sockaddr);
    
    ai = (struct addrinfoW *)calloc(1, sizeof(struct addrinfoW));
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

void print_addrinfo_w(struct addrinfoW *ai) {
    printf("addrinfoW: %p\n", (void *)ai);
    if(ai) {
        printf("ai_flags = %d\n", ai->ai_flags);
        printf("ai_family = %d\n", ai->ai_family);
        printf("ai_socktype = %d\n", ai->ai_socktype);
        printf("ai_protocol = %d\n", ai->ai_protocol);
        printf("ai_addrlen = %d\n", ai->ai_addrlen);
        printf("ai_canonname = (%p) %s\n", (void*)(ai->ai_canonname), ai->ai_canonname);
        printf("ai_addr = %p\n", (void *)(ai->ai_addr));
        printf("ai_next = %p\n", (void*)(ai->ai_next));
    }
    printf("\n");
    if(ai->ai_next) {
        print_addrinfo_w(ai->ai_next);
    }
}

void
dp_freeaddrinfo_w(struct addrinfoW *ai) {
    struct addrinfoW *next;
    while (ai != NULL) {
        if (ai->ai_canonname != NULL)
            free(ai->ai_canonname);
        if (ai->ai_addr)
            free(ai->ai_addr);
        next = ai->ai_next;
        free(ai);
        ai = next;
    }
}

static struct 
addrinfoW *dup_addrinfo_w(struct addrinfoW *ai) {
    struct addrinfoW *cur, *head = NULL, *prev = NULL;
    while (ai != NULL) {
        cur = (struct addrinfoW *)malloc(sizeof(struct addrinfoW));
        if (!cur)
            goto error;

        memcpy(cur, ai, sizeof(struct addrinfoW));

        cur->ai_addr = (struct sockaddr *)malloc(sizeof(struct sockaddr));
        if (!cur->ai_addr) {
            free(cur);
            goto error;
        };
        memcpy(cur->ai_addr, ai->ai_addr, sizeof(struct sockaddr));

        if (ai->ai_canonname)
            cur->ai_canonname = strdup_w(ai->ai_canonname);

        if (prev)
            prev->ai_next = cur;
        else
            head = cur;
        prev = cur;

        ai = ai->ai_next;
    }

    return head;

error:
    if (head) {
        dp_freeaddrinfo_w(head);
    }
    return NULL;
}


static int 
fillin_addrinfoW_res(struct addrinfoW **res, struct host_info *hi,
    int port, int socktype, int proto) {
    int i;
    struct addrinfoW *cur, *prev = NULL;
    for (i = 0; i < hi->addr_list_len; i++) {
        struct in_addr *in = ((struct in_addr *)hi->h_addr_list[i]);
        cur = malloc_addrinfo_w(port, in->s_addr, socktype, proto);
        if (cur == NULL) {
            if (*res)
                dp_freeaddrinfo_w(*res);
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
dp_getaddrinfo_w(LPCWSTR node_w, LPCWSTR service_w,
    const struct addrinfoW *hints, struct addrinfoW **res) {
    struct host_info *hi = NULL;
    int port = 0, socktype, proto, ret = 0;
    time_t ttl;
    struct addrinfoW *answer;
    char * node, *service;
    WSADATA wsa;

    if (node_w == NULL)
        return EAI_NONAME;
    node = widechar_to_byte(node_w);
    service = widechar_to_byte(service_w);
    if (node == NULL)
        return EAI_NONAME;

    WSAStartup(MAKEWORD(2, 2), &wsa);

    printf("!!!! node = %s\n", node);
    *res = NULL;
    
    if (is_address(node) || (hints && (hints->ai_flags & AI_NUMERICHOST)))
        goto SYS_DNS;

    if (hints && hints->ai_family != PF_INET
        && hints->ai_family != PF_UNSPEC
        && hints->ai_family != PF_INET6) {
        goto SYS_DNS;
    }
    if (hints && hints->ai_socktype != SOCK_DGRAM
        && hints->ai_socktype != SOCK_STREAM
        && hints->ai_socktype != 0) {
        goto SYS_DNS;
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
                goto SYS_DNS;
            }
            port = servent->s_port;
        }
    }
    else {
        port = htons(0);
    }

    /*
    * 首先使用HttpDNS向D+服务器进行请求,
    * 如果失败则调用系统接口进行解析，该结果不会缓存
    */
    hi = http_query(node, &ttl);
    if (NULL == hi) {
        goto SYS_DNS;
    }

    ret = fillin_addrinfoW_res(res, hi, port, socktype, proto);
    printf("HTTP_DNS: ret = %d, node = %s\n", ret, node);
    host_info_clear(hi);
    if(ret == 0) goto RET;

SYS_DNS:
    *res = NULL;
    ret = GetAddrInfoW(node_w, service_w, hints, &answer);
    printf("SYS_DNS: ret = %d, node = %s\n", ret, node);
    if (ret == 0) {
        *res = dup_addrinfo_w(answer);
        FreeAddrInfoW(answer);
        if (*res == NULL) {
            return EAI_MEMORY;
        }
    }

RET:
    WSACleanup();
    return ret;
}

#ifdef __TEST
void test_w(int argc, char *argv[]) {
    struct addrinfo hints;
    struct addrinfo *ailist;
    int ret;
    char *node, node_w[300];
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    if(argc < 2) {
        node = "www.baidu.com";
    }
    else {
        node = argv[1];
    }


    MultiByteToWideChar(CP_UTF8,
            0,
            node,
            -1,
            node_w,
            200
            );

    node = node_w;

    strlen_w(node);

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    ret = dp_getaddrinfo_w(node, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo(ailist); 
    if(ailist) dp_freeaddrinfo_w(ailist);
}

#endif

#endif
