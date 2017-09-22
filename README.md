# 封装dnspod基于http的域名解析服务
dnspod: https://www.dnspod.cn/

build lib:
1.linux:
    gcc -D_GNU_SOURCE -D__TEST -std=c89 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o
2.mac:
    gcc -D_GNU_SOURCE -D__TEST -std=c89 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o
3.win:(need mingw)
    gcc -DWIN32 -D__TEST -std=c89 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o -l wsock32 -lWs2_32

build test:
1.linux:
    gcc -D_GNU_SOURCE -std=c89 -pedantic -Wall -Wextra test_dplus.c dplus.o -o test

    todo:
    1.处理malloc alloc realloc失败
