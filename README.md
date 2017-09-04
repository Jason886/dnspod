# 封装dnspod域名解析服务
reflink: https://www.dnspod.cn/

build:
1.linux:
    gcc dnspod.c
2.mac:
    gcc dnspod.c
3.win:(need mingw)
    gcc -D_WIN32 dnspod.c -l wsock32
