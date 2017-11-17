## 封装dnspod基于http的域名解析服务
##dnspod: https://www.dnspod.cn/

## build lib:
1.linux:
    gcc -D_GNU_SOURCE -D__TEST -DDEBUG -std=c99 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o
2.mac:
    gcc -D_GNU_SOURCE -D__TEST -DNDEBUG -std=c99 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o
3.win:(need mingw)
    gcc -D__WIN32__ -D__TEST -DNDEBUG -std=c99 -pedantic -Wall -Wextra -c -I. dplus.c -o dplus.o

## build test:
1.linux:
    gcc -D_GNU_SOURCE -std=c99 -pedantic -Wall -Wextra test_dplus.c dplus.o -o test
2.mac:
    gcc -D_GNU_SOURCE -std=c99 -pedantic -Wall -Wextra test_dplus.c dplus.o -o test
3.win:(need mingw)
    gcc -D__WIN32__ -std=c99 -pedantic -Wall -Wextra test_dplus.c dplus.o -o test -l wsock32 -lWs2_32
	
	
    todo:
    1.处理malloc alloc realloc失败
