/*
 * Debug Test for 262K Route Limit
 *
 * Comprehensive debugging to find the exact cause of the 2^18 limit
 */

#include "test_framework.h"
#include "compat_shim.h"
#include "radix.h"
#include <arpa/inet.h>

/* Global debugging counters */
static size_t g_malloc_count = 0;
static size_t g_malloc_bytes = 0;
static size_t g_free_count = 0;
static size_t g_routes_attempted = 0;
static size_t g_routes_successful = 0;

/* Override malloc for debugging */
void *debug_malloc(size_t size, int type, int flags) {
    (void)type; (void)flags;
    /* Use system malloc temporarily by undefining the macro */
    #undef malloc
    void *ptr = malloc(size);
    #define malloc(size, type, flags) bsd_malloc(size, type, flags)

    if (ptr) {
        g_malloc_count++;
        g_malloc_bytes += size;

        /* Log critical allocation milestones */
        if (g_malloc_count % 10000 == 0) {
            printf("[MEM] Allocs: %zu, Bytes: %.2f MB\n",
                   g_malloc_count, g_malloc_bytes / (1024.0 * 1024.0));
        }

        /* Alert near the 262K boundary */
        if (g_routes_attempted >= 260000 && g_routes_attempted <= 265000) {
            printf("[BOUNDARY] Route %zu, Allocs: %zu, Bytes: %.2f MB\n",
                   g_routes_attempted, g_malloc_count, g_malloc_bytes / (1024.0 * 1024.0));
        }
    } else {
        printf("❌ [MALLOC FAILED] at route %zu, alloc #%zu\n",
               g_routes_attempted, g_malloc_count);
    }
    return ptr;
}

void debug_free(void *ptr, int type) {
    (void)type;
    if (ptr) {
        g_free_count++;
        /* Use system free temporarily */
        #undef free
        free(ptr);
        #define free(ptr, type) bsd_free(ptr, type)
    }
}

/* Generate route with detailed logging */
static struct sockaddr_in *make_debug_route_key(uint32_t route_id, int should_log) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) {
        printf("❌ Key allocation failed for route %u\n", route_id);
        return NULL;
    }

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;

    /* Use the corrected logic from the fixed scale test */
    if (route_id < 65536) {  /* 2^16 routes max per /24 range */
        sa->sin_addr.s_addr = htonl(0x0A000000 | (route_id << 8));
    } else if (route_id < 131072) {  /* 65536 + 65536 (172.16.0.0/12 allows 4096 /24s) */
        uint32_t offset = route_id - 65536;
        /* 172.16.0.0/12 provides 172.16.0.0 to 172.31.255.0 */
        /* That's 16 * 256 = 4096 possible /24 networks */
        /* We need more space, so use 172.16.0.0/8 (entire 172.x.x.0) */
        uint32_t major = offset / 256;  /* 172.x part */
        uint32_t minor = offset % 256;  /* 172.x.y part */
        sa->sin_addr.s_addr = htonl(0xAC000000 | (major << 16) | (minor << 8));

        if (route_id == 69632) {
            printf("[DEBUG] route_id=%u, offset=%u, major=%u, minor=%u, result=172.%u.%u.0\n",
                   route_id, offset, major, minor, major, minor);
        }
    } else if (route_id < 196608) {  /* 131072 + 65536 (192.168.0.0/16) */
        uint32_t offset = route_id - 131072;
        /* Use 193.x.x.0/24 instead to avoid bit conflicts */
        uint32_t major = offset / 256;  /* 193.x part */
        uint32_t minor = offset % 256;  /* 193.x.y part */
        sa->sin_addr.s_addr = htonl(0xC1000000 | (major << 16) | (minor << 8));
    } else if (route_id < 262144) {  /* 196608 + 65536 (TEST-NET-3) */
        uint32_t offset = route_id - 196608;
        /* Use 204.x.x.0/24 to avoid bit conflicts */
        uint32_t major = offset / 256;  /* 204.x part */
        uint32_t minor = offset % 256;  /* 204.x.y part */
        sa->sin_addr.s_addr = htonl(0xCC000000 | (major << 16) | (minor << 8));
    } else {
        /* Continue with more ranges for higher route counts */
        uint32_t range = route_id / 65536;
        uint32_t offset = route_id % 65536;
        sa->sin_addr.s_addr = htonl(0xF0000000 | ((range & 0xFF) << 16) | (offset << 8));
    }

    if (should_log || (route_id == 4096) || (route_id == 69632)) {
        printf("[ROUTE] ID=%u, IP=%s", route_id, inet_ntoa(sa->sin_addr));
        if (route_id < 65536) {
            printf(" (Range 1, offset=%u)\n", route_id);
        } else if (route_id < 131072) {
            printf(" (Range 2, offset=%u)\n", route_id - 65536);
        } else {
            printf(" (Range 3+, offset=%u)\n", route_id - 131072);
        }
    }

    return sa;
}

static struct sockaddr_in *make_debug_route_mask(void) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) {
        printf("❌ Mask allocation failed\n");
        return NULL;
    }

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(0xFFFFFF00);  /* /24 mask */

    return sa;
}

/* Check if route already exists */
static int route_exists(struct radix_node_head *rnh, struct sockaddr_in *key, struct sockaddr_in *mask) {
    struct radix_node *existing = rnh->rnh_lookup ?
        rnh->rnh_lookup(key, mask, &rnh->rh) : NULL;
    return existing != NULL;
}

/* Test the 262K limit with comprehensive logging */
static int test_262k_limit_debug(void) {
    printf("=== 262K Limit Debug Test ===\n");
    printf("Testing exactly where route addition fails...\n\n");

    /* Replace bsd_malloc/bsd_free temporarily */
    #undef bsd_malloc
    #undef bsd_free
    #define bsd_malloc(size, type, flags) debug_malloc(size, type, flags)
    #define bsd_free(ptr, type) debug_free(ptr, type)

    struct radix_node_head *rnh = NULL;
    int offset = offsetof(struct sockaddr_in, sin_addr);

    if (rn_inithead((void **)&rnh, offset) != 1) {
        printf("❌ rn_inithead failed\n");
        return -1;
    }

    printf("✅ Radix tree initialized\n");
    printf("Starting route addition test to 300K routes...\n\n");

    /* Test route addition up to 300K */
    for (uint32_t i = 0; i < 300000; i++) {
        g_routes_attempted = i + 1;

        struct sockaddr_in *key = make_debug_route_key(i,
            /* Log details near failure */ (i >= 262140 && i <= 262150));
        struct sockaddr_in *mask = make_debug_route_mask();

        if (!key || !mask) {
            printf("❌ Memory allocation failed at route %u\n", i);
            if (key) bsd_free(key, M_RTABLE);
            if (mask) bsd_free(mask, M_RTABLE);
            break;
        }

        /* Check if this route already exists (duplicate detection) */
        if (route_exists(rnh, key, mask)) {
            printf("❌ DUPLICATE ROUTE at ID %u: %s/24\n", i, inet_ntoa(key->sin_addr));
            printf("   Route ID %u should generate: ", i);
            if (i < 65536) {
                printf("Range 1 (10.x.x.0), offset=%u\n", i);
            } else if (i < 131072) {
                printf("Range 2 (172.16.x.0), offset=%u\n", i - 65536);
            } else {
                printf("Range 3+, offset=%u\n", i - 131072);
            }
            printf("   This proves route generation is repeating!\n");
            bsd_free(key, M_RTABLE);
            bsd_free(mask, M_RTABLE);
            break;
        }

        /* Allocate radix nodes */
        struct radix_node *nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);
        if (!nodes) {
            printf("❌ Node allocation failed at route %u\n", i);
            bsd_free(key, M_RTABLE);
            bsd_free(mask, M_RTABLE);
            break;
        }

        /* Try to add the route */
        struct radix_node *rn = rnh->rnh_addaddr(key, mask, &rnh->rh, nodes);

        if (rn) {
            g_routes_successful++;

            /* Progress indicators */
            if ((i + 1) % 10000 == 0) {
                printf("✅ Routes added: %u/%u (%.1f%%)\n",
                       i + 1, 300000, ((i + 1) * 100.0) / 300000);
            }
        } else {
            /* FAILURE - This is what we want to debug! */
            printf("\n🔴 ROUTE ADDITION FAILED!\n");
            printf("Route ID: %u\n", i);
            printf("Route IP: %s/24\n", inet_ntoa(key->sin_addr));
            printf("Routes attempted: %zu\n", g_routes_attempted);
            printf("Routes successful: %zu\n", g_routes_successful);
            printf("Memory allocs: %zu\n", g_malloc_count);
            printf("Memory bytes: %.2f MB\n", g_malloc_bytes / (1024.0 * 1024.0));

            /* Check if it's a duplicate by searching manually */
            struct radix_node *existing = rnh->rnh_matchaddr(key, &rnh->rh);
            if (existing) {
                printf("🔍 Conflicting route found: %p\n", (void*)existing);
            } else {
                printf("🔍 No conflicting route - likely internal limit\n");
            }

            /* Try to understand why it failed */
            printf("\n🔍 Failure Analysis:\n");

            /* Test if we can still allocate memory */
            #undef malloc
            #undef free
            void *test_alloc = malloc(1024);
            if (test_alloc) {
                printf("  ✅ System malloc still works\n");
                free(test_alloc);
            } else {
                printf("  ❌ System malloc failed - memory exhaustion!\n");
            }
            #define malloc(size, type, flags) bsd_malloc(size, type, flags)
            #define free(ptr, type) bsd_free(ptr, type)

            /* Test with a different route */
            struct sockaddr_in *test_key = make_debug_route_key(i + 100000, 1);
            struct sockaddr_in *test_mask = make_debug_route_mask();
            struct radix_node *test_nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);

            if (test_key && test_mask && test_nodes) {
                struct radix_node *test_rn = rnh->rnh_addaddr(test_key, test_mask, &rnh->rh, test_nodes);
                if (test_rn) {
                    printf("  ✅ Different route (+100K offset) works - confirms pattern issue\n");
                } else {
                    printf("  ❌ Different route also fails - likely internal limit\n");
                }
                if (test_key) bsd_free(test_key, M_RTABLE);
                if (test_mask) bsd_free(test_mask, M_RTABLE);
            }

            /* Clean up failed attempt */
            bsd_free(nodes, M_RTABLE);
            bsd_free(key, M_RTABLE);
            bsd_free(mask, M_RTABLE);
            break;
        }
    }

    printf("\n=== Final Statistics ===\n");
    printf("Routes attempted: %zu\n", g_routes_attempted);
    printf("Routes successful: %zu\n", g_routes_successful);
    printf("Success rate: %.2f%%\n", (g_routes_successful * 100.0) / g_routes_attempted);
    printf("Memory allocations: %zu\n", g_malloc_count);
    printf("Memory freed: %zu\n", g_free_count);
    printf("Peak memory usage: %.2f MB\n", g_malloc_bytes / (1024.0 * 1024.0));

    /* Clean up */
    rn_detachhead((void **)&rnh);

    return (g_routes_successful >= 262144) ? 0 : -1;
}

/* Test suite */
static test_case_t debug_262k_tests[] = {
    TEST_CASE(262k_limit_debug,
              "Debug the exact cause of 262K route limit",
              test_262k_limit_debug),
    TEST_SUITE_END()
};

test_suite_t debug_262k_test_suite = {
    "262K Limit Debug Tests",
    "Comprehensive debugging for the 2^18 route limit",
    debug_262k_tests,
    0,  /* num_tests calculated at runtime */
    NULL, /* setup */
    NULL  /* teardown */
};

/* Main test runner */
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("FreeBSD Radix Tree 262K Limit Debug\n");
    printf("===================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Initialize compatibility layer */
    kernel_compat_init();

    /* Count tests */
    int count = 0;
    while (debug_262k_tests[count].name != NULL) {
        count++;
    }
    debug_262k_test_suite.num_tests = count;

    /* Run the test suite */
    int result = test_run_suite(&debug_262k_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    return (result == 0 && g_test_result.failed_tests == 0) ? 0 : 1;
}