#include "map.c"
#include <stdio.h>

struct map_t map;

static int a = 5;
static int c = 8;
int main() {
    int b = 0; 
    map_init(&map, 100, NULL, NULL);
    map_set(&map, "hello", &a);
    map_set(&map, "c", &c);
    b = *(int *) map_get(&map, "hello");
    map_remove(&map, "c");
    map_clear(&map);
    map_unit(&map);
    printf("b = %d\n", b);
    return 0;
}
