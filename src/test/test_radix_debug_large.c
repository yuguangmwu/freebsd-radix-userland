/*
 * Radix Large-Scale Debug Test
 *
 * Debug why radix tree fails at large route counts (65K+ routes)
 */

#include "test_framework.h"
#include "compat_shim.h"
#include "radix.h"
#include <arpa/inet.h>

/* Tree walk callback for debugging */
static int debug_count_nodes(struct radix_node *rn, void *arg) {
    (void)rn;
    int *counter = (int*)arg;
    (*counter)++;
    return 0;
}
#define DEBUG_BATCH_SIZE 1000
#define DEBUG_MAX_ROUTES 70000  /* Test beyond the 65K failure point */

/* Detailed failure tracking */
typedef struct {
    int route_id;
    struct sockaddr_in key;
    struct sockaddr_in mask;
    int error_code;
    const char *error_msg;
} route_failure_t;

static route_failure_t failures[1000];  /* Track up to 1000 failures */
static int failure_count = 0;

/* Enhanced route creation with error checking */
static struct sockaddr_in *make_debug_key(int route_id) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) {
        printf("ERROR: malloc failed for route key %d\n", route_id);
        return NULL;
    }

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;

    /* Sequential pattern: 10.x.y.0 where x.y comes from route_id */
    uint32_t addr = 0x0A000000 | ((route_id & 0xFFFF) << 8);
    sa->sin_addr.s_addr = htonl(addr);

    return sa;
}

static struct sockaddr_in *make_debug_mask(void) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) {
        printf("ERROR: malloc failed for route mask\n");
        return NULL;
    }

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(0xFFFFFF00);  /* /24 mask */

    return sa;
}

/* Record failure details */
static void record_failure(int route_id, struct sockaddr_in *key, struct sockaddr_in *mask,
                          int error_code, const char *error_msg) {
    if (failure_count < 1000) {
        failures[failure_count].route_id = route_id;
        if (key) failures[failure_count].key = *key;
        if (mask) failures[failure_count].mask = *mask;
        failures[failure_count].error_code = error_code;
        failures[failure_count].error_msg = error_msg;
        failure_count++;
    }
}

/* Check radix tree internal state */
static void debug_tree_state(struct radix_node_head *rnh, int route_count, const char *phase) {
    printf("\n=== Tree State Debug (%s, %d routes) ===\n", phase, route_count);

    if (!rnh) {
        printf("❌ Tree head is NULL\n");
        return;
    }

    printf("Tree head: %p\n", (void*)rnh);
    printf("rh.rnh_treetop: %p\n", (void*)rnh->rh.rnh_treetop);
    printf("rh.rnh_masks: %p\n", (void*)rnh->rh.rnh_masks);

    /* Check function pointers are still valid */
    printf("Function pointers:\n");
    printf("  rnh_addaddr: %p %s\n", (void*)rnh->rnh_addaddr,
           rnh->rnh_addaddr ? "✓" : "❌ NULL");
    printf("  rnh_matchaddr: %p %s\n", (void*)rnh->rnh_matchaddr,
           rnh->rnh_matchaddr ? "✓" : "❌ NULL");
    printf("  rnh_deladdr: %p %s\n", (void*)rnh->rnh_deladdr,
           rnh->rnh_deladdr ? "✓" : "❌ NULL");

    /* Count nodes in tree */
    int node_count = 0;
    if (rnh->rnh_walktree) {
        rnh->rnh_walktree(&rnh->rh, debug_count_nodes, &node_count);
        printf("Actual nodes in tree: %d\n", node_count);
    } else {
        printf("❌ rnh_walktree is NULL\n");
    }

    printf("========================================\n\n");
}

/* Test memory allocation patterns */
static void debug_memory_allocation(int num_routes) {
    printf("=== Memory Allocation Debug ===\n");

    /* Test if we can allocate the expected amount of memory */
    size_t key_size = sizeof(struct sockaddr_in);
    size_t mask_size = sizeof(struct sockaddr_in);
    size_t node_size = sizeof(struct radix_node);

    printf("Memory requirements for %d routes:\n", num_routes);
    printf("  Keys: %zu bytes (%zu per route)\n", num_routes * key_size, key_size);
    printf("  Masks: %zu bytes (%zu per route)\n", num_routes * mask_size, mask_size);
    printf("  Nodes: %zu bytes (%zu per route)\n", num_routes * 2 * node_size, 2 * node_size);
    printf("  Total: %zu bytes (%.2f MB)\n",
           num_routes * (key_size + mask_size + 2 * node_size),
           (num_routes * (key_size + mask_size + 2 * node_size)) / (1024.0 * 1024.0));

    /* Test bulk allocation */
    printf("Testing bulk memory allocation...\n");
    void **test_ptrs = calloc(num_routes, sizeof(void*));
    int alloc_success = 0;

    for (int i = 0; i < num_routes; i++) {
        test_ptrs[i] = bsd_malloc(key_size, M_RTABLE, M_WAITOK | M_ZERO);
        if (test_ptrs[i]) {
            alloc_success++;
        } else {
            printf("❌ Memory allocation failed at route %d\n", i);
            break;
        }
    }

    printf("Memory allocation test: %d/%d successful\n", alloc_success, num_routes);

    /* Clean up */
    for (int i = 0; i < alloc_success; i++) {
        if (test_ptrs[i]) {
            bsd_free(test_ptrs[i], M_RTABLE);
        }
    }
    #undef free
    free(test_ptrs);
    #define free(ptr, type) bsd_free(ptr, type)

    printf("===============================\n\n");
}

/* Debug the exact failure point */
static int debug_large_scale_failure(void) {
    printf("=== Large Scale Failure Debug ===\n");
    printf("Testing up to %d routes to find exact failure point...\n\n", DEBUG_MAX_ROUTES);

    perf_timer_t timer;
    struct radix_node_head *rnh = NULL;
    int successful_adds = 0;
    int last_success_batch = -1;

    /* Initialize radix tree */
    int offset = offsetof(struct sockaddr_in, sin_addr);
    if (rn_inithead((void **)&rnh, offset) != 1) {
        printf("❌ FAIL: rn_inithead failed\n");
        return -1;
    }

    debug_tree_state(rnh, 0, "Initial");
    debug_memory_allocation(DEBUG_MAX_ROUTES);

    /* Add routes in batches with detailed monitoring */
    PERF_START(&timer);

    for (int i = 0; i < DEBUG_MAX_ROUTES; i++) {
        struct sockaddr_in *key = make_debug_key(i);
        struct sockaddr_in *mask = make_debug_mask();

        if (!key || !mask) {
            record_failure(i, key, mask, -1, "Memory allocation failed");
            if (key) bsd_free(key, M_RTABLE);
            if (mask) bsd_free(mask, M_RTABLE);
            printf("❌ Route %d: Memory allocation failed\n", i);
            break;
        }

        /* Allocate radix nodes */
        struct radix_node *nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);
        if (!nodes) {
            record_failure(i, key, mask, -2, "Node allocation failed");
            bsd_free(key, M_RTABLE);
            bsd_free(mask, M_RTABLE);
            printf("❌ Route %d: Node allocation failed\n", i);
            break;
        }

        /* Try to add the route */
        struct radix_node *rn = rnh->rnh_addaddr(key, mask, &rnh->rh, nodes);

        if (rn) {
            successful_adds++;

            /* Batch progress reporting */
            if ((i + 1) % DEBUG_BATCH_SIZE == 0) {
                printf("✅ Batch %d: Added routes %d-%d (total: %d)\n",
                       (i + 1) / DEBUG_BATCH_SIZE, i - DEBUG_BATCH_SIZE + 1, i, successful_adds);
                last_success_batch = (i + 1) / DEBUG_BATCH_SIZE;

                /* Debug tree state every 10K routes */
                if ((i + 1) % 10000 == 0) {
                    debug_tree_state(rnh, successful_adds, "Mid-test");
                }
            }
        } else {
            /* Route addition failed - this is where we want to debug! */
            record_failure(i, key, mask, 0, "rnh_addaddr returned NULL");

            printf("❌ FIRST FAILURE at route %d (after %d successful)\n", i, successful_adds);
            printf("   Route: %s/24\n", inet_ntoa(key->sin_addr));
            printf("   Last successful batch: %d\n", last_success_batch);

            /* Detailed debugging at failure point */
            debug_tree_state(rnh, successful_adds, "At failure");

            /* Try to understand why it failed */
            printf("\n=== Failure Analysis ===\n");

            /* Check if it's a duplicate */
            struct radix_node *existing = rnh->rnh_matchaddr(key, &rnh->rh);
            if (existing) {
                printf("🔍 Route matches existing entry: %p\n", (void*)existing);

                /* Check if it's exact match */
                struct radix_node *exact = rnh->rnh_lookup ?
                    rnh->rnh_lookup(key, mask, &rnh->rh) : NULL;
                if (exact) {
                    printf("🔍 Exact duplicate found: %p\n", (void*)exact);
                }
            } else {
                printf("🔍 No matching route found - new route should be addable\n");
            }

            /* Check tree consistency */
            int final_count = 0;
            if (rnh->rnh_walktree) {
                rnh->rnh_walktree(&rnh->rh, debug_count_nodes, &final_count);
                printf("🔍 Tree walk count: %d (expected: %d)\n", final_count, successful_adds);
            }

            /* Clean up failed attempt */
            bsd_free(nodes, M_RTABLE);
            bsd_free(key, M_RTABLE);
            bsd_free(mask, M_RTABLE);

            break;  /* Stop at first failure for analysis */
        }
    }

    PERF_END(&timer);

    printf("\n=== Final Results ===\n");
    printf("Successfully added: %d/%d routes\n", successful_adds, DEBUG_MAX_ROUTES);
    printf("Total time: %.2fms\n", timer.elapsed_ms);
    printf("Rate: %.2f routes/ms\n", successful_adds / (timer.elapsed_ms + 0.001));
    printf("Failures recorded: %d\n", failure_count);

    /* Print failure details */
    if (failure_count > 0) {
        printf("\n=== Failure Details ===\n");
        for (int i = 0; i < min(10, failure_count); i++) {
            printf("Failure %d: Route %d - %s\n",
                   i + 1, failures[i].route_id, failures[i].error_msg);
        }
        if (failure_count > 10) {
            printf("... and %d more failures\n", failure_count - 10);
        }
    }

    debug_tree_state(rnh, successful_adds, "Final");

    /* Cleanup */
    rn_detachhead((void **)&rnh);

    return successful_adds >= DEBUG_MAX_ROUTES * 0.95 ? 0 : -1;
}

/* Test suite definition */
static test_case_t radix_debug_large_tests[] = {
    TEST_CASE(large_scale_failure,
              "Debug large-scale route addition failures",
              debug_large_scale_failure),
    TEST_SUITE_END()
};

test_suite_t radix_debug_large_test_suite = {
    "Radix Large-Scale Debug Tests",
    "Debug suite for large-scale route addition failures",
    radix_debug_large_tests,
    0,  /* num_tests calculated at runtime */
    NULL, /* setup */
    NULL  /* teardown */
};

/* Main test runner */
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    printf("FreeBSD Radix Tree Large-Scale Debug Suite\n");
    printf("==========================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Initialize compatibility layer */
    kernel_compat_init();

    /* Count tests */
    int count = 0;
    while (radix_debug_large_tests[count].name != NULL) {
        count++;
    }
    radix_debug_large_test_suite.num_tests = count;

    /* Run the test suite */
    int result = test_run_suite(&radix_debug_large_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    return (result == 0 && g_test_result.failed_tests == 0) ? 0 : 1;
}