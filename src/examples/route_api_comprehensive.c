/*
 * Route Library Comprehensive API Test
 *
 * Tests both basic functionality AND enterprise-scale performance
 * using the clean route_lib API (not low-level radix calls).
 */

#include "route_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

/* Performance timing */
typedef struct {
    struct timeval start;
    struct timeval end;
    double elapsed_ms;
} perf_timer_t;

static void perf_start(perf_timer_t* timer) {
    gettimeofday(&timer->start, NULL);
}

static void perf_end(perf_timer_t* timer) {
    gettimeofday(&timer->end, NULL);
    timer->elapsed_ms = (timer->end.tv_sec - timer->start.tv_sec) * 1000.0 +
                        (timer->end.tv_usec - timer->start.tv_usec) / 1000.0;
}

/* Helper to create IPv4 sockaddr */
static struct sockaddr_in* make_ipv4_addr(const char* ip_str) {
    struct sockaddr_in* addr = calloc(1, sizeof(*addr));
    if (!addr) return NULL;

    addr->sin_len = sizeof(*addr);
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, ip_str, &addr->sin_addr);

    return addr;
}

/* Helper to create IPv4 sockaddr from uint32_t */
static struct sockaddr_in* make_ipv4_addr_raw(uint32_t addr_host) {
    struct sockaddr_in* addr = calloc(1, sizeof(*addr));
    if (!addr) return NULL;

    addr->sin_len = sizeof(*addr);
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(addr_host);

    return addr;
}

/* Helper to create IPv4 netmask */
static struct sockaddr_in* make_ipv4_mask(int prefix_len) {
    struct sockaddr_in* mask = calloc(1, sizeof(*mask));
    if (!mask) return NULL;

    mask->sin_len = sizeof(*mask);
    mask->sin_family = AF_INET;

    if (prefix_len == 0) {
        mask->sin_addr.s_addr = 0;
    } else {
        mask->sin_addr.s_addr = htonl(~((1U << (32 - prefix_len)) - 1));
    }

    return mask;
}

/* Route walker callback for printing routes */
static int print_route_callback(struct route_info* ri, void* arg) {
    (void)arg;

    char dst_str[INET_ADDRSTRLEN];
    char gw_str[INET_ADDRSTRLEN];
    int prefix_len = 0;

    /* Convert destination */
    if (ri->ri_dst && ri->ri_dst->sa_family == AF_INET) {
        struct sockaddr_in* dst = (struct sockaddr_in*)ri->ri_dst;
        inet_ntop(AF_INET, &dst->sin_addr, dst_str, sizeof(dst_str));

        /* Calculate prefix length from mask */
        if (ri->ri_netmask) {
            struct sockaddr_in* mask = (struct sockaddr_in*)ri->ri_netmask;
            uint32_t mask_val = ntohl(mask->sin_addr.s_addr);
            prefix_len = __builtin_popcount(mask_val);
        }
    } else {
        strcpy(dst_str, "unknown");
    }

    /* Convert gateway */
    if (ri->ri_gateway && ri->ri_gateway->sa_family == AF_INET) {
        struct sockaddr_in* gw = (struct sockaddr_in*)ri->ri_gateway;
        inet_ntop(AF_INET, &gw->sin_addr, gw_str, sizeof(gw_str));
    } else {
        strcpy(gw_str, "none");
    }

    printf("  %s/%d -> %s (flags: 0x%x, if: %d)\n",
           dst_str, prefix_len, gw_str, ri->ri_flags, ri->ri_ifindex);

    return 0;
}

/* Test basic functionality */
static int test_basic_functionality(struct rib_head* rt) {
    printf("=== Basic Functionality Test ===\n");

    /* Add sample routes */
    struct route_info routes[] = {
        /* Default route */
        {
            .ri_dst = (struct sockaddr*)make_ipv4_addr("0.0.0.0"),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(0),
            .ri_gateway = (struct sockaddr*)make_ipv4_addr("192.168.1.1"),
            .ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY,
            .ri_ifindex = 1,
            .ri_fibnum = 0
        },
        /* Host route */
        {
            .ri_dst = (struct sockaddr*)make_ipv4_addr("10.1.2.3"),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(32),
            .ri_gateway = (struct sockaddr*)make_ipv4_addr("192.168.1.10"),
            .ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY | ROUTE_RTF_HOST,
            .ri_ifindex = 1,
            .ri_fibnum = 0
        },
        /* Network route */
        {
            .ri_dst = (struct sockaddr*)make_ipv4_addr("192.168.1.0"),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(24),
            .ri_gateway = NULL,  /* Direct route */
            .ri_flags = ROUTE_RTF_UP,
            .ri_ifindex = 1,
            .ri_fibnum = 0
        }
    };

    int num_routes = sizeof(routes) / sizeof(routes[0]);
    int added = 0;

    for (int i = 0; i < num_routes; i++) {
        int result = route_add(rt, &routes[i]);
        if (result == ROUTE_OK) {
            added++;
            printf("✅ Added route %d\n", i + 1);
        } else {
            printf("❌ Failed to add route %d (error: %d)\n", i + 1, result);
        }
    }

    printf("Successfully added %d/%d routes\n", added, num_routes);

    /* Test lookups */
    printf("\nTesting lookups:\n");
    const char* test_ips[] = {"8.8.8.8", "10.1.2.3", "192.168.1.100"};

    for (int i = 0; i < 3; i++) {
        struct sockaddr_in* lookup_addr = make_ipv4_addr(test_ips[i]);
        struct route_info ri_out;

        int result = route_lookup(rt, (struct sockaddr*)lookup_addr, &ri_out);
        printf("  %s -> %s\n", test_ips[i], (result == ROUTE_OK) ? "Found" : "Not found");
        free(lookup_addr);
    }

    /* Test deletion with EXACT match verification */
    printf("\nTesting route deletion:\n");
    struct sockaddr_in* del_dst = make_ipv4_addr("10.1.2.3");
    struct sockaddr_in* del_mask = make_ipv4_mask(32);

    int del_result = route_delete(rt, (struct sockaddr*)del_dst, (struct sockaddr*)del_mask);
    if (del_result == ROUTE_OK) {
        printf("✅ Deleted host route for 10.1.2.3/32\n");

        /* FIXED: Use exact lookup to verify deletion */
        struct route_info ri_out;
        int lookup_result = route_lookup(rt, (struct sockaddr*)del_dst, &ri_out);

        if (lookup_result == ROUTE_ENOENT) {
            printf("✅ Route deletion verified - no exact match found\n");
        } else if (lookup_result == ROUTE_OK) {
            /* Check if it's the exact route or a different (broader) match */
            if (ri_out.ri_netmask) {
                struct sockaddr_in* found_mask = (struct sockaddr_in*)ri_out.ri_netmask;
                uint32_t found_mask_val = ntohl(found_mask->sin_addr.s_addr);
                uint32_t expected_mask_val = ntohl(del_mask->sin_addr.s_addr);

                if (found_mask_val == expected_mask_val) {
                    printf("❌ Exact route still exists after deletion\n");
                } else {
                    printf("✅ Route deletion verified - found broader match (expected)\n");
                }
            }
        }
    } else {
        printf("❌ Failed to delete route (error: %d)\n", del_result);
    }

    free(del_dst);
    free(del_mask);

    /* Clean up allocated addresses */
    for (int i = 0; i < num_routes; i++) {
        free(routes[i].ri_dst);
        free(routes[i].ri_netmask);
        free(routes[i].ri_gateway);
    }

    return added;
}

/* Enterprise-scale performance test using API */
static int test_enterprise_scale_api(struct rib_head* rt) {
    printf("\n=== Enterprise Scale API Test ===\n");

    const int SCALE_ROUTES = 50000;  /* Test 50K routes through API */
    perf_timer_t timer;
    int successful_adds = 0;
    int successful_lookups = 0;
    int successful_deletes = 0;

    printf("Testing %d routes through route_lib API...\n", SCALE_ROUTES);

    /* Phase 1: Add routes using API */
    printf("Phase 1: Adding routes via API...\n");

    perf_start(&timer);
    for (int i = 0; i < SCALE_ROUTES; i++) {
        /* Generate route using our proven pattern */
        uint32_t route_id = i;
        uint32_t addr;

        if (route_id < 65536) {
            /* 10.0.x.0/24 */
            addr = 0x0A000000 | (route_id << 8);
        } else if (route_id < 131072) {
            /* 172.16.x.0/24 */
            uint32_t offset = route_id - 65536;
            addr = 0xAC100000 | (offset << 8);
        } else {
            /* 192.168.x.0/24 */
            uint32_t offset = route_id - 131072;
            addr = 0xC0A80000 | (offset << 8);
        }

        struct route_info ri = {
            .ri_dst = (struct sockaddr*)make_ipv4_addr_raw(addr),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(24),
            .ri_gateway = (struct sockaddr*)make_ipv4_addr("192.168.1.1"),
            .ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY,
            .ri_ifindex = 1,
            .ri_fibnum = 0
        };

        if (route_add(rt, &ri) == ROUTE_OK) {
            successful_adds++;
        }

        /* Progress indicator */
        if ((i + 1) % 10000 == 0) {
            printf("  Added %d/%d routes (%.1f%%)\n",
                   i + 1, SCALE_ROUTES, ((i + 1) * 100.0) / SCALE_ROUTES);
        }

        /* Clean up this iteration's allocations */
        free(ri.ri_dst);
        free(ri.ri_netmask);
        free(ri.ri_gateway);
    }
    perf_end(&timer);

    double add_rate = successful_adds / (timer.elapsed_ms + 0.001);
    printf("✅ API Added %d/%d routes in %.2fms (%.2f routes/ms)\n",
           successful_adds, SCALE_ROUTES, timer.elapsed_ms, add_rate);

    /* Phase 2: Lookup routes using API */
    printf("Phase 2: Looking up routes via API...\n");

    perf_start(&timer);
    for (int i = 0; i < successful_adds && i < 10000; i++) {  /* Test first 10K lookups */
        uint32_t route_id = i;
        uint32_t addr;

        if (route_id < 65536) {
            addr = 0x0A000000 | (route_id << 8);
        } else if (route_id < 131072) {
            uint32_t offset = route_id - 65536;
            addr = 0xAC100000 | (offset << 8);
        } else {
            uint32_t offset = route_id - 131072;
            addr = 0xC0A80000 | (offset << 8);
        }

        struct sockaddr_in* lookup_addr = make_ipv4_addr_raw(addr);
        struct route_info ri_out;

        if (route_lookup(rt, (struct sockaddr*)lookup_addr, &ri_out) == ROUTE_OK) {
            successful_lookups++;
        }

        free(lookup_addr);
    }
    perf_end(&timer);

    double lookup_rate = successful_lookups / (timer.elapsed_ms + 0.001);
    printf("✅ API Found %d routes in %.2fms (%.2f lookups/ms)\n",
           successful_lookups, timer.elapsed_ms, lookup_rate);

    /* Phase 3: Delete some routes using API */
    printf("Phase 3: Deleting routes via API...\n");

    int delete_count = (successful_adds > 5000) ? 5000 : successful_adds;
    perf_start(&timer);

    for (int i = 0; i < delete_count; i++) {
        uint32_t route_id = i;
        uint32_t addr;

        if (route_id < 65536) {
            addr = 0x0A000000 | (route_id << 8);
        } else {
            uint32_t offset = route_id - 65536;
            addr = 0xAC100000 | (offset << 8);
        }

        struct sockaddr_in* del_dst = make_ipv4_addr_raw(addr);
        struct sockaddr_in* del_mask = make_ipv4_mask(24);

        if (route_delete(rt, (struct sockaddr*)del_dst, (struct sockaddr*)del_mask) == ROUTE_OK) {
            successful_deletes++;
        }

        free(del_dst);
        free(del_mask);
    }
    perf_end(&timer);

    double delete_rate = successful_deletes / (timer.elapsed_ms + 0.001);
    printf("✅ API Deleted %d routes in %.2fms (%.2f deletes/ms)\n",
           successful_deletes, timer.elapsed_ms, delete_rate);

    /* Performance summary */
    printf("\n📊 API Performance Summary:\n");
    printf("   Add rate:    %.2f routes/ms\n", add_rate);
    printf("   Lookup rate: %.2f lookups/ms\n", lookup_rate);
    printf("   Delete rate: %.2f deletes/ms\n", delete_rate);
    printf("   Success rates: %d%% add, %d%% lookup, %d%% delete\n",
           (successful_adds * 100) / SCALE_ROUTES,
           (successful_lookups * 100) / 10000,
           (successful_deletes * 100) / delete_count);

    return successful_adds;
}

int main(void) {
    printf("FreeBSD Route Library Comprehensive API Test\n");
    printf("============================================\n\n");

    /* Initialize the library */
    if (route_lib_init() != ROUTE_OK) {
        fprintf(stderr, "Failed to initialize route library\n");
        return 1;
    }
    printf("✅ Route library initialized\n");

    /* Create IPv4 routing table */
    struct rib_head* rt = route_table_create(AF_INET, 0);
    if (!rt) {
        fprintf(stderr, "Failed to create routing table\n");
        route_lib_cleanup();
        return 1;
    }
    printf("✅ Created IPv4 routing table\n\n");

    /* Test basic functionality */
    int basic_routes = test_basic_functionality(rt);

    /* Test enterprise scale */
    int scale_routes = test_enterprise_scale_api(rt);

    /* Final statistics */
    struct route_stats stats;
    if (route_get_stats(rt, &stats) == ROUTE_OK) {
        printf("\n📈 Final Statistics:\n");
        printf("  Total nodes: %lu\n", stats.rs_nodes);
        printf("  Total lookups: %lu (hits: %lu, misses: %lu)\n",
               stats.rs_lookups, stats.rs_hits, stats.rs_misses);
        printf("  Hit rate: %.1f%%\n",
               stats.rs_lookups > 0 ? (stats.rs_hits * 100.0) / stats.rs_lookups : 0.0);
        printf("  Operations: %lu adds, %lu deletes, %lu changes\n",
               stats.rs_adds, stats.rs_deletes, stats.rs_changes);
    }

    /* Final table count */
    printf("\nFinal routing table:\n");
    int final_count = route_walk(rt, print_route_callback, NULL);
    printf("Total routes remaining: %d\n", final_count);

    /* Clean up */
    route_table_destroy(rt);
    route_lib_cleanup();

    printf("\n🎉 Comprehensive API test completed!\n");
    printf("   Basic functionality: %d routes ✅\n", basic_routes);
    printf("   Enterprise scale: %d routes ✅\n", scale_routes);

    return 0;
}