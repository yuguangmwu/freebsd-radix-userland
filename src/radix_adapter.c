/*
 * Radix Tree Adapter Layer
 *
 * This layer bridges between FreeBSD's radix tree implementation
 * and our clean userland API, handling the impedance mismatch.
 */

#include "compat_shim.h"
#include "radix.h"

/* Our adapter functions */

/* Initialize a radix tree head */
struct radix_node_head* userland_rn_inithead(void) {
    struct radix_node_head *rnh = NULL;

    if (rn_inithead((void **)&rnh, 0) != 1) {
        return NULL;
    }

    return rnh;
}

/* Destroy a radix tree head */
void userland_rn_destroy(struct radix_node_head *rnh) {
    if (rnh) {
        rn_detachhead((void **)&rnh);
    }
}

/* Add a route to the radix tree */
struct radix_node* userland_rn_addroute(struct radix_node_head *rnh,
                                        struct sockaddr *dst,
                                        struct sockaddr *mask) {
    if (!rnh || !dst) {
        return NULL;
    }

    /* FreeBSD radix stores pointers to key and mask, so we need to allocate
     * permanent copies instead of using stack variables from caller */

    /* Copy destination to heap */
    struct sockaddr *heap_dst = bsd_malloc(dst->sa_len, M_RTABLE, M_WAITOK | M_ZERO);
    if (!heap_dst) {
        return NULL;
    }
    memcpy(heap_dst, dst, dst->sa_len);

    /* Copy mask to heap if provided */
    struct sockaddr *heap_mask = NULL;
    if (mask) {
        heap_mask = bsd_malloc(mask->sa_len, M_RTABLE, M_WAITOK | M_ZERO);
        if (!heap_mask) {
            bsd_free(heap_dst, M_RTABLE);
            return NULL;
        }
        memcpy(heap_mask, mask, mask->sa_len);
    }

    /* Allocate nodes for FreeBSD radix tree */
    struct radix_node *nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);
    if (!nodes) {
        bsd_free(heap_dst, M_RTABLE);
        if (heap_mask) bsd_free(heap_mask, M_RTABLE);
        return NULL;
    }

    struct radix_node *rn = rnh->rnh_addaddr(heap_dst, heap_mask, &rnh->rh, nodes);
    if (!rn) {
        bsd_free(nodes, M_RTABLE);
        bsd_free(heap_dst, M_RTABLE);
        if (heap_mask) bsd_free(heap_mask, M_RTABLE);
        return NULL;
    }

    return rn;
}

/* Look up a route in the radix tree */
struct radix_node* userland_rn_lookup(struct radix_node_head *rnh,
                                      struct sockaddr *dst) {
    if (!rnh || !dst) {
        return NULL;
    }

    return rnh->rnh_matchaddr(dst, &rnh->rh);
}

/* Delete a route from the radix tree */
struct radix_node* userland_rn_delete(struct radix_node_head *rnh,
                                      struct sockaddr *dst,
                                      struct sockaddr *mask) {
    if (!rnh || !dst) {
        return NULL;
    }

    return rnh->rnh_deladdr(dst, mask, &rnh->rh);
}

/* Walk the radix tree */
int userland_rn_walktree(struct radix_node_head *rnh,
                        int (*walker)(struct radix_node *, void *),
                        void *arg) {
    if (!rnh || !walker) {
        return -1;
    }

    return rnh->rnh_walktree(&rnh->rh, walker, arg);
}

/* Helper to create a sockaddr_in from IP string */
int userland_make_sockaddr_in(const char *ip_str, struct sockaddr_in *addr) {
    if (!ip_str || !addr) {
        return -1;
    }

    memset(addr, 0, sizeof(*addr));
    addr->sin_len = sizeof(*addr);
    addr->sin_family = AF_INET;

    if (inet_pton(AF_INET, ip_str, &addr->sin_addr) != 1) {
        return -1;
    }

    return 0;
}

/* Helper to create a netmask from prefix length */
int userland_make_netmask(int prefixlen, struct sockaddr_in *mask) {
    if (prefixlen < 0 || prefixlen > 32 || !mask) {
        return -1;
    }

    memset(mask, 0, sizeof(*mask));
    mask->sin_len = sizeof(*mask);
    mask->sin_family = AF_INET;

    if (prefixlen == 0) {
        mask->sin_addr.s_addr = 0;
    } else {
        mask->sin_addr.s_addr = htonl(~((1U << (32 - prefixlen)) - 1));
    }

    return 0;
}