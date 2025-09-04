#include <testkit.h>
#include <kvdb.h>
#include <stdio.h>

SystemTest(test_kvdb_open, ((const char *[]){})) {
    struct kvdb_t db;
    tk_assert(kvdb_open(&db, "/tmp/test.db") == 0, "Must open db");
    tk_assert(kvdb_close(&db) == 0, "Must close db");
}

SystemTest(test_kvdb_put_get, ((const char *[]){})) {
    struct kvdb_t db;
    char* buf[32] = {};
    tk_assert(kvdb_open(&db, "/tmp/test.db") == 0, "Must open db");
    tk_assert(kvdb_put(&db, "key", "value") == 0, "Must put key");
    printf("%d\n", kvdb_get(&db, "key", buf, 7)); // 测试仅获取长度
    tk_assert(kvdb_get(&db, "key", buf, 7) == 5, "Must get key");
    tk_assert(kvdb_close(&db) == 0, "Must close db");
}

int main() {
    struct kvdb_t db;
    
    // 打开数据库
    if (kvdb_open(&db, "test.db")) {
        perror("Failed to open database");
        return 1;
    }
    
    // 写入数据
    if (kvdb_put(&db, "name", "Alice")) {
        perror("Failed to put value");
    }
    
    // 读取数据
    char buf[32];
    int len = kvdb_get(&db, "name", buf, sizeof(buf));
    if (len >= 0) {
        printf("Value: %s\n", buf);
    } else {
        perror("Key not found");
    }
    
    // 关闭数据库
    kvdb_close(&db);
    return 0;
}
