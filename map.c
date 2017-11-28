#include <stdlib.h>
#include <string.h>

#define _MAP_CAP_DEFAULT 128

#define FREE_KEY(map, key) \
if(map->free_key && key) { map->free_key(key); }

#define FREE_VALUE(map, value) \
if(map->free_value && value) { map->free_value(value); }

typedef void (*free_key_f)(char *key);
typedef void (*free_value_f)(void *value);

struct map_node_t {
    char *key;
    void *value;
    struct map_node_t *next;
};

struct map_t {
    size_t capacity;
    free_key_f free_key;
    free_value_f free_value;
    struct map_node_t **nodes;
};

static int hash(char *key) {
    int i = 0;
    while (*key) { i += i * 131 + (*key++); }
    return i & 0x7FFFFFFF;
}

static int equals(char *key1, char *key2) {
    return !strcmp(key1, key2);
}

static int
map_init(struct map_t *map, size_t capacity, free_key_f free_k, free_value_f free_v) {
    if(!map) return -1;
    map->nodes = malloc(capacity * sizeof(void*));
    if(map->nodes) {
        memset(map->nodes, 0x00, capacity * sizeof(void*));
        map->capacity = capacity ? capacity : _MAP_CAP_DEFAULT;
        map->free_key = free_k;
        map->free_value = free_v;
        return 0;
    }
    return -1;
}

static void map_clear(struct map_t *map);

static void
map_unit(struct map_t *map) {
    if(map) {
        map_clear(map);
        free(map->nodes);
    }
}

static struct map_t *
map_new(size_t capacity, free_key_f free_k, free_value_f free_v) {
    struct map_t * map;
    map = malloc(sizeof(*map));
    if(map) {
        if(map_init(map, capacity, free_k, free_v) != 0) {
            free(map);
            map = NULL;
        }
    }
    return map;
}

static void
map_delete(struct map_t * map) {
    if(map) {
        map_unit(map);
        free(map);
    }
}

static int
map_set(struct map_t *map, char *key, void *val) {
    struct map_node_t *n=NULL, *p=NULL;
    int i;
    
    if(!map) return -1;
    if(!key) return -1;

    i = hash(key) % map->capacity;
    if(!map->nodes[i]) {
        n = malloc(sizeof(*n));
        if(!n) return -1;
        memset(n, 0x00, sizeof(*n));
        map->nodes[i] = n;
    }
    else {
        for(p = map->nodes[i]; p; p = p->next) {
            if(equals(p->key, key)) {
                n = p;
                break;
            }
            else if(!p->next) {
                n = malloc(sizeof(*n));
                if(!n) return -1;
                memset(n, 0x00, sizeof(*n));
                p->next = n;
                break;
            }
        }
    }
    
    if(n->key != key) {
        FREE_KEY(map, n->key);
    }
    if(n->value != val) {
        FREE_VALUE(map, n->value);
    }
    n->key = key;
    n->value = val;
    return 0;
}

static void *
map_get(struct map_t *map, char *key) {
    struct map_node_t * n;
    int i;

    if(!map) return NULL;
    if(!key) return NULL;

    i = hash(key) % map->capacity;
    for(n = map->nodes[i]; n; n = n->next) {
        if(equals(n->key, key)) {
            return n->value;
        }
    }
    return NULL;
}

static void 
map_remove(struct map_t *map , char *key) {
    struct map_node_t *n, *pre;
    int i;

    if(!map) return;
    if(!key) return;

    i = hash(key) % map->capacity;
    for(n = map->nodes[i], pre = NULL; n; pre = n, n = n->next) {
        if(equals(n->key, key)) {
            if(!pre) {
                map->nodes[i] = n->next;
            }
            else {
                pre->next = n->next;
            }
            FREE_KEY(map, n->key);
            FREE_VALUE(map, n->value);
            free(n);
            return;
        }
   }
}

static void
map_clear(struct map_t *map) {
    struct map_node_t *p, *q;
    size_t i;

    if(!map) return;

    for(i = 0; i < map->capacity; ++i) {
        for(p = map->nodes[i]; p;) {
            q = p;
            p = p->next;
            FREE_KEY(map, q->key);
            FREE_VALUE(map, q->value);
            free(q);
        }
        map->nodes[i] = 0;
    }
}

