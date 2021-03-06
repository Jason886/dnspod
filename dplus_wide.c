#ifdef __WIN32__

static size_t 
strlen_w(LPCWSTR wstr) {
    size_t len = 0;
    if(!wstr) return 0;
    while(wstr[len]) {
        len++;
    }
    return len;
}

static LPWSTR 
strdup_w(LPCWSTR wstr) {
    size_t len;
    LPWSTR ret = 0;
    if(!wstr) return NULL;
    len = strlen_w(wstr);
    if(len > 0) {
        ret = malloc((len+1)*sizeof(WCHAR));
        memset(ret, 0x00, (len+1)*sizeof(WCHAR));
        memcpy(ret, wstr, len*sizeof(WCHAR));
    }
    return ret;
}

static char * 
widechar_to_byte(LPCWSTR wstr) {
    char * str = NULL;
    size_t len;
    len = WideCharToMultiByte(CP_UTF8,
                        0,
                        wstr,
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
                            wstr,
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

static struct addrinfoW *
dup_addrinfo_w(struct addrinfoW *ai) {
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
	char *node = NULL, *service = NULL;
	struct host_info *hi = NULL;
	int port = 0, socktype, proto, ret = 0;
    time_t ttl, rawtime;
    struct addrinfoW *answer = NULL;
    struct cache_data * c_data;
    WSADATA wsa;

    node = widechar_to_byte(node_w);
    if (!node) return EAI_NONAME;
	service = widechar_to_byte(service_w);

    WSAStartup(MAKEWORD(2, 2), &wsa);

    _DPLUS_INFO("!!!! node = %s\n", node);
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

    if (service != NULL && service[0] == '*' && service[1] == 0) {
        free(service);
        service = NULL;
    }

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
    
    cache_lock();

    c_data = cache_get((char *)node, ntohs(port));
    if(c_data) {
        hi = c_data->hi;
        time(&rawtime);
        if(c_data->expire_time > rawtime) {
            ret = fillin_addrinfoW_res(res, hi, port, socktype, proto);
            _DPLUS_DEBUG("CACHE_DNS: ret = %d, node = %s\n", ret, node);
            cache_unlock();
            if(ret == 0) goto RET;
            else goto SYS_DNS; 
        }
        cache_remove((char *)node, ntohs(port));
    }

    /*
    * 首先使用HttpDNS向D+服务器进行请求,
    * 如果失败则调用系统接口进行解析，该结果不会缓存
    */
    hi = http_query(node, &ttl);
    if (NULL == hi) {
        _DPLUS_INFO("!!! HTTP_DNS FAILED.\n");
        cache_unlock();
        goto SYS_DNS;
    }

    ret = fillin_addrinfoW_res(res, hi, port, socktype, proto);
    _DPLUS_DEBUG("HTTP_DNS: ret = %d, node = %s\n", ret, node);

    /* 缓存时间 3/4*ttl分钟 */
    if(ret != 0 || cache_set((char *)node, ntohs(port), hi, ttl*60/4*3) != 0) {
        host_info_clear(hi);
    }
    cache_unlock();

    if(ret == 0) goto RET;

SYS_DNS:
    *res = NULL;
    ret = GetAddrInfoW(node_w, service_w, hints, &answer);
    _DPLUS_DEBUG("SYS_DNS: ret = %d, node = %s\n", ret, node);
    if (ret == 0) {
        *res = dup_addrinfo_w(answer);
        FreeAddrInfoW(answer);
        if (*res == NULL) {
			if(node) free(node);
			if(service) free(service);
            return EAI_MEMORY;
        }
    }

RET:
    WSACleanup();
	if(node) free(node);
	if(service) free(service);
    return ret;
}

#endif  /* __WIN32__ */
