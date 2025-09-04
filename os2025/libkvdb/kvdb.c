#include "kvdb.h"

// 打开数据库
int kvdb_open(struct kvdb_t *db, const char *path) {
    if (!db || !path) return -1;
    
    memset(db, 0, sizeof(*db));

    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755) == -1) {
            return -1;
        }
    }
    
    db->path = strdup(path);
    if (!db->path) {
        return -1;
    }
    
    return 0;
}

// 存储键值对
int kvdb_put(struct kvdb_t *db, const char *key, const char *value) {
    if (!db || !key || !value) return -1;
    
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", db->path, key);
    
    int fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return -1;
    }
    
    ssize_t written = write(fd, value, strlen(value));
    close(fd);
    
    return (written >= 0) ? 0 : -1;
}

// 获取键值
int kvdb_get(struct kvdb_t *db, const char *key, char *buf, size_t length) {
    if (!db || !key || !buf || length == 0) return -1;
    
    char file_path[PATH_MAX];
    snprintf(file_path, sizeof(file_path), "%s/%s", db->path, key);
    
    int fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    ssize_t bytes_read = read(fd, buf, length - 1);
    close(fd);
    
    if (bytes_read < 0) {
        return -1;
    }
    
    buf[bytes_read] = '\0';
    return bytes_read;
}

// 关闭数据库
int kvdb_close(struct kvdb_t *db) {
    if (!db) return -1;
    
    free(db->path);
    memset(db, 0, sizeof(*db));
    
    return 0;
}