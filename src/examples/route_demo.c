/*
 * FreeBSD Routing Library - Demo Program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "route_lib.h"

/* Helper function to create sockaddr_in */
static struct sockaddr_in* make_sockaddr_in(const char* ip) {
    static struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_len = sizeof(addr);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    return &addr;
}

/* Demo route walker function */
static int print_route(struct route_info* ri, void* arg) {
    char dst_str[INET_ADDRSTRLEN];
    char gw_str[INET_ADDRSTRLEN];

    if (ri->ri_dst && ri->ri_dst->sa_family == AF_INET) {
        struct sockaddr_in* dst = (struct sockaddr_in*)ri->ri_dst;
        inet_ntop(AF_INET, &dst->sin_addr, dst_str, sizeof(dst_str));
    } else {
        strcpy(dst_str, "unknown");
    }

    if (ri->ri_gateway && ri->ri_gateway->sa_family == AF_INET) {
        struct sockaddr_in* gw = (struct sockaddr_in*)ri->ri_gateway;
        inet_ntop(AF_INET, &gw->sin_addr, gw_str, sizeof(gw_str));
    } else {
        strcpy(gw_str, "none");
    }

    printf("  %s via %s (flags: 0x%x)\n", dst_str, gw_str, ri->ri_flags);
    return 0;  /* Continue walking */
}

int main(int argc, char* argv[]) {
    printf("FreeBSD Routing Library Demo\n");
    printf("============================\n\n");

    /* Initialize the routing library */
    if (route_lib_init() != 0) {
        fprintf(stderr, "Failed to initialize routing library\n");
        return 1;
    }

    printf("Routing library initialized successfully\n");

    /* Create a routing table */
    struct rib_head* rh = route_table_create(AF_INET, 0);
    if (!rh) {
        fprintf(stderr, "Failed to create routing table\n");
        route_lib_cleanup();
        return 1;
    }

    printf("Created IPv4 routing table (FIB 0)\n\n");

    /* Add some sample routes */
    printf("Adding sample routes...\n");

    struct route_info ri;
    int result;

    /* Route 1: 192.168.1.0/24 via 192.168.1.1 */
    memset(&ri, 0, sizeof(ri));
    ri.ri_dst = (struct sockaddr*)make_sockaddr_in("192.168.1.0");
    ri.ri_netmask = (struct sockaddr*)make_sockaddr_in("255.255.255.0");
    ri.ri_gateway = (struct sockaddr*)make_sockaddr_in("192.168.1.1");
    ri.ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY;
    ri.ri_fibnum = 0;

    result = route_add(rh, &ri);
    printf("  192.168.1.0/24 via 192.168.1.1: %s\n",
           result == ROUTE_OK ? "SUCCESS" : "FAILED");

    /* Route 2: 10.0.0.0/8 via 10.0.0.1 */
    memset(&ri, 0, sizeof(ri));
    ri.ri_dst = (struct sockaddr*)make_sockaddr_in("10.0.0.0");
    ri.ri_netmask = (struct sockaddr*)make_sockaddr_in("255.0.0.0");
    ri.ri_gateway = (struct sockaddr*)make_sockaddr_in("10.0.0.1");
    ri.ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY;
    ri.ri_fibnum = 0;

    result = route_add(rh, &ri);
    printf("  10.0.0.0/8 via 10.0.0.1: %s\n",
           result == ROUTE_OK ? "SUCCESS" : "FAILED");

    /* Route 3: 172.16.0.0/12 via 172.16.0.1 */
    memset(&ri, 0, sizeof(ri));
    ri.ri_dst = (struct sockaddr*)make_sockaddr_in("172.16.0.0");
    ri.ri_netmask = (struct sockaddr*)make_sockaddr_in("255.240.0.0");
    ri.ri_gateway = (struct sockaddr*)make_sockaddr_in("172.16.0.1");
    ri.ri_flags = ROUTE_RTF_UP | ROUTE_RTF_GATEWAY;
    ri.ri_fibnum = 0;

    result = route_add(rh, &ri);
    printf("  172.16.0.0/12 via 172.16.0.1: %s\n",
           result == ROUTE_OK ? "SUCCESS" : "FAILED");

    printf("\n");

    /* Display all routes */
    printf("Current routing table contents:\n");
    result = route_walk(rh, print_route, NULL);
    if (result != ROUTE_OK) {
        printf("  (Failed to enumerate routes)\n");
    }
    printf("\n");

    /* Test route lookups */
    printf("Testing route lookups...\n");

    const char* test_ips[] = {
        "192.168.1.100",
        "10.1.2.3",
        "172.16.5.10",
        "8.8.8.8"  /* Should not match any route */
    };

    for (int i = 0; i < 4; i++) {
        struct sockaddr_in* lookup_addr = make_sockaddr_in(test_ips[i]);
        struct route_info lookup_result;

        result = route_lookup(rh, (struct sockaddr*)lookup_addr, &lookup_result);

        if (result == ROUTE_OK) {
            char gw_str[INET_ADDRSTRLEN];
            if (lookup_result.ri_gateway && lookup_result.ri_gateway->sa_family == AF_INET) {
                struct sockaddr_in* gw = (struct sockaddr_in*)lookup_result.ri_gateway;
                inet_ntop(AF_INET, &gw->sin_addr, gw_str, sizeof(gw_str));
            } else {
                strcpy(gw_str, "none");
            }
            printf("  %s -> gateway %s\n", test_ips[i], gw_str);
        } else {
            printf("  %s -> no route found\n", test_ips[i]);
        }
    }
    printf("\n");

    /* Display statistics */
    struct route_stats stats;
    result = route_get_stats(rh, &stats);
    if (result == ROUTE_OK) {
        printf("Routing table statistics:\n");
        printf("  Lookups:  %lu\n", stats.rs_lookups);
        printf("  Hits:     %lu\n", stats.rs_hits);
        printf("  Misses:   %lu\n", stats.rs_misses);
        printf("  Adds:     %lu\n", stats.rs_adds);
        printf("  Deletes:  %lu\n", stats.rs_deletes);
        printf("  Nodes:    %lu\n", stats.rs_nodes);
    } else {
        printf("Failed to get routing table statistics\n");
    }
    printf("\n");

    /* Validate table integrity */
    printf("Validating routing table integrity...\n");
    result = route_validate_table(rh);
    printf("  Validation: %s\n", result == ROUTE_OK ? "PASSED" : "FAILED");
    printf("\n");

    /* Clean up */
    route_table_destroy(rh);
    route_lib_cleanup();

    printf("Demo completed successfully!\n");
    return 0;
}