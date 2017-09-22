#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "dplus.h"

void 
print_addrinfo(struct addrinfo *ai) {
    if(ai) {
        printf("addrinfo: %p\n", (void *)ai);
        printf("ai_flags = %d\n", ai->ai_flags);
        printf("ai_family = %d\n", ai->ai_family);
        printf("ai_socktype = %d\n", ai->ai_socktype);
        printf("ai_protocol = %d\n", ai->ai_protocol);
        printf("ai_addrlen = %lu\n", (unsigned long) (ai->ai_addrlen));
        printf("ai_canonname = (%p) %s\n", (void*)(ai->ai_canonname), ai->ai_canonname);
        printf("ai_addr = %p\n", (void *)(ai->ai_addr));
        printf("ai_next = %p\n", (void*)(ai->ai_next));
        printf("\n");
        if(ai->ai_next) {
            print_addrinfo(ai->ai_next);
        }
    }
    else {
        printf("addrinfo: %p\n", (void *)ai);
    }
}

void
test(int argc, char *argv[]) {
    struct addrinfo hints;
    struct addrinfo *ailist;
    int ret;
    char *node;
    #ifdef __WIN32__
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
    #endif

    if(argc < 2) {
        node = "api.chivox.com";
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
    print_addrinfo(ailist); 
    if(ailist) dp_freeaddrinfo(ailist);

    sleep(10);

    ret = dp_getaddrinfo(node, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo(ailist); 
    if(ailist) dp_freeaddrinfo(ailist);

    sleep(1);

    ret = dp_getaddrinfo(node, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo(ailist); 
    if(ailist) dp_freeaddrinfo(ailist);
}


#ifdef __WIN32__

void
print_addrinfo_w(struct addrinfoW *ai) {
    if(ai) {
		printf("addrinfoW: %p\n", (void *)ai);
        printf("ai_flags = %d\n", ai->ai_flags);
        printf("ai_family = %d\n", ai->ai_family);
        printf("ai_socktype = %d\n", ai->ai_socktype);
        printf("ai_protocol = %d\n", ai->ai_protocol);
        printf("ai_addrlen = %lu\n", (unsigned long)(ai->ai_addrlen) );
        wprintf(L"ai_canonname = (%p) %s\n", (void*)(ai->ai_canonname), ai->ai_canonname);
        printf("ai_addr = %p\n", (void *)(ai->ai_addr));
        printf("ai_next = %p\n", (void*)(ai->ai_next));
		printf("\n");
		if(ai->ai_next) {
			print_addrinfo_w(ai->ai_next);
		}
    }
	else {
		printf("addrinfoW: %p\n", (void *)ai);
	}
}

void
test_w(int argc, char *argv[]) {
    struct addrinfoW hints;
    struct addrinfoW *ailist;
    char *node;
	WCHAR node_w[1024];
	int ret;
	
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
            1000
            );

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0;

    ret = dp_getaddrinfo_w(node_w, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo_w(ailist); 
    if(ailist) dp_freeaddrinfo_w(ailist);

    sleep(10);

    ret = dp_getaddrinfo_w(node_w, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo_w(ailist); 
    if(ailist) dp_freeaddrinfo_w(ailist);

    sleep(5);

    ret = dp_getaddrinfo_w(node_w, NULL, &hints, &ailist);
    printf("ret = %d\n", ret);
    print_addrinfo_w(ailist); 
    if(ailist) dp_freeaddrinfo_w(ailist);
}
#endif /* __WIN32__ */

int
main(int argc, char *argv[]) {
#ifdef __WIN32__
    test_w(argc, argv);
    /*
    test(argc, argv);
     */
#else
    test(argc, argv);
#endif
    return 0;
}
