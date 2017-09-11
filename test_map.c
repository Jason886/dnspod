#include "map.c"
#include <stdio.h>

struct map_t map;

static int a = 5;
static int c = 8;


struct test_data {
    char data[1024];
};

char * make_key(int i) {
    char * key=0;
    key = malloc(1024);
    snprintf(key, 1024, "key_%d", i);
    return key;
}

struct test_data * make_data(int i) {
    struct test_data * data;
    data = malloc(sizeof(*data));
    snprintf(data->data, 1024, "data_%d", i);
    return data;
}

void free_key(char * key) {
    if(key) free(key);
}

void free_data(void * data) {
    if(data) free(data);
}

int main() {
    size_t i;
    struct map_t *map;
    char * key;
    struct test_data * data;
    int ret;
    
    map = map_new(20, free_key, free_data);
    for(i = 0; i < 10000; i++) {
        key = make_key(i);
        data = make_data(i);
        ret = map_set(map, key, data);
        if(ret != 0) {
            free_key(key);
            free_data(data);
        }
        ret = map_set(map, key, data);
        if(ret != 0) {
            free_key(key);
            free_data(data);
        }
    }

    printf("~~~~~\n");

    for(i = 0; i < 10000; i++) {
        key = make_key(i);
        data = make_data(i);
        ret = map_set(map, key, data);
        if(ret != 0) {
            free_key(key);
            free_data(data);
        }
    }

    printf("======\n");

    map_clear(map);


    printf(">>>>>\n");
    for(i =0; i<5000; i++) {
        key = make_key(i*2);
        data = make_data(i*2);
        ret = map_set(map, key, data);
        if(ret != 0) {
            free_key(key);
            free_data(data);
        }
    }

    for(i=0; i<10000; i++) {
        key = make_key(i);
        map_remove(map, key);
        free(key);
    }

    return 0;
}
