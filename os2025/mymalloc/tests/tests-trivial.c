// This is just a demonstration of how to write test cases.
// Write good test cases by yourself ;)

#include <testkit.h>
#include <pthread.h>
#include <mymalloc.h>

SystemTest(trivial, ((const char *[]){})) {
    int *p1 = mymalloc(4);
    tk_assert(p1 != NULL, "malloc should not return NULL");
    *p1 = 1024;
    
    int *p2 = mymalloc(4);
    tk_assert(p2 != NULL, "malloc should not return NULL");
    *p2 = 2048;

    printf("p1: %d, p2: %d\n", *p1, *p2);
    tk_assert(*p1 == 1024, "value check should pass");
    tk_assert(*p2 == 2048, "value check should pass");
    tk_assert(p1 != p2, "malloc should return different pointers");
    tk_assert(*p1 * 2 == *p2, "value check should pass");

    myfree(p1);
    myfree(p2);
}

SystemTest(vmalloc, ((const char *[]){})) {
    void *p1 = vmalloc(NULL, 4096);
    tk_assert(p1 != NULL, "vmalloc should not return NULL");
    tk_assert((uintptr_t)p1 % 4096 == 0, "vmalloc should return page-aligned address");

    void *p2 = vmalloc(NULL, 8192);
    tk_assert(p2 != NULL, "vmalloc should not return NULL");
    tk_assert((uintptr_t)p2 % 4096 == 0, "vmalloc should return page-aligned address");
    tk_assert(p1 != p2, "vmalloc should return different pointers");

    vmfree(p1, 4096);
    vmfree(p2, 8192);
}

#define N 100000
void T_malloc1() {
    for (int i = 0; i < N; i++) {
        mymalloc(1);
    }
}
void T_malloc2() {
    for (int i = 0; i < N; i++) {
        mymalloc(10);
    }
}
void T_malloc3() {
    for (int i = 0; i < N; i++) {
        mymalloc(20);
    }
}
void T_malloc4() {
    for (int i = 0; i < N; i++) {
        mymalloc(40);
    }
}

/*SystemTest(concurrent, ((const char *[]){})) {
    // We don't need this malloc_count; you can safely remove this test case.
    extern long malloc_count;
    pthread_t t1, t2, t3, t4;

    pthread_create(&t1, NULL, (void *(*)(void *))T_malloc1, NULL);
    pthread_create(&t2, NULL, (void *(*)(void *))T_malloc2, NULL);
    pthread_create(&t3, NULL, (void *(*)(void *))T_malloc3, NULL);
    pthread_create(&t4, NULL, (void *(*)(void *))T_malloc4, NULL);
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    printf("%ld\n", malloc_count);
    tk_assert(malloc_count == 4 * N, "malloc_count should be 4N");
}*/

#define THREADS 100
#define ITERS 10000
void basic_test() {
    for (int i = 0; i < ITERS; i++) {
        int *ptrs[10] = {};
        
        // 批量分配
        for (int j = 0; j < 10; j++) {
            ptrs[j] = mymalloc(8);
            tk_assert(ptrs[j] != NULL, "mymalloc should not return NULL");
            *ptrs[j] = j; // 写入测试数据
        }
        
        // 验证并释放
        for (int j = 9; j >= 0; j--) {
            tk_assert(*ptrs[j] == j, "mymalloc should return the same pointer");
            myfree(ptrs[j]);
        }
    }
}

SystemTest(basic_concurrent, ((const char *[]){})) {
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, (void *(*)(void *))basic_test, NULL);
    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);
}

void* cross_free_producer(void* arg) {
    void **shared = (void**)arg;
    for (int i = 0; i < ITERS; i++) {
        void *p = mymalloc(128);
        tk_assert(p != NULL, "mymalloc should not return NULL");
        *(volatile int*)p = i;
        *shared = p;
    }
    *shared = NULL; // 结束信号
    return NULL;
}

void* cross_free_consumer(void* arg) {
    void **shared = (void**)arg;
    while (1) {
        void *p = *shared;
        if (p == NULL) break;
        tk_assert(*(volatile int*)p >= 0, "mymalloc should return the same pointer");
        myfree(p);
    }
    return NULL;
}

SystemTest(run_cross_free, ((const char *[]){})) {
    void *shared_ptr = NULL;
    pthread_t prod, cons;
    
    pthread_create(&prod, NULL, cross_free_producer, &shared_ptr);
    pthread_create(&cons, NULL, cross_free_consumer, &shared_ptr);
    
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
}

#define STRESS_ALLOC_SIZE 512

void* stress_thread(void* arg) {
    void *ptrs[100] = {0};
    for (int i = 0; i < 10000; i++) {
        // 随机分配/释放模式
        int idx = rand() % 100;
        if (ptrs[idx]) {
            tk_assert(*(volatile int*)ptrs[idx] == idx, "mymalloc should return the same pointer");
            myfree(ptrs[idx]);
            ptrs[idx] = NULL;
        } else {
            void *p = mymalloc(STRESS_ALLOC_SIZE);
            tk_assert(p != NULL, "mymalloc should not return NULL");
            *(volatile int*)p = idx;
            ptrs[idx] = p;
        }
    }
    return NULL;
}

SystemTest(run_stress_test, ((const char *[]){})) {
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, stress_thread, NULL);
    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);
}

void* boundary_test_thread(void* arg) {
    // 测试极大块分配
    void *big = mymalloc(1UL << 30); // 1GB
    if (big) {
        myfree(big);
    }
    
    // 测试OOM恢复能力
    for (int i = 0; i < 100; i++) {
        void *p = mymalloc(1UL << 20);
        if (p) myfree(p);
    }
    
    return NULL;
}

SystemTest(run_boundary_test, ((const char *[]){})) {
    pthread_t threads[THREADS];
    for (int i = 0; i < THREADS; i++)
        pthread_create(&threads[i], NULL, boundary_test_thread, NULL);
    for (int i = 0; i < THREADS; i++)
        pthread_join(threads[i], NULL);
}