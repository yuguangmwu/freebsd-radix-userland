/*
 * FreeBSD Kernel Compatibility Layer Implementation
 */

#include <stdarg.h>
#include <time.h>
#include <string.h>  /* for memcmp */
/* Include standard library functions before our redefinitions */
#include <stdlib.h>

/* Store standard library functions before we include our header */
static void* (*std_malloc)(size_t) = malloc;
static void (*std_free)(void*) = free;

#include "kernel_compat.h"

/* Global variables */
int maxfib = 16;
volatile time_t time_second;
epoch_t net_epoch_preempt = NULL;

/* Memory management - use standard library directly to avoid macro conflicts */
void* user_malloc(size_t size, int flags) {
    void* ptr;

    if (flags & M_ZERO) {
        ptr = calloc(1, size);
    } else {
        ptr = std_malloc(size);  /* Use saved standard malloc */
    }

    if (!ptr && (flags & M_WAITOK)) {
        fprintf(stderr, "malloc failed: out of memory\n");
        abort();
    }

    return ptr;
}

void user_free(void* ptr) {
    std_free(ptr);  /* Use saved standard free */
}

/* Printf implementation */
int user_printf(const char* fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    return ret;
}

/* Socket address comparison */
int sa_equal(const struct sockaddr* a, const struct sockaddr* b) {
    if (a == NULL || b == NULL)
        return (a == b);

    if (a->sa_len != b->sa_len)
        return 0;

    return (memcmp(a, b, a->sa_len) == 0);
}

/* Time initialization */
void time_init(void) {
    time_second = time(NULL);
}

/* Kernel compatibility initialization */
void kernel_compat_init(void) {
    /* Initialize time subsystem */
    time_init();

    /* Initialize epoch subsystem (no-op in userland) */
    net_epoch_preempt = NULL;

    /* Set maximum FIB number */
    maxfib = 16;

    printf("[KERNEL_COMPAT] Initialized kernel compatibility layer\n");
}