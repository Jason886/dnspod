# 封装dnspod域名解析服务
reflink: https://www.dnspod.cn/

build:
1.linux:
    gcc -D__TEST -std=c89 -pedantic -Wall dnspod.c
2.mac:
    gcc -D__TEST -std=c89 -pedantic -Wall dnspod.c
3.win:(need mingw)
    gcc -D__TEST -D_WIN32 -pedantic -Wall dnspod.c -l wsock32 -lWs2_32
