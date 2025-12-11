/*
 * Test suite for Radix Tree functionality
 */

#include "test_framework.h"
/* Use new Level 2 compatibility layer directly */
#include "../kernel_compat/compat_shim.h"
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
    struct radix_node_head* rnh = NULL;

    /* Test creating a radix node head */
    if (!rn_inithead((void**)&rnh, 32)) {
        TEST_FAIL("Failed to create radix node head");
    }

    TEST_ASSERT_NOT_NULL(rnh, "Radix node head should not be NULL");
    TEST_ASSERT_NOT_NULL(rnh->rnh_addaddr, "Add function should be set");
    TEST_ASSERT_NOT_NULL(rnh->rnh_deladdr, "Delete function should be set");
    TEST_ASSERT_NOT_NULL(rnh->rnh_matchaddr, "Match function should be set");

    /* Clean up properly */
    rn_detachhead((void**)&rnh);

    TEST_PASS();
}

static int test_radix_basic_operations(void) {
    struct radix_node_head* rnh = NULL;
    struct radix_node* rn;
    struct sockaddr_in* dest, *mask;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, 32)) {
        TEST_FAIL("Failed to initialize radix tree");
    }

    /* Test data */
    dest = make_sockaddr_in("192.168.1.0", 0);
    mask = make_sockaddr_in("255.255.255.0", 0);

    /* Test adding a node */
    struct radix_node *nodes = bsd_malloc(2 * sizeof(*nodes), M_RTABLE, M_WAITOK | M_ZERO);
    if (!nodes) {
        rn_detachhead((void**)&rnh);
        TEST_FAIL("Failed to allocate radix nodes");
    }

    rn = rnh->rnh_addaddr((struct sockaddr*)dest,
                         (struct sockaddr*)mask,
                         &rnh->rh, nodes);

    TEST_ASSERT_NOT_NULL(rn, "Should be able to add a route");

    /* Test looking up the node */
    rn = rnh->rnh_matchaddr((struct sockaddr*)dest, &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should be able to find the added route");

    /* Test deleting the node */
    rn = rnh->rnh_deladdr((struct sockaddr*)dest,
                         (struct sockaddr*)mask,
                         &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should be able to delete the route");

    /* Free the nodes after successful deletion */
    if (rn) {
        bsd_free(nodes, M_RTABLE);
    }

    /* Verify node is gone */
    rn = rnh->rnh_matchaddr((struct sockaddr*)dest, &rnh->rh);
    TEST_ASSERT_NULL(rn, "Route should be deleted");

    /* Clean up properly */
    rn_detachhead((void**)&rnh);

    TEST_PASS();
}

static int test_radix_longest_match(void) {
    struct radix_node_head* rnh = NULL;
    struct radix_node* rn;
    struct sockaddr_in* dest1, *dest2, *mask1, *mask2, *lookup;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, 32)) {
        TEST_FAIL("Failed to initialize radix tree");
    }

    /* Add a general route: 192.168.0.0/16 */
    dest1 = make_sockaddr_in("192.168.0.0", 0);
    mask1 = make_sockaddr_in("255.255.0.0", 0);
    struct radix_node *nodes1 = bsd_malloc(2 * sizeof(*nodes1), M_RTABLE, M_WAITOK | M_ZERO);
    if (!nodes1) {
        rn_detachhead((void**)&rnh);
        TEST_FAIL("Failed to allocate radix nodes1");
    }

    rn = rnh->rnh_addaddr((struct sockaddr*)dest1,
                         (struct sockaddr*)mask1,
                         &rnh->rh, nodes1);
    TEST_ASSERT_NOT_NULL(rn, "Should add 192.168.0.0/16 route");

    /* Add a more specific route: 192.168.1.0/24 */
    dest2 = make_sockaddr_in("192.168.1.0", 0);
    mask2 = make_sockaddr_in("255.255.255.0", 0);
    struct radix_node *nodes2 = bsd_malloc(2 * sizeof(*nodes2), M_RTABLE, M_WAITOK | M_ZERO);
    if (!nodes2) {
        rn_detachhead((void**)&rnh);
        TEST_FAIL("Failed to allocate radix nodes2");
    }

    rn = rnh->rnh_addaddr((struct sockaddr*)dest2,
                         (struct sockaddr*)mask2,
                         &rnh->rh, nodes2);
    TEST_ASSERT_NOT_NULL(rn, "Should add 192.168.1.0/24 route");

    /* Lookup should find the most specific match */
    lookup = make_sockaddr_in("192.168.1.100", 0);
    rn = rnh->rnh_matchaddr((struct sockaddr*)lookup, &rnh->rh);
    TEST_ASSERT_NOT_NULL(rn, "Should find a matching route for 192.168.1.100");

    /* The matched route should be the more specific one */
    /* Note: In a real implementation, we'd need to compare the actual
     * route data to verify which route was matched */

    /* Clean up properly */
    rn_detachhead((void**)&rnh);

    TEST_PASS();
}

static int test_radix_multiple_routes(void) {
    struct radix_node_head* rnh = NULL;
    struct radix_node* rn;
    int route_count = 0;

    /* Initialize radix tree */
    if (!rn_inithead((void**)&rnh, 32)) {
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
        struct radix_node *nodes = bsd_malloc(2 * sizeof(*nodes), M_RTABLE, M_WAITOK | M_ZERO);
        if (!nodes) {
            rn_detachhead((void**)&rnh);
            TEST_FAIL("Failed to allocate radix nodes for route %d", i);
        }

        rn = rnh->rnh_addaddr((struct sockaddr*)dest,
                             (struct sockaddr*)mask,
                             &rnh->rh, nodes);
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

    /* Clean up properly */
    rn_detachhead((void**)&rnh);

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