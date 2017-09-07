#include "map.c"

static struct map_t *cache = NULL;

struct host_info;
static void host_info_clear(struct host_info *host);

struct cache_data {
    struct host_info *hi;
    size_t expire_time; /* secs from 1970 */
};

static void
cache_lock() {
}

static void 
cache_unlock() {
}

static struct cache_data *
make_cache_data(struct host_info *hi, size_t time) {
    struct cache_data *data;
    data = malloc(sizeof(*data));
    if(data) {
        data->hi = hi;  /* hold, should free it at last.*/
        data->expire_time = /*curtime*/ + time;
    }
    return data;
}

static void free_key(char *key) {
    if(key) {
        free(key);
    }
}

static void free_value(void *value) {
    struct cache_data * data = value;
    if(data) {
        host_info_clear(data->hi);
        free(data);
    }
}

static void
cache_remove(char *domain) {
    if(cache) {
        map_remove(cache, domain);
    }
}

static void 
cache_clear() {
    if(cache) {
        map_clear(cache);
    }
}

static struct cache_data * 
cache_get(char *domain) {
    if(domain && cache) {
        return map_get(cache, domain); 
    }
    return NULL;
}

static int
cache_set(char *domain, void *hi, size_t time) {
    struct cache_data *data;
    char *key;
    int ret;

    if(!domain) return -1;
    cache_remove(domain);
    if(!cache) {
        cache = map_new(6, free_key, free_value);
    }
    if(!cache) return -1;

    key = strdup(domain);
    if(!key) return -1;

    data = make_cache_data(hi, time);
    if(!data) {
        free_key(key);
        return -1;
    }

    ret = map_set(cache, key, data);
    if(ret != 0) {
        free_key(key);
        free_value(data);
    }
    return ret;
}
