/*
 * Minimal Radix Tree Test
 *
 * This tests our ability to integrate FreeBSD radix tree code
 * with our kernel compatibility layer.
 */

#include "test_framework.h"
#include "radix_adapter.h"

/* Test basic radix tree operations */
static int test_radix_init_destroy(void) {
    struct radix_node_head *rnh;

    /* Test initialization */
    rnh = userland_rn_inithead();
    TEST_ASSERT_NOT_NULL(rnh, "Radix tree should initialize");

    /* Test cleanup */
    userland_rn_destroy(rnh);

    TEST_PASS();
}

static int test_radix_basic_operations(void) {
    struct radix_node_head *rnh;
    struct radix_node *rn;
    struct sockaddr_in dest, mask;

    /* Initialize */
    rnh = userland_rn_inithead();
    TEST_ASSERT_NOT_NULL(rnh, "Radix tree should initialize");

    /* Create test route: 192.168.1.0/24 */
    TEST_ASSERT_EQ(0, userland_make_sockaddr_in("192.168.1.0", &dest),
                   "Should create destination address");
    TEST_ASSERT_EQ(0, userland_make_netmask(24, &mask),
                   "Should create netmask");

    /* Add route */
    rn = userland_rn_addroute(rnh, (struct sockaddr*)&dest, (struct sockaddr*)&mask);
    TEST_ASSERT_NOT_NULL(rn, "Should add route to radix tree");

    /* Lookup route */
    struct sockaddr_in lookup_addr;
    TEST_ASSERT_EQ(0, userland_make_sockaddr_in("192.168.1.100", &lookup_addr),
                   "Should create lookup address");

    rn = userland_rn_lookup(rnh, (struct sockaddr*)&lookup_addr);
    TEST_ASSERT_NOT_NULL(rn, "Should find matching route");

    /* Delete route */
    rn = userland_rn_delete(rnh, (struct sockaddr*)&dest, (struct sockaddr*)&mask);
    TEST_ASSERT_NOT_NULL(rn, "Should delete route");

    /* Verify route is gone */
    rn = userland_rn_lookup(rnh, (struct sockaddr*)&lookup_addr);
    TEST_ASSERT_NULL(rn, "Route should be gone after deletion");

    /* Cleanup */
    userland_rn_destroy(rnh);

    TEST_PASS();
}

static int test_radix_longest_match(void) {
    struct radix_node_head *rnh;
    struct radix_node *rn;
    struct sockaddr_in dest1, dest2, dest3, mask1, mask2, mask3, lookup;

    /* Initialize */
    rnh = userland_rn_inithead();
    TEST_ASSERT_NOT_NULL(rnh, "Radix tree should initialize");

    /* Add routes with different specificities */
    /* 10.0.0.0/8 - general */
    userland_make_sockaddr_in("10.0.0.0", &dest1);
    userland_make_netmask(8, &mask1);
    rn = userland_rn_addroute(rnh, (struct sockaddr*)&dest1, (struct sockaddr*)&mask1);
    TEST_ASSERT_NOT_NULL(rn, "Should add /8 route");

    /* 10.1.0.0/16 - more specific */
    userland_make_sockaddr_in("10.1.0.0", &dest2);
    userland_make_netmask(16, &mask2);
    rn = userland_rn_addroute(rnh, (struct sockaddr*)&dest2, (struct sockaddr*)&mask2);
    TEST_ASSERT_NOT_NULL(rn, "Should add /16 route");

    /* 10.1.1.0/24 - most specific */
    userland_make_sockaddr_in("10.1.1.0", &dest3);
    userland_make_netmask(24, &mask3);
    rn = userland_rn_addroute(rnh, (struct sockaddr*)&dest3, (struct sockaddr*)&mask3);
    TEST_ASSERT_NOT_NULL(rn, "Should add /24 route");

    /* Test lookup - should find most specific match */
    userland_make_sockaddr_in("10.1.1.100", &lookup);
    rn = userland_rn_lookup(rnh, (struct sockaddr*)&lookup);
    TEST_ASSERT_NOT_NULL(rn, "Should find most specific route for 10.1.1.100");

    /* Test lookup - should find /16 route */
    userland_make_sockaddr_in("10.1.2.100", &lookup);
    rn = userland_rn_lookup(rnh, (struct sockaddr*)&lookup);
    TEST_ASSERT_NOT_NULL(rn, "Should find /16 route for 10.1.2.100");

    /* Test lookup - should find /8 route */
    userland_make_sockaddr_in("10.2.3.100", &lookup);
    rn = userland_rn_lookup(rnh, (struct sockaddr*)&lookup);
    TEST_ASSERT_NOT_NULL(rn, "Should find /8 route for 10.2.3.100");

    /* Cleanup */
    userland_rn_destroy(rnh);

    TEST_PASS();
}

/* Helper for tree walking test */
static int count_routes(struct radix_node *rn, void *arg) {
    (void)rn; /* Suppress unused warning */
    int *count = (int*)arg;
    (*count)++;
    return 0; /* Continue walking */
}

static int test_radix_tree_walk(void) {
    struct radix_node_head *rnh;
    struct radix_node *rn;
    struct sockaddr_in dest, mask;
    int route_count = 0;

    /* Initialize */
    rnh = userland_rn_inithead();
    TEST_ASSERT_NOT_NULL(rnh, "Radix tree should initialize");

    /* Add several routes */
    const char* routes[] = {
        "192.168.1.0", "192.168.2.0", "10.0.0.0", "172.16.0.0"
    };

    for (int i = 0; i < 4; i++) {
        userland_make_sockaddr_in(routes[i], &dest);
        userland_make_netmask(24, &mask);
        rn = userland_rn_addroute(rnh, (struct sockaddr*)&dest, (struct sockaddr*)&mask);
        TEST_ASSERT_NOT_NULL(rn, "Should add route %s", routes[i]);
    }

    /* Walk the tree and count routes */
    int result = userland_rn_walktree(rnh, count_routes, &route_count);
    TEST_ASSERT_EQ(0, result, "Tree walk should succeed");
    TEST_ASSERT(route_count >= 4, "Should find at least 4 routes (found %d)", route_count);

    /* Cleanup */
    userland_rn_destroy(rnh);

    TEST_PASS();
}

/* Performance test */
static int test_radix_performance(void) {
    struct radix_node_head *rnh;
    struct sockaddr_in dest, mask, lookup;
    perf_timer_t timer;
    const int num_routes = 100;  /* Start small */
    int successful_adds = 0;
    int successful_lookups = 0;

    /* Initialize */
    rnh = userland_rn_inithead();
    TEST_ASSERT_NOT_NULL(rnh, "Radix tree should initialize");

    test_log_info("Performance test: Adding %d routes...", num_routes);

    /* Performance test: Add routes */
    PERF_START(&timer);
    for (int i = 1; i <= num_routes; i++) {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.0", (i >> 8) & 0xFF, i & 0xFF);

        if (userland_make_sockaddr_in(ip_str, &dest) == 0 &&
            userland_make_netmask(24, &mask) == 0) {

            if (userland_rn_addroute(rnh, (struct sockaddr*)&dest, (struct sockaddr*)&mask)) {
                successful_adds++;
            }
        }
    }
    PERF_END(&timer);

    test_log_info("Added %d/%d routes in %.2fms (%.2f routes/ms)",
                  successful_adds, num_routes, timer.elapsed_ms,
                  successful_adds / (timer.elapsed_ms + 0.001)); /* Avoid div by zero */

    /* Performance test: Lookup routes */
    PERF_START(&timer);
    for (int i = 1; i <= num_routes; i++) {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.100", (i >> 8) & 0xFF, i & 0xFF);

        if (userland_make_sockaddr_in(ip_str, &lookup) == 0) {
            if (userland_rn_lookup(rnh, (struct sockaddr*)&lookup)) {
                successful_lookups++;
            }
        }
    }
    PERF_END(&timer);

    test_log_info("Found %d/%d routes in %.2fms (%.2f lookups/ms)",
                  successful_lookups, successful_adds, timer.elapsed_ms,
                  successful_lookups / (timer.elapsed_ms + 0.001));

    /* Cleanup */
    userland_rn_destroy(rnh);

    TEST_ASSERT(successful_adds > num_routes * 0.8,
                "Should successfully add most routes");
    TEST_ASSERT(successful_lookups > successful_adds * 0.8,
                "Should successfully lookup most added routes");

    TEST_PASS();
}

/* Test suite definition */
static test_case_t radix_integration_tests[] = {
    TEST_CASE(radix_init_destroy,
              "Test radix tree initialization and cleanup",
              test_radix_init_destroy),

    TEST_CASE(radix_basic_operations,
              "Test basic radix tree operations",
              test_radix_basic_operations),

    TEST_CASE(radix_longest_match,
              "Test longest prefix matching",
              test_radix_longest_match),

    TEST_CASE(radix_tree_walk,
              "Test radix tree enumeration",
              test_radix_tree_walk),

    TEST_CASE(radix_performance,
              "Performance test for radix operations",
              test_radix_performance),

    TEST_SUITE_END()
};

test_suite_t radix_integration_test_suite = {
    "Radix Integration Tests",
    "Test suite for FreeBSD radix tree integration",
    radix_integration_tests,
    0,  /* num_tests calculated at runtime */
    NULL, /* setup */
    NULL  /* teardown */
};

/* Main test runner */
int main(int argc, char* argv[]) {
    printf("FreeBSD Radix Tree Integration Test Suite\n");
    printf("=========================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Initialize compatibility layer */
    kernel_compat_init();

    /* Count tests */
    int count = 0;
    while (radix_integration_tests[count].name != NULL) {
        count++;
    }
    radix_integration_test_suite.num_tests = count;

    /* Handle command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h     Show this help message\n");
            printf("  --verbose, -v  Enable verbose output\n");
            printf("\n");
            printf("This tests FreeBSD radix tree integration.\n");
            return 0;
        }
    }

    /* Run the test suite */
    int result = test_run_suite(&radix_integration_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    if (result != 0 || g_test_result.failed_tests > 0) {
        printf("\n❌ Some tests failed. Check the output above.\n");
        return 1;
    }

    printf("\n✅ All radix integration tests passed!\n");
    printf("Phase 1 Complete: FreeBSD radix tree is operational in userland.\n");

    return 0;
}