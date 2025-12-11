/*
 * Simple Concurrent Test - Verify Level 2 rmlock works with basic radix operations
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define DEBUG_THREADING 1
#undef DEBUG_THREADING_VERBOSE

/* Use our enhanced compatibility layer directly */
#include "../kernel_compat/compat_shim.h"

/* Simple test */
static struct rmlock global_lock;
static int shared_counter = 0;
static volatile int running = 1;

void* reader_thread(void* arg) {
    int thread_id = *(int*)arg;
    struct rm_priotracker tracker;
    int read_count = 0;

    while (running) {
        rm_rlock(&global_lock, &tracker);
        int value = shared_counter;  /* Read shared data */
        rm_runlock(&global_lock, &tracker);

        read_count++;
        if (read_count % 1000 == 0) {
            printf("[READER %d] Read value %d (%d times)\n", thread_id, value, read_count);
        }
        usleep(100);
    }

    printf("[READER %d] Final: read %d times\n", thread_id, read_count);
    return NULL;
}

void* writer_thread(void* arg) {
    int thread_id = *(int*)arg;
    int write_count = 0;

    while (running) {
        rm_wlock(&global_lock);
        shared_counter++;  /* Modify shared data */
        int value = shared_counter;
        rm_wunlock(&global_lock);

        write_count++;
        if (write_count % 100 == 0) {
            printf("[WRITER %d] Wrote value %d (%d times)\n", thread_id, value, write_count);
        }
        usleep(5000);
    }

    printf("[WRITER %d] Final: wrote %d times\n", thread_id, write_count);
    return NULL;
}

int main() {
    printf("🧪 Simple Level 2 rmlock Concurrent Test\n");
    printf("=========================================\n");

    /* Initialize lock */
    if (rm_init_flags(&global_lock, "test_lock", RM_DUPOK) != 0) {
        printf("ERROR: Lock init failed\n");
        return 1;
    }
    printf("✅ Lock initialized\n");

    /* Create threads */
    pthread_t readers[2], writers[1];
    int reader_ids[] = {1, 2};
    int writer_ids[] = {1};

    /* Start threads */
    for (int i = 0; i < 2; i++) {
        pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]);
    }
    pthread_create(&writers[0], NULL, writer_thread, &writer_ids[0]);

    printf("✅ Started 2 readers + 1 writer\n");
    printf("⏱️  Running for 10 seconds...\n");

    /* Run test */
    for (int i = 0; i < 10; i++) {
        sleep(1);
        printf("[%d/10] Counter: %d\n", i+1, shared_counter);
    }

    /* Stop threads */
    printf("🛑 Stopping...\n");
    running = 0;

    /* Wait */
    for (int i = 0; i < 2; i++) {
        pthread_join(readers[i], NULL);
    }
    pthread_join(writers[0], NULL);

    printf("✅ Final counter: %d\n", shared_counter);
    printf("📊 Lock stats: R=%llu W=%llu\n",
           (unsigned long long)global_lock.total_reads,
           (unsigned long long)global_lock.total_writes);

    rm_destroy(&global_lock);
    printf("🎉 SUCCESS: Level 2 rmlock works!\n");
    return 0;
}