/*
 * Test suite for Route Table functionality
 */

#include "test_framework.h"
#include "../include/route_lib.h"
#include "../include/freebsd_route_adapter.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Test setup and teardown */
static int route_test_setup(void) {
    return route_lib_init();
}

static int route_test_teardown(void) {
    route_lib_cleanup();
    return 0;
}

/* Helper function to create route info */
static struct route_info* make_route_info(const char* dst, const char* mask, const char* gw, int flags) {
    static struct route_info ri;
    static struct sockaddr_in dst_addr, mask_addr, gw_addr;

    memset(&ri, 0, sizeof(ri));
    memset(&dst_addr, 0, sizeof(dst_addr));
    memset(&mask_addr, 0, sizeof(mask_addr));
    memset(&gw_addr, 0, sizeof(gw_addr));

    /* Destination */
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, dst, &dst_addr.sin_addr);
    ri.ri_dst = (struct sockaddr*)&dst_addr;

    /* Netmask */
    if (mask) {
        mask_addr.sin_family = AF_INET;
        mask_addr.sin_len = sizeof(mask_addr);
        inet_pton(AF_INET, mask, &mask_addr.sin_addr);
        ri.ri_netmask = (struct sockaddr*)&mask_addr;
    }

    /* Gateway */
    if (gw) {
        gw_addr.sin_family = AF_INET;
        gw_addr.sin_len = sizeof(gw_addr);
        inet_pton(AF_INET, gw, &gw_addr.sin_addr);
        ri.ri_gateway = (struct sockaddr*)&gw_addr;
    }

    ri.ri_flags = flags;
    ri.ri_fibnum = 0;

    return &ri;
}

/* Test cases */
static int test_route_table_creation(void) {
    struct rib_head* rh;

    /* Test creating IPv4 routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create IPv4 routing table");

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

static int test_route_basic_operations(void) {
    struct rib_head* rh;
    struct route_info* ri;
    struct route_info lookup_result;
    struct sockaddr_in dst_addr;
    int result;

    /* Create routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create routing table");

    /* Add a route */
    ri = make_route_info("192.168.1.0", "255.255.255.0", "192.168.1.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add route successfully");

    /* Look up the route */
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, "192.168.1.100", &dst_addr.sin_addr);

    result = route_lookup(rh, (struct sockaddr*)&dst_addr, &lookup_result);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should find the route");

    /* Delete the route */
    struct sockaddr_in mask_addr;
    mask_addr.sin_family = AF_INET;
    mask_addr.sin_len = sizeof(mask_addr);
    inet_pton(AF_INET, "255.255.255.0", &mask_addr.sin_addr);

    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, "192.168.1.0", &dst_addr.sin_addr);

    result = route_delete(rh, (struct sockaddr*)&dst_addr, (struct sockaddr*)&mask_addr);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should delete route successfully");

    /* Verify route is gone */
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, "192.168.1.100", &dst_addr.sin_addr);

    result = route_lookup(rh, (struct sockaddr*)&dst_addr, &lookup_result);
    TEST_ASSERT_EQ(ROUTE_ENOENT, result, "Route should not be found after deletion");

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

static int test_route_longest_prefix_match(void) {
    struct rib_head* rh;
    struct route_info* ri;
    struct route_info lookup_result;
    struct sockaddr_in dst_addr;
    int result;

    /* Create routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create routing table");

    /* Add a general route: 192.168.0.0/16 */
    ri = make_route_info("192.168.0.0", "255.255.0.0", "192.168.0.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add /16 route");

    /* Add a more specific route: 192.168.1.0/24 */
    ri = make_route_info("192.168.1.0", "255.255.255.0", "192.168.1.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add /24 route");

    /* Lookup should find the most specific match */
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, "192.168.1.100", &dst_addr.sin_addr);

    result = route_lookup(rh, (struct sockaddr*)&dst_addr, &lookup_result);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should find a matching route");

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

static int test_route_multiple_fibs(void) {
    struct rib_head* rh1;
    struct rib_head* rh2;
    struct route_info* ri;
    int result;

    /* Create two different FIBs */
    rh1 = route_table_create(AF_INET, 0);
    rh2 = route_table_create(AF_INET, 1);

    TEST_ASSERT_NOT_NULL(rh1, "Should create FIB 0");
    TEST_ASSERT_NOT_NULL(rh2, "Should create FIB 1");

    /* Add different routes to each FIB */
    ri = make_route_info("10.0.0.0", "255.0.0.0", "10.0.0.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh1, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add route to FIB 0");

    ri = make_route_info("172.16.0.0", "255.240.0.0", "172.16.0.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh2, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add route to FIB 1");

    /* Verify routes are in correct FIBs */
    struct sockaddr_in dst_addr;
    struct route_info lookup_result;

    /* Check FIB 0 */
    dst_addr.sin_family = AF_INET;
    dst_addr.sin_len = sizeof(dst_addr);
    inet_pton(AF_INET, "10.1.1.1", &dst_addr.sin_addr);

    result = route_lookup(rh1, (struct sockaddr*)&dst_addr, &lookup_result);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should find 10.x route in FIB 0");

    result = route_lookup(rh2, (struct sockaddr*)&dst_addr, &lookup_result);
    TEST_ASSERT_EQ(ROUTE_ENOENT, result, "Should not find 10.x route in FIB 1");

    /* Clean up */
    route_table_destroy(rh1);
    route_table_destroy(rh2);

    TEST_PASS();
}

static int test_route_statistics(void) {
    struct rib_head* rh;
    struct route_info* ri;
    struct route_stats stats;
    int result;

    /* Create routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create routing table");

    /* Get initial statistics */
    result = route_get_stats(rh, &stats);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should get statistics");

    u_long initial_adds = stats.rs_adds;

    /* Add a route */
    ri = make_route_info("203.0.113.0", "255.255.255.0", "203.0.113.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
    result = route_add(rh, ri);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should add route");

    /* Check statistics updated */
    result = route_get_stats(rh, &stats);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should get updated statistics");
    TEST_ASSERT_EQ(initial_adds + 1, stats.rs_adds, "Add count should increment");

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

/* Performance test */
static int test_route_performance(void) {
    struct rib_head* rh;
    struct route_info* ri;
    perf_timer_t timer;
    const int num_routes = 1000;
    char ip_str[16];
    int result;

    /* Create routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create routing table");

    /* Performance test: Add many routes */
    PERF_START(&timer);

    for (int i = 1; i <= num_routes; i++) {
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.0", (i >> 8) & 0xFF, i & 0xFF);
        ri = make_route_info(ip_str, "255.255.255.0", "10.0.0.1", ROUTE_RTF_UP | ROUTE_RTF_GATEWAY);
        result = route_add(rh, ri);

        if (result != ROUTE_OK) {
            test_log_error(__FILE__, __LINE__, __func__,
                          "Failed to add route %d: %s", i, ip_str);
            break;
        }
    }

    PERF_END(&timer);

    test_log_info("Added %d routes in %.2fms (%.2f routes/ms)",
                  num_routes, timer.elapsed_ms, num_routes / timer.elapsed_ms);

    /* Performance test: Lookup many routes */
    PERF_START(&timer);

    struct route_info lookup_result;
    struct sockaddr_in dst_addr;
    int lookups_performed = 0;

    for (int i = 1; i <= num_routes; i++) {
        snprintf(ip_str, sizeof(ip_str), "10.%d.%d.100", (i >> 8) & 0xFF, i & 0xFF);

        dst_addr.sin_family = AF_INET;
        dst_addr.sin_len = sizeof(dst_addr);
        inet_pton(AF_INET, ip_str, &dst_addr.sin_addr);

        result = route_lookup(rh, (struct sockaddr*)&dst_addr, &lookup_result);
        if (result == ROUTE_OK) {
            lookups_performed++;
        }
    }

    PERF_END(&timer);

    test_log_info("Performed %d lookups in %.2fms (%.2f lookups/ms)",
                  lookups_performed, timer.elapsed_ms, lookups_performed / timer.elapsed_ms);

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

/* Test walker function for route enumeration */
static int test_walker(struct route_info* ri, void* arg) {
    (void)ri;  /* Suppress unused parameter warning */
    int* count = (int*)arg;
    (*count)++;
    return 0;  /* Continue walking */
}

static int test_route_enumeration(void) {
    struct rib_head* rh;
    struct route_info* ri;
    int result;
    int route_count = 0;

    /* Create routing table */
    rh = route_table_create(AF_INET, 0);
    TEST_ASSERT_NOT_NULL(rh, "Should create routing table");

    /* Add several routes */
    const char* routes[] = {
        "192.168.1.0",
        "192.168.2.0",
        "10.0.0.0",
        "172.16.0.0"
    };

    for (int i = 0; i < 4; i++) {
        ri = make_route_info(routes[i], "255.255.255.0", "192.168.1.1", ROUTE_RTF_UP);
        result = route_add(rh, ri);
        TEST_ASSERT_EQ(ROUTE_OK, result, "Should add route %s", routes[i]);
    }

    /* Walk through all routes */
    result = route_walk(rh, test_walker, &route_count);
    TEST_ASSERT_EQ(ROUTE_OK, result, "Should walk routes successfully");
    TEST_ASSERT_EQ(4, route_count, "Should have walked through 4 routes");

    /* Clean up */
    route_table_destroy(rh);

    TEST_PASS();
}

/* Test suite definition */
static test_case_t route_table_tests[] = {
    TEST_CASE(route_table_creation,
              "Test creation and destruction of route tables",
              test_route_table_creation),

    TEST_CASE(route_basic_operations,
              "Test basic route add/lookup/delete operations",
              test_route_basic_operations),

    TEST_CASE(route_longest_prefix_match,
              "Test longest prefix matching algorithm",
              test_route_longest_prefix_match),

    TEST_CASE(route_multiple_fibs,
              "Test multiple forwarding information bases",
              test_route_multiple_fibs),

    TEST_CASE(route_statistics,
              "Test route table statistics collection",
              test_route_statistics),

    TEST_CASE(route_enumeration,
              "Test route table enumeration via walker",
              test_route_enumeration),

    TEST_CASE(route_performance,
              "Performance test for route operations",
              test_route_performance),

    TEST_SUITE_END()
};

test_suite_t route_table_test_suite = {
    "Route Table Tests",
    "Test suite for route table management and operations",
    route_table_tests,
    0,  /* num_tests calculated at runtime */
    route_test_setup,
    route_test_teardown
};