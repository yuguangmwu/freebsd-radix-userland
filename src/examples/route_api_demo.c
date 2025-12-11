/*
 * Route Library API Demo
 *
 * Demonstrates the high-level route_lib API built on our proven
 * FreeBSD radix tree implementation.
 */

#include "route_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Helper to create IPv4 sockaddr */
static struct sockaddr_in* make_ipv4_addr(const char* ip_str) {
    struct sockaddr_in* addr = calloc(1, sizeof(*addr));
    if (!addr) return NULL;

    addr->sin_len = sizeof(*addr);
    addr->sin_family = AF_INET;
    inet_pton(AF_INET, ip_str, &addr->sin_addr);

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

int main(void) {
    printf("FreeBSD Route Library API Demo\n");
    printf("==============================\n\n");

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

    /* Add some sample routes */
    printf("Adding sample routes...\n");

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
        /* Local network */
        {
            .ri_dst = (struct sockaddr*)make_ipv4_addr("192.168.1.0"),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(24),
            .ri_gateway = NULL,  /* Direct route */
            .ri_flags = ROUTE_RTF_UP,
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
        /* Corporate network */
        {
            .ri_dst = (struct sockaddr*)make_ipv4_addr("10.0.0.0"),
            .ri_netmask = (struct sockaddr*)make_ipv4_mask(8),
            .ri_gateway = (struct sockaddr*)make_ipv4_addr("192.168.1.5"),
            .ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY,
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
            printf("  ✅ Added route %d\n", i + 1);
        } else {
            printf("  ❌ Failed to add route %d (error: %d)\n", i + 1, result);
        }
    }

    printf("Successfully added %d/%d routes\n\n", added, num_routes);

    /* Display routing table */
    printf("Current routing table:\n");
    int route_count = route_walk(rt, print_route_callback, NULL);
    printf("Total routes: %d\n\n", route_count);

    /* Test route lookups */
    printf("Testing route lookups...\n");

    const char* test_ips[] = {
        "8.8.8.8",       /* Should match default route */
        "192.168.1.100", /* Should match local network */
        "10.1.2.3",      /* Should match host route */
        "10.5.6.7",      /* Should match corporate network */
        "127.0.0.1"      /* Should match default route */
    };

    for (int i = 0; i < 5; i++) {
        struct sockaddr_in* lookup_addr = make_ipv4_addr(test_ips[i]);
        struct route_info ri_out;

        int result = route_lookup(rt, (struct sockaddr*)lookup_addr, &ri_out);

        printf("  %s -> ", test_ips[i]);
        if (result == ROUTE_OK) {
            char gw_str[INET_ADDRSTRLEN];
            if (ri_out.ri_gateway && ri_out.ri_gateway->sa_family == AF_INET) {
                struct sockaddr_in* gw = (struct sockaddr_in*)ri_out.ri_gateway;
                inet_ntop(AF_INET, &gw->sin_addr, gw_str, sizeof(gw_str));
                printf("Gateway: %s", gw_str);
            } else {
                printf("Direct");
            }
            printf(" (if: %d, flags: 0x%x)\n", ri_out.ri_ifindex, ri_out.ri_flags);
        } else {
            printf("No route found (error: %d)\n", result);
        }

        free(lookup_addr);
    }

    printf("\n");

    /* Display statistics */
    struct route_stats stats;
    if (route_get_stats(rt, &stats) == ROUTE_OK) {
        printf("Routing table statistics:\n");
        printf("  Total nodes: %lu\n", stats.rs_nodes);
        printf("  Lookups: %lu (hits: %lu, misses: %lu)\n",
               stats.rs_lookups, stats.rs_hits, stats.rs_misses);
        printf("  Hit rate: %.1f%%\n",
               stats.rs_lookups > 0 ? (stats.rs_hits * 100.0) / stats.rs_lookups : 0.0);
        printf("  Operations: %lu adds, %lu deletes, %lu changes\n",
               stats.rs_adds, stats.rs_deletes, stats.rs_changes);
    }

    /* Test route deletion */
    printf("\nTesting route deletion...\n");
    struct sockaddr_in* del_dst = make_ipv4_addr("10.1.2.3");
    struct sockaddr_in* del_mask = make_ipv4_mask(32);

    int del_result = route_delete(rt, (struct sockaddr*)del_dst, (struct sockaddr*)del_mask);
    if (del_result == ROUTE_OK) {
        printf("✅ Deleted host route for 10.1.2.3/32\n");

        /* FIXED: Use exact lookup to verify deletion */
        struct route_info ri_out;
        int lookup_result = route_lookup(rt, (struct sockaddr*)del_dst, &ri_out);

        if (lookup_result == ROUTE_ENOENT) {
            printf("✅ Route deletion verified - no match found\n");
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

    /* Final statistics */
    printf("\nFinal routing table:\n");
    route_count = route_walk(rt, print_route_callback, NULL);
    printf("Total routes: %d\n", route_count);

    /* Clean up */
    printf("\nCleaning up...\n");

    /* Free allocated addresses */
    for (int i = 0; i < num_routes; i++) {
        free(routes[i].ri_dst);
        free(routes[i].ri_netmask);
        free(routes[i].ri_gateway);
    }

    route_table_destroy(rt);
    route_lib_cleanup();

    printf("✅ Demo completed successfully!\n");
    return 0;
}