#include "map.c"

#define KEY_BUFF_SIZE 256

struct host_info;
static void host_info_clear(struct host_info *host);

static struct map_t *cache = NULL;
static pthread_mutex_t cache_mtx = PTHREAD_MUTEX_INITIALIZER;

struct cache_data {
    struct host_info *hi;
    time_t expire_time; /* secs from 1970 */
};

static void
cache_lock() {
    pthread_mutex_lock(&cache_mtx);
}

static void 
cache_unlock() {
    pthread_mutex_unlock(&cache_mtx);
}

static void
make_cache_key(char *key_buff, size_t key_buff_size, char *domain, int port) {
    snprintf(key_buff, key_buff_size, "%s:%d", domain ? domain : "null", port);
}

static struct cache_data *
make_cache_data(struct host_info *hi, size_t ttl) {
    struct cache_data *data;
    time_t rawtime;
    data = malloc(sizeof(*data));
    if(data) {
        data->hi = hi;  /* hold, should free at last.*/
        time(&rawtime);
        data->expire_time = rawtime + ttl;
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
        if(data->hi) {
            host_info_clear(data->hi);
        }
        free(data);
    }
}

static void
cache_remove(char *domain, int port) {
    char key_buff[KEY_BUFF_SIZE] = {0};
    if(cache && domain) {
        make_cache_key(key_buff, KEY_BUFF_SIZE, domain, port);
        map_remove(cache, key_buff);
    }
}

static void 
cache_clear() {
    if(cache) {
        map_clear(cache);
    }
}

static struct cache_data * 
cache_get(char *domain, int port) {
    char key_buff[KEY_BUFF_SIZE] = {0};
    if(cache && domain) {
        make_cache_key(key_buff, KEY_BUFF_SIZE, domain, port);
        return map_get(cache, key_buff); 
    }
    return NULL;
}

static int
cache_set(char *domain, int port, void *hi, size_t time) {
    char key_buff[KEY_BUFF_SIZE] = {0};
    struct cache_data *data;
    char *key;
    int ret;

    if(!domain) return -1;
    cache_remove(domain, port);
    if(!cache) {
        cache = map_new(10, free_key, free_value);
    }
    if(!cache) return -1;

    make_cache_key(key_buff, KEY_BUFF_SIZE, domain, port);
    key = strdup(key_buff);
    if(!key) return -1;

    data = make_cache_data(hi, time);
    if(!data) {
        free_key(key);
        return -1;
    }

    ret = map_set(cache, key, data);
    if(ret != 0) {
        free_key(key);
        free(data);  /* 如果缓存失败，应保持对外不变(由外部释放data->hi)。 */
    }
    return ret;
}
