/*
 * Minimal Radix Tree Implementation for Userland
 *
 * This is a clean wrapper around FreeBSD's radix tree that works in userland.
 * We include only the radix tree functionality without the full routing stack.
 */

/* Include our compatibility shim FIRST */
#include "../kernel_compat/compat_shim.h"

/* Now we can safely include the FreeBSD radix header */
#include "freebsd/radix_userland.h"

/* Global routing tree head */
static struct radix_node_head *rt_head_v4 = NULL;

/* Initialize the radix tree */
int radix_init(void) {
    kernel_compat_init();

    /* Initialize IPv4 radix tree */
    if (!rn_inithead((void **)&rt_head_v4,
                     offsetof(struct sockaddr_in, sin_addr) * 8)) {
        return -1;
    }

    return 0;
}

/* Cleanup the radix tree */
void radix_cleanup(void) {
    if (rt_head_v4 && rt_head_v4->rnh_close) {
        rt_head_v4->rnh_close((struct radix_node *)rt_head_v4, NULL);
        rt_head_v4 = NULL;
    }
}

/* Helper to create sockaddr_in from IP string and prefix length */
static int make_sockaddr_in(const char *ip_str, int prefixlen,
                           struct sockaddr_in *addr, struct sockaddr_in *mask) {
    memset(addr, 0, sizeof(*addr));
    memset(mask, 0, sizeof(*mask));

    addr->sin_family = AF_INET;
    addr->sin_len = sizeof(*addr);

    if (inet_pton(AF_INET, ip_str, &addr->sin_addr) != 1) {
        return -1;
    }

    /* Create netmask */
    if (prefixlen >= 0 && prefixlen <= 32) {
        mask->sin_family = AF_INET;
        mask->sin_len = sizeof(*mask);

        if (prefixlen == 0) {
            mask->sin_addr.s_addr = 0;
        } else {
            mask->sin_addr.s_addr = htonl(~((1U << (32 - prefixlen)) - 1));
        }
    }

    return 0;
}

/* Add a route to the radix tree */
int radix_add_route(const char *dest_ip, int prefixlen, const char *gateway_ip) {
    struct sockaddr_in dest, mask;
    struct radix_node *rn;

    if (!rt_head_v4) {
        return -1;
    }

    if (make_sockaddr_in(dest_ip, prefixlen, &dest, &mask) != 0) {
        return -1;
    }

    /* For now, we just store the route in the tree without gateway info */
    rn = rt_head_v4->rnh_addaddr((struct sockaddr *)&dest,
                                (struct sockaddr *)&mask,
                                &rt_head_v4->rh, NULL);

    return (rn != NULL) ? 0 : -1;
}

/* Look up a route in the radix tree */
int radix_lookup_route(const char *dest_ip, char *gateway_buf, size_t gateway_buflen) {
    struct sockaddr_in dest;
    struct radix_node *rn;

    if (!rt_head_v4) {
        return -1;
    }

    if (make_sockaddr_in(dest_ip, -1, &dest, NULL) != 0) {
        return -1;
    }

    rn = rt_head_v4->rnh_matchaddr((struct sockaddr *)&dest, &rt_head_v4->rh);

    if (rn) {
        /* For now, just indicate success - we'll add gateway lookup later */
        if (gateway_buf && gateway_buflen > 0) {
            snprintf(gateway_buf, gateway_buflen, "0.0.0.0");  /* Placeholder */
        }
        return 0;
    }

    return -1;
}

/* Delete a route from the radix tree */
int radix_delete_route(const char *dest_ip, int prefixlen) {
    struct sockaddr_in dest, mask;
    struct radix_node *rn;

    if (!rt_head_v4) {
        return -1;
    }

    if (make_sockaddr_in(dest_ip, prefixlen, &dest, &mask) != 0) {
        return -1;
    }

    rn = rt_head_v4->rnh_deladdr((struct sockaddr *)&dest,
                                (struct sockaddr *)&mask,
                                &rt_head_v4->rh);

    return (rn != NULL) ? 0 : -1;
}

/* Walk through all routes in the tree */
int radix_walk_routes(int (*walker)(const char *dest_ip, int prefixlen,
                                   const char *gateway_ip, void *arg),
                     void *arg) {
    /* TODO: Implement tree walking */
    (void)walker;
    (void)arg;

    return -1;  /* Not implemented yet */
}