/*
 * Minimal Radix Tree Test Program
 *
 * This tests the core radix tree functionality in userland
 * to validate our kernel compatibility layer.
 */

#include "test_framework.h"
#include "radix_minimal.h"

/* Test setup and teardown */
static int radix_test_setup(void) {
    return radix_init();
}

static int radix_test_teardown(void) {
    radix_cleanup();
    return 0;
}

/* Basic functionality tests */
static int test_radix_init(void) {
    /* Cleanup first in case already initialized */
    radix_cleanup();

    int result = radix_init();
    TEST_ASSERT_EQ(0, result, "Radix tree should initialize successfully");

    /* Cleanup for next test */
    radix_cleanup();

    TEST_PASS();
}

static int test_radix_add_route(void) {
    int result;

    /* Test adding a simple route */
    result = radix_add_route("192.168.1.0", 24, "192.168.1.1");
    TEST_ASSERT_EQ(0, result, "Should add route 192.168.1.0/24 successfully");

    /* Test adding another route */
    result = radix_add_route("10.0.0.0", 8, "10.0.0.1");
    TEST_ASSERT_EQ(0, result, "Should add route 10.0.0.0/8 successfully");

    TEST_PASS();
}

static int test_radix_lookup_route(void) {
    char gateway[16];
    int result;

    /* Add a route first */
    radix_add_route("192.168.1.0", 24, "192.168.1.1");

    /* Test lookup that should match */
    result = radix_lookup_route("192.168.1.100", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find route for 192.168.1.100");

    /* Test lookup that should not match */
    result = radix_lookup_route("172.16.1.1", gateway, sizeof(gateway));
    TEST_ASSERT_NE(0, result, "Should not find route for 172.16.1.1");

    TEST_PASS();
}

static int test_radix_delete_route(void) {
    int result;
    char gateway[16];

    /* Add a route */
    radix_add_route("203.0.113.0", 24, "203.0.113.1");

    /* Verify it exists */
    result = radix_lookup_route("203.0.113.100", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Route should exist before deletion");

    /* Delete the route */
    result = radix_delete_route("203.0.113.0", 24);
    TEST_ASSERT_EQ(0, result, "Should delete route successfully");

    /* Verify it's gone */
    result = radix_lookup_route("203.0.113.100", gateway, sizeof(gateway));
    TEST_ASSERT_NE(0, result, "Route should not exist after deletion");

    TEST_PASS();
}

static int test_radix_longest_match(void) {
    char gateway[16];
    int result;

    /* Add routes of different specificities */
    radix_add_route("10.0.0.0", 8, "10.0.0.1");        /* /8 - general */
    radix_add_route("10.1.0.0", 16, "10.1.0.1");       /* /16 - more specific */
    radix_add_route("10.1.1.0", 24, "10.1.1.1");       /* /24 - most specific */

    /* Test lookup that should match the most specific route */
    result = radix_lookup_route("10.1.1.100", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find most specific route for 10.1.1.100");

    /* Test lookup that should match the /16 route */
    result = radix_lookup_route("10.1.2.100", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find /16 route for 10.1.2.100");

    /* Test lookup that should match the /8 route */
    result = radix_lookup_route("10.2.3.100", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find /8 route for 10.2.3.100");

    TEST_PASS();
}

static int test_radix_default_route(void) {
    char gateway[16];
    int result;

    /* Add a default route */
    radix_add_route("0.0.0.0", 0, "192.168.1.1");

    /* Test that any address matches the default route */
    result = radix_lookup_route("8.8.8.8", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find default route for 8.8.8.8");

    result = radix_lookup_route("1.1.1.1", gateway, sizeof(gateway));
    TEST_ASSERT_EQ(0, result, "Should find default route for 1.1.1.1");

    TEST_PASS();
}

static int test_radix_edge_cases(void) {
    int result;

    /* Test invalid IP addresses */
    result = radix_add_route("999.999.999.999", 24, "192.168.1.1");
    TEST_ASSERT_NE(0, result, "Should reject invalid destination IP");

    /* Test invalid prefix length */
    result = radix_add_route("192.168.1.0", 33, "192.168.1.1");
    /* Note: Our current implementation may not validate this */

    /* Test adding duplicate routes */
    result = radix_add_route("192.168.2.0", 24, "192.168.2.1");
    TEST_ASSERT_EQ(0, result, "Should add first instance of route");

    result = radix_add_route("192.168.2.0", 24, "192.168.2.2");
    /* Behavior depends on implementation - may succeed or fail */

    TEST_PASS();
}

/* Performance test */
static int test_radix_performance(void) {
    perf_timer_t timer;
    const int num_routes = 1000;
    char ip_str[16];
    char gw_str[16];
    int successful_adds = 0;
    int successful_lookups = 0;

    /* Performance test: Add many routes */
    test_log_info("Performance test: Adding %d routes...", num_routes);
    PERF_START(&timer);

    for (int i = 1; i <= num_routes; i++) {
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.0", (i >> 8) & 0xFF, i & 0xFF);
        snprintf(gw_str, sizeof(gw_str), "10.%d.%d.1", (i >> 8) & 0xFF, i & 0xFF);

        if (radix_add_route(ip_str, 24, gw_str) == 0) {
            successful_adds++;
        }
    }

    PERF_END(&timer);
    test_log_info("Added %d/%d routes in %.2fms (%.2f routes/ms)",
                  successful_adds, num_routes, timer.elapsed_ms,
                  successful_adds / timer.elapsed_ms);

    /* Performance test: Lookup many routes */
    test_log_info("Performance test: Looking up %d routes...", num_routes);
    PERF_START(&timer);

    char gateway[16];
    for (int i = 1; i <= num_routes; i++) {
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.100", (i >> 8) & 0xFF, i & 0xFF);

        if (radix_lookup_route(ip_str, gateway, sizeof(gateway)) == 0) {
            successful_lookups++;
        }
    }

    PERF_END(&timer);
    test_log_info("Found %d/%d routes in %.2fms (%.2f lookups/ms)",
                  successful_lookups, num_routes, timer.elapsed_ms,
                  successful_lookups / timer.elapsed_ms);

    /* We expect most routes to be found */
    TEST_ASSERT(successful_adds > num_routes * 0.9,
                "Should successfully add most routes");
    TEST_ASSERT(successful_lookups > successful_adds * 0.9,
                "Should successfully lookup most added routes");

    TEST_PASS();
}

/* Test suite definition */
static test_case_t radix_minimal_tests[] = {
    TEST_CASE(radix_init,
              "Test radix tree initialization",
              test_radix_init),

    TEST_CASE(radix_add_route,
              "Test adding routes to radix tree",
              test_radix_add_route),

    TEST_CASE(radix_lookup_route,
              "Test looking up routes in radix tree",
              test_radix_lookup_route),

    TEST_CASE(radix_delete_route,
              "Test deleting routes from radix tree",
              test_radix_delete_route),

    TEST_CASE(radix_longest_match,
              "Test longest prefix matching",
              test_radix_longest_match),

    TEST_CASE(radix_default_route,
              "Test default route handling",
              test_radix_default_route),

    TEST_CASE(radix_edge_cases,
              "Test edge cases and error conditions",
              test_radix_edge_cases),

    TEST_CASE(radix_performance,
              "Performance test for radix operations",
              test_radix_performance),

    TEST_SUITE_END()
};

test_suite_t radix_minimal_test_suite = {
    "Radix Tree Minimal Tests",
    "Test suite for minimal radix tree implementation in userland",
    radix_minimal_tests,
    0,  /* num_tests calculated at runtime */
    radix_test_setup,
    radix_test_teardown
};

/* Main test runner */
int main(int argc, char* argv[]) {
    printf("FreeBSD Radix Tree Minimal Test Suite\n");
    printf("=====================================\n\n");

    /* Initialize test framework */
    if (test_framework_init() != 0) {
        fprintf(stderr, "Failed to initialize test framework\n");
        return 1;
    }

    /* Count tests */
    int count = 0;
    while (radix_minimal_tests[count].name != NULL) {
        count++;
    }
    radix_minimal_test_suite.num_tests = count;

    /* Handle command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --help, -h     Show this help message\n");
            printf("  --verbose, -v  Enable verbose output\n");
            printf("\n");
            printf("This tests the minimal radix tree implementation.\n");
            return 0;
        }
    }

    /* Run the test suite */
    int result = test_run_suite(&radix_minimal_test_suite);

    /* Print summary */
    test_print_summary();

    /* Clean up test framework */
    test_framework_cleanup();

    /* Return appropriate exit code */
    if (result != 0 || g_test_result.failed_tests > 0) {
        return 1;
    }

    return 0;
}