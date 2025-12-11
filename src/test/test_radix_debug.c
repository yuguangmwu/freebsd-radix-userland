/*
 * Radix Debug Test
 *
 * Investigates the multiple route addition issue with FreeBSD radix code
 */

#include "test_framework.h"
#include "compat_shim.h"
#include "radix.h"  /* Use FreeBSD radix.h directly for full structure access */
#include <arpa/inet.h>

/* Debug helpers */
static const char *debug_sockaddr_in(struct sockaddr_in *sa) {
    static char buf[64];
    if (!sa) return "NULL";

    char addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sa->sin_addr, addr, sizeof(addr));
    snprintf(buf, sizeof(buf), "%s (len=%d, fam=%d)",
             addr, sa->sin_len, sa->sin_family);
    return buf;
}

static int debug_prefixlen(struct sockaddr_in *mask) {
    if (!mask) return -1;
    uint32_t m = ntohl(mask->sin_addr.s_addr);
    return __builtin_popcount(m);
}

static void debug_radix_state(struct radix_node_head *rnh) {
    printf("\n=== Radix Tree State ===\n");
    printf("Head pointer: %p\n", (void*)rnh);

    if (!rnh) {
        printf("Head is NULL\n");
        return;
    }

    printf("rh.rnh_treetop: %p\n", (void*)rnh->rh.rnh_treetop);
    printf("rh.rnh_masks: %p\n", (void*)rnh->rh.rnh_masks);
    printf("rnh_addaddr: %p\n", (void*)rnh->rnh_addaddr);
    printf("rnh_matchaddr: %p\n", (void*)rnh->rnh_matchaddr);
    printf("rnh_deladdr: %p\n", (void*)rnh->rnh_deladdr);
    printf("rnh_walktree: %p\n", (void*)rnh->rnh_walktree);

    if (rnh->rh.rnh_treetop) {
        printf("Tree top node details:\n");
        struct radix_node *rn = rnh->rh.rnh_treetop;
        printf("  rn_flags: 0x%x\n", rn->rn_flags);
        printf("  rn_bit: %d\n", rn->rn_bit);
        printf("  rn_key: %p\n", (void*)rn->rn_key);
        printf("  rn_mask: %p\n", (void*)rn->rn_mask);
    } else {
        printf("Tree is empty\n");
    }
    printf("========================\n\n");
}

/* Tree walk callback function */
static int count_nodes(struct radix_node *rn, void *arg) {
    int *counter = (int*)arg;
    (*counter)++;
    printf("  Walked node: %p (flags=0x%x)\n", (void*)rn, rn->rn_flags);
    return 0;  /* Continue walking */
}
static struct radix_node *debug_rn_addroute(void *key, void *mask,
                                           struct radix_node_head *rnh,
                                           const char *route_desc) {
    struct sockaddr_in *sa_key = (struct sockaddr_in *)key;
    struct sockaddr_in *sa_mask = (struct sockaddr_in *)mask;

    printf("[rn_addroute] Adding %s\n", route_desc);
    printf("  key=%s key_ptr=%p\n", debug_sockaddr_in(sa_key), key);
    if (sa_mask) {
        printf("  mask=%s mask_ptr=%p prefixlen=%d\n",
               debug_sockaddr_in(sa_mask), mask, debug_prefixlen(sa_mask));
    } else {
        printf("  mask=NULL\n");
    }

    /* Allocate nodes for FreeBSD radix tree */
    struct radix_node *nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);
    if (!nodes) {
        printf("  ERROR: Failed to allocate nodes\n");
        return NULL;
    }

    struct radix_node *rn = rnh->rnh_addaddr(key, mask, &rnh->rh, nodes);

    printf("  -> result=%p\n", (void*)rn);
    if (!rn) {
        printf("  ERROR: rn_addaddr returned NULL\n");
        bsd_free(nodes, M_RTABLE);
    } else {
        printf("  SUCCESS: Node added with flags=0x%x, bit=%d\n",
               rn->rn_flags, rn->rn_bit);
    }
    printf("\n");

    return rn;
}

/* Create properly allocated and initialized sockaddr_in */
static struct sockaddr_in *make_key_v4(const char *addr_str) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) return NULL;

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;

    if (inet_pton(AF_INET, addr_str, &sa->sin_addr) != 1) {
        bsd_free(sa, M_RTABLE);
        return NULL;
    }

    return sa;
}

static struct sockaddr_in *make_mask_v4(int prefixlen) {
    if (prefixlen < 0 || prefixlen > 32) return NULL;

    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) return NULL;

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;

    if (prefixlen == 0) {
        sa->sin_addr.s_addr = 0;
    } else {
        sa->sin_addr.s_addr = htonl(~((1U << (32 - prefixlen)) - 1));
    }

    return sa;
}

static int test_multiple_routes_debug(void) {
    printf("=== Testing Multiple Route Addition (Debug) ===\n");

    /* Initialize with correct offset */
    struct radix_node_head *rnh = NULL;
    int offset = offsetof(struct sockaddr_in, sin_addr);
    printf("Using offset: %d (should be %zu for sin_addr)\n", offset, offsetof(struct sockaddr_in, sin_addr));

    if (rn_inithead((void **)&rnh, offset) != 1) {
        printf("FAIL: rn_inithead failed\n");
        return -1;
    }

    printf("SUCCESS: rn_inithead returned rnh=%p\n", (void*)rnh);
    debug_radix_state(rnh);

    /* Route 1: 10.0.0.0/24 */
    printf("\n[1] Adding first route: 10.0.0.0/24\n");
    struct sockaddr_in *key1 = make_key_v4("10.0.0.0");
    struct sockaddr_in *mask1 = make_mask_v4(24);

    if (!key1 || !mask1) {
        printf("FAIL: Could not create key1/mask1\n");
        return -1;
    }

    struct radix_node *rn1 = debug_rn_addroute(key1, mask1, rnh, "10.0.0.0/24");
    debug_radix_state(rnh);

    if (!rn1) {
        printf("FAIL: First route addition failed\n");
        return -1;
    }

    /* Route 2: 192.168.1.0/24 (completely different prefix) */
    printf("\n[2] Adding second route: 192.168.1.0/24\n");
    struct sockaddr_in *key2 = make_key_v4("192.168.1.0");
    struct sockaddr_in *mask2 = make_mask_v4(24);

    if (!key2 || !mask2) {
        printf("FAIL: Could not create key2/mask2\n");
        return -1;
    }

    struct radix_node *rn2 = debug_rn_addroute(key2, mask2, rnh, "192.168.1.0/24");
    debug_radix_state(rnh);

    if (!rn2) {
        printf("FAIL: Second route addition failed\n");
        return -1;
    }

    /* Route 3: 10.1.0.0/16 (same first octet as route 1, different mask) */
    printf("\n[3] Adding third route: 10.1.0.0/16\n");
    struct sockaddr_in *key3 = make_key_v4("10.1.0.0");
    struct sockaddr_in *mask3 = make_mask_v4(16);

    if (!key3 || !mask3) {
        printf("FAIL: Could not create key3/mask3\n");
        return -1;
    }

    struct radix_node *rn3 = debug_rn_addroute(key3, mask3, rnh, "10.1.0.0/16");
    debug_radix_state(rnh);

    if (!rn3) {
        printf("FAIL: Third route addition failed\n");
        return -1;
    }

    /* Verification: Lookup tests */
    printf("\n[4] Verifying routes with lookups...\n");

    /* Test 1: 10.0.0.100 should match 10.0.0.0/24 */
    struct sockaddr_in *test1 = make_key_v4("10.0.0.100");
    struct radix_node *found1 = rnh->rnh_matchaddr(test1, &rnh->rh);
    printf("Lookup 10.0.0.100: %s\n", found1 ? "FOUND" : "NOT FOUND");

    /* Test 2: 192.168.1.50 should match 192.168.1.0/24 */
    struct sockaddr_in *test2 = make_key_v4("192.168.1.50");
    struct radix_node *found2 = rnh->rnh_matchaddr(test2, &rnh->rh);
    printf("Lookup 192.168.1.50: %s\n", found2 ? "FOUND" : "NOT FOUND");

    /* Test 3: 10.1.5.10 should match 10.1.0.0/16 */
    struct sockaddr_in *test3 = make_key_v4("10.1.5.10");
    struct radix_node *found3 = rnh->rnh_matchaddr(test3, &rnh->rh);
    printf("Lookup 10.1.5.10: %s\n", found3 ? "FOUND" : "NOT FOUND");

    /* Test tree walk count */
    printf("\n[5] Tree walk test...\n");
    int count = 0;
    int walk_result = rnh->rnh_walktree(&rnh->rh, count_nodes, &count);

    printf("Walk result: %d, found %d nodes\n", walk_result, count);
    printf("Expected: 3 routes + internal nodes\n");

    /* Cleanup */
    if (test1) bsd_free(test1, M_RTABLE);
    if (test2) bsd_free(test2, M_RTABLE);
    if (test3) bsd_free(test3, M_RTABLE);

    rn_detachhead((void **)&rnh);

    printf("\n=== Debug Test Summary ===\n");
    printf("Route 1 (10.0.0.0/24): %s\n", rn1 ? "SUCCESS" : "FAILED");
    printf("Route 2 (192.168.1.0/24): %s\n", rn2 ? "SUCCESS" : "FAILED");
    printf("Route 3 (10.1.0.0/16): %s\n", rn3 ? "SUCCESS" : "FAILED");
    printf("Lookups: %s %s %s\n",
           found1 ? "OK" : "FAIL",
           found2 ? "OK" : "FAIL",
           found3 ? "OK" : "FAIL");
    printf("Walk count: %d nodes\n", count);

    TEST_PASS();
}

/* Test suite definition */
static test_case_t radix_debug_tests[] = {
    TEST_CASE(multiple_routes_debug,
              "Debug multiple route addition issue",
              test_multiple_routes_debug),
    TEST_SUITE_END()
};

test_suite_t radix_debug_test_suite = {
    "Radix Debug Tests",
    "Debug suite for FreeBSD radix tree multiple route issue",
    radix_debug_tests,
    0,  /* num_tests calculated at runtime */
    NULL, /* setup */
    NULL  /* teardown */
};

/* Main test runner */
int main(int argc, char* argv[]) {
    (void)argc; (void)argv;  /* Suppress unused warnings */

    printf("FreeBSD Radix Tree Debug Test Suite\n");
    printf("==================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Initialize compatibility layer */
    kernel_compat_init();

    /* Count tests */
    int count = 0;
    while (radix_debug_tests[count].name != NULL) {
        count++;
    }
    radix_debug_test_suite.num_tests = count;

    /* Run the test suite */
    int result = test_run_suite(&radix_debug_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    if (result != 0 || g_test_result.failed_tests > 0) {
        printf("\n❌ Debug tests revealed issues.\n");
        return 1;
    }

    printf("\n✅ Debug tests completed successfully!\n");
    return 0;
}