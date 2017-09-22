#ifndef _DPLUS_H_
#define _DPLUS_H_

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

int dp_getaddrinfo(const char *node, const char *service, \
        const struct addrinfo *hints, struct addrinfo **res);
void dp_freeaddrinfo(struct addrinfo *ai);

#ifdef __WIN32__
int dp_getaddrinfo_w(LPCWSTR node_w, LPCWSTR service_w, \
        const struct addrinfoW *hints, struct addrinfoW **res);
void dp_freeaddrinfo_w(struct addrinfoW *ai);
#endif

void dp_cache_clear();

#ifdef __cplusplus
}
#endif

#endif /* _DPLUS_H_ */
