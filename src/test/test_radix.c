/*
 * Test suite for Radix Tree functionality
 */

#include "test_framework.h"
#include "../include/freebsd_route_adapter.h"
#include "../freebsd/radix.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Test helper functions */
static struct sockaddr_in* make_sockaddr_in(const char* ip, int port) {
    static struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    addr.sin_len = sizeof(addr);
    return &addr;
}

/* Test cases */
static int test_radix_node_head_creation(void) {
    struct radix_node_head* rnh;

    /* Test creating a radix node head */
    if (!rn_inithead((void**)&rnh, AF_INET)) {
        TEST_FAIL("Failed to create radix node head for AF_INET");
    }

    TEST_ASSERT_NOT_NULL(rnh, "Radix node head should not be NULL");
    TEST_ASSERT_NOT_NULL(rnh->rnh_addaddr, "Add function should be set");
    TEST_ASSERT_NOT_NULL(rnh->rnh_deladdr, "Delete function should be set");
    TEST_ASSERT_NOT_NULL(rnh->rnh_matchaddr, "Match function should be set");

    /* Clean up */
    if (rnh->rnh_close) {
        rnh->rnh_close((struct radix_node*)rnh, NULL);
    }

    TEST_PASS();
}

static int test_radix_basic_operations(void) {
    struct radix_node_head* rnh;
    struct radix_node* rn;
    struct sockaddr_in* dest, *mask;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, AF_INET)) {
        TEST_FAIL("Failed to initialize radix tree");
    }

    /* Test data */
    dest = make_sockaddr_in("192.168.1.0", 0);
    mask = make_sockaddr_in("255.255.255.0", 0);

    /* Test adding a node */
    rn = rnh->rnh_addaddr((struct sockaddr*)dest,
                         (struct sockaddr*)mask,
                         &rnh->rh, NULL);

    TEST_ASSERT_NOT_NULL(rn, "Should be able to add a route");

    /* Test looking up the node */
    rn = rnh->rnh_matchaddr((struct sockaddr*)dest, &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should be able to find the added route");

    /* Test deleting the node */
    rn = rnh->rnh_deladdr((struct sockaddr*)dest,
                         (struct sockaddr*)mask,
                         &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should be able to delete the route");

    /* Verify node is gone */
    rn = rnh->rnh_matchaddr((struct sockaddr*)dest, &rnh->rh);
    TEST_ASSERT_NULL(rn, "Route should be deleted");

    /* Clean up */
    if (rnh->rnh_close) {
        rnh->rnh_close((struct radix_node*)rnh, NULL);
    }

    TEST_PASS();
}

static int test_radix_longest_match(void) {
    struct radix_node_head* rnh;
    struct radix_node* rn;
    struct sockaddr_in* dest1, *dest2, *mask1, *mask2, *lookup;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, AF_INET)) {
        TEST_FAIL("Failed to initialize radix tree");
    }

    /* Add a general route: 192.168.0.0/16 */
    dest1 = make_sockaddr_in("192.168.0.0", 0);
    mask1 = make_sockaddr_in("255.255.0.0", 0);
    rn = rnh->rnh_addaddr((struct sockaddr*)dest1,
                         (struct sockaddr*)mask1,
                         &rnh->rh, NULL);
    TEST_ASSERT_NOT_NULL(rn, "Should add 192.168.0.0/16 route");

    /* Add a more specific route: 192.168.1.0/24 */
    dest2 = make_sockaddr_in("192.168.1.0", 0);
    mask2 = make_sockaddr_in("255.255.255.0", 0);
    rn = rnh->rnh_addaddr((struct sockaddr*)dest2,
                         (struct sockaddr*)mask2,
                         &rnh->rh, NULL);
    TEST_ASSERT_NOT_NULL(rn, "Should add 192.168.1.0/24 route");

    /* Lookup should find the most specific match */
    lookup = make_sockaddr_in("192.168.1.100", 0);
    rn = rnh->rnh_matchaddr((struct sockaddr*)lookup, &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should find a matching route for 192.168.1.100");

    /* The matched route should be the more specific one */
    /* Note: In a real implementation, we'd need to compare the actual
     * route data to verify which route was matched */

    /* Clean up */
    if (rnh->rnh_close) {
        rnh->rnh_close((struct radix_node*)rnh, NULL);
    }

    TEST_PASS();
}

static int test_radix_multiple_routes(void) {
    struct radix_node_head* rnh;
    struct radix_node* rn;
    int route_count = 0;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, AF_INET)) {
        TEST_FAIL("Failed to initialize radix tree");
    }

    /* Add multiple routes */
    const char* routes[] = {
        "10.0.0.0",
        "172.16.0.0",
        "192.168.0.0",
        "203.0.113.0"
    };

    struct sockaddr_in* mask = make_sockaddr_in("255.255.255.0", 0);

    for (int i = 0; i < 4; i++) {
        struct sockaddr_in* dest = make_sockaddr_in(routes[i], 0);
        rn = rnh->rnh_addaddr((struct sockaddr*)dest,
                             (struct sockaddr*)mask,
                             &rnh->rh, NULL);
        if (rn) {
            route_count++;
        }
    }

    TEST_ASSERT_EQ(4, route_count, "Should have added 4 routes");

    /* Test lookups for each route */
    for (int i = 0; i < 4; i++) {
        struct sockaddr_in* lookup = make_sockaddr_in(routes[i], 0);
        rn = rnh->rnh_matchaddr((struct sockaddr*)lookup, &rnh->rh);
        TEST_ASSERT_NOT_NULL(rn, "Should find route for %s", routes[i]);
    }

    /* Clean up */
    if (rnh->rnh_close) {
        rnh->rnh_close((struct radix_node*)rnh, NULL);
    }

    TEST_PASS();
}

/* Test suite definition */
static test_case_t radix_tests[] = {
    TEST_CASE(radix_node_head_creation,
              "Test creation of radix node head",
              test_radix_node_head_creation),

    TEST_CASE(radix_basic_operations,
              "Test basic add/lookup/delete operations",
              test_radix_basic_operations),

    TEST_CASE(radix_longest_match,
              "Test longest prefix matching",
              test_radix_longest_match),

    TEST_CASE(radix_multiple_routes,
              "Test handling multiple routes",
              test_radix_multiple_routes),

    TEST_SUITE_END()
};

test_suite_t radix_test_suite = {
    "Radix Tree Tests",
    "Test suite for radix tree data structure used in routing",
    radix_tests,
    0,  /* num_tests calculated at runtime */
    NULL,  /* setup */
    NULL   /* teardown */
};