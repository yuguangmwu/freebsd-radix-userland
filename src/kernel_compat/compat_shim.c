/*
 * Kernel Compatibility Implementation
 */

#include "compat_shim.h"

/* Global variables */
int maxfib = 16;
volatile time_t time_second;
epoch_t net_epoch_preempt = NULL;
struct thread* curthread = NULL;

/* Initialize kernel compatibility layer */
void kernel_compat_init(void) {
    time_second = time(NULL);
}

/* Socket address comparison utility */
int sa_equal(const struct sockaddr* a, const struct sockaddr* b) {
    if (a == NULL || b == NULL)
        return (a == b);

    if (a->sa_len != b->sa_len)
        return 0;

    return (memcmp(a, b, a->sa_len) == 0);
}