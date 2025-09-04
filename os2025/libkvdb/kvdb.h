#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

// 数据库结构
struct kvdb_t {
    char *path;             // 数据库目录路径
};

int kvdb_open(struct kvdb_t *db, const char *path);
int kvdb_put(struct kvdb_t *db, const char *key, const char *value);
int kvdb_get(struct kvdb_t *db, const char *key, char *buf, size_t length);
int kvdb_close(struct kvdb_t *db);
