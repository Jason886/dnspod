# 封装dnspod域名解析服务
reflink: https://www.dnspod.cn/

build:
1.linux:
    gcc -D_GNU_SOURCE -D__TEST -std=c89 -pedantic -Wall -Wextra dplus.c
2.mac:
    gcc -D_GNU_SOURCE -D__TEST -std=c89 -pedantic -Wall -Wextra dplus.c
3.win:(need mingw)
    gcc -DWIN32 -D__TEST -std=c89 -pedantic -Wall -Wextra dplus.c -l wsock32 -lWs2_32
