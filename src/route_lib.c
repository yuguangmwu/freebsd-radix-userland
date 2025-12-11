/*
 * FreeBSD Routing Library - Public API Implementation
 *
 * This implementation provides a clean, easy-to-use routing API that
 * leverages our proven FreeBSD radix tree port for enterprise-scale routing.
 */

#include "route_lib.h"
#include "compat_shim.h"
#include "radix.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>

/* Internal structures */
struct route_entry {
    struct sockaddr* re_dst;      /* Destination (in heap) */
    struct sockaddr* re_mask;     /* Mask (in heap) */
    struct sockaddr* re_gateway;  /* Gateway (in heap) */
    int re_flags;                 /* Route flags */
    int re_ifindex;              /* Interface index */
    u_int re_fibnum;             /* FIB number */
    struct radix_node re_nodes[2]; /* FreeBSD radix requires 2 nodes */
};

struct rib_head {
    struct radix_node_head* rh_rnh;  /* Radix tree head */
    int rh_family;                   /* Address family */
    u_int rh_fibnum;                /* FIB number */
    struct route_stats rh_stats;     /* Statistics */
};

/* Global initialization state */
static int g_route_lib_initialized = 0;

/* Helper functions */
static struct sockaddr* copy_sockaddr(const struct sockaddr* src) {
    if (!src) return NULL;

    struct sockaddr* copy = bsd_malloc(src->sa_len, M_RTABLE, M_WAITOK | M_ZERO);
    if (copy) {
        memcpy(copy, src, src->sa_len);
    }
    return copy;
}

static void free_sockaddr(struct sockaddr* sa) {
    if (sa) {
        bsd_free(sa, M_RTABLE);
    }
}

/* Tree walk callback for route_walk */
struct walk_ctx {
    route_walker_f walker;
    void* arg;
    int count;
};

static int route_walk_callback(struct radix_node* rn, void* arg) {
    struct walk_ctx* ctx = (struct walk_ctx*)arg;

    /* Skip internal nodes - only process leaves with route entries */
    if (!(rn->rn_flags & RNF_ROOT) && rn->rn_key) {
        struct route_entry* re = (struct route_entry*)
            ((char*)rn - offsetof(struct route_entry, re_nodes[0]));

        struct route_info ri = {
            .ri_dst = re->re_dst,
            .ri_netmask = re->re_mask,
            .ri_gateway = re->re_gateway,
            .ri_flags = re->re_flags,
            .ri_ifindex = re->re_ifindex,
            .ri_fibnum = re->re_fibnum
        };

        ctx->count++;
        return ctx->walker(&ri, ctx->arg);
    }
    return 0;
}

/* API Implementation */

int route_lib_init(void) {
    if (g_route_lib_initialized) {
        return ROUTE_OK;
    }

    /* Initialize compatibility layer */
    kernel_compat_init();

    g_route_lib_initialized = 1;
    return ROUTE_OK;
}

void route_lib_cleanup(void) {
    if (g_route_lib_initialized) {
        g_route_lib_initialized = 0;
    }
}

struct rib_head* route_table_create(int family, u_int fibnum) {
    if (!g_route_lib_initialized) {
        errno = EINVAL;
        return NULL;
    }

    struct rib_head* rh = bsd_malloc(sizeof(*rh), M_RTABLE, M_WAITOK | M_ZERO);
    if (!rh) {
        errno = ENOMEM;
        return NULL;
    }

    /* Initialize radix tree based on address family */
    int offset;
    switch (family) {
        case AF_INET:
            offset = offsetof(struct sockaddr_in, sin_addr);
            break;
        case AF_INET6:
            offset = offsetof(struct sockaddr_in6, sin6_addr);
            break;
        default:
            offset = 0;  /* Default offset */
            break;
    }

    if (rn_inithead((void**)&rh->rh_rnh, offset) != 1) {
        bsd_free(rh, M_RTABLE);
        errno = ENOMEM;
        return NULL;
    }

    rh->rh_family = family;
    rh->rh_fibnum = fibnum;
    memset(&rh->rh_stats, 0, sizeof(rh->rh_stats));

    return rh;
}

void route_table_destroy(struct rib_head* rh) {
    if (!rh) return;

    /* TODO: Walk tree and free all route entries first */
    if (rh->rh_rnh) {
        rn_detachhead((void**)&rh->rh_rnh);
    }

    bsd_free(rh, M_RTABLE);
}

int route_add(struct rib_head* rh, struct route_info* ri) {
    if (!rh || !ri || !ri->ri_dst) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    /* Allocate route entry */
    struct route_entry* re = bsd_malloc(sizeof(*re), M_RTABLE, M_WAITOK | M_ZERO);
    if (!re) {
        errno = ENOMEM;
        return ROUTE_ENOMEM;
    }

    /* Copy addresses to heap (FreeBSD radix stores pointers) */
    re->re_dst = copy_sockaddr(ri->ri_dst);
    re->re_mask = copy_sockaddr(ri->ri_netmask);
    re->re_gateway = copy_sockaddr(ri->ri_gateway);
    re->re_flags = ri->ri_flags;
    re->re_ifindex = ri->ri_ifindex;
    re->re_fibnum = ri->ri_fibnum;

    if (!re->re_dst) {
        free_sockaddr(re->re_mask);
        free_sockaddr(re->re_gateway);
        bsd_free(re, M_RTABLE);
        errno = ENOMEM;
        return ROUTE_ENOMEM;
    }

    /* Add to radix tree */
    struct radix_node* rn = rh->rh_rnh->rnh_addaddr(
        re->re_dst, re->re_mask, &rh->rh_rnh->rh, re->re_nodes);

    if (!rn) {
        free_sockaddr(re->re_dst);
        free_sockaddr(re->re_mask);
        free_sockaddr(re->re_gateway);
        bsd_free(re, M_RTABLE);
        errno = EEXIST;  /* Likely duplicate */
        return ROUTE_EEXIST;
    }

    /* Update statistics */
    rh->rh_stats.rs_adds++;
    rh->rh_stats.rs_nodes++;

    return ROUTE_OK;
}

int route_delete(struct rib_head* rh, struct sockaddr* dst, struct sockaddr* netmask) {
    if (!rh || !dst) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    /* Find and delete from radix tree */
    struct radix_node* rn = rh->rh_rnh->rnh_deladdr(dst, netmask, &rh->rh_rnh->rh);

    if (!rn) {
        errno = ENOENT;
        return ROUTE_ENOENT;
    }

    /* Free the route entry */
    struct route_entry* re = (struct route_entry*)
        ((char*)rn - offsetof(struct route_entry, re_nodes[0]));

    free_sockaddr(re->re_dst);
    free_sockaddr(re->re_mask);
    free_sockaddr(re->re_gateway);
    bsd_free(re, M_RTABLE);

    /* Update statistics */
    rh->rh_stats.rs_deletes++;
    rh->rh_stats.rs_nodes--;

    return ROUTE_OK;
}

int route_lookup(struct rib_head* rh, struct sockaddr* dst, struct route_info* ri_out) {
    if (!rh || !dst) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    rh->rh_stats.rs_lookups++;

    /* Perform radix tree lookup */
    struct radix_node* rn = rh->rh_rnh->rnh_matchaddr(dst, &rh->rh_rnh->rh);

    if (!rn) {
        rh->rh_stats.rs_misses++;
        errno = ENOENT;
        return ROUTE_ENOENT;
    }

    rh->rh_stats.rs_hits++;

    /* Extract route entry */
    struct route_entry* re = (struct route_entry*)
        ((char*)rn - offsetof(struct route_entry, re_nodes[0]));

    if (ri_out) {
        ri_out->ri_dst = re->re_dst;
        ri_out->ri_netmask = re->re_mask;
        ri_out->ri_gateway = re->re_gateway;
        ri_out->ri_flags = re->re_flags;
        ri_out->ri_ifindex = re->re_ifindex;
        ri_out->ri_fibnum = re->re_fibnum;
    }

    return ROUTE_OK;
}

int route_change(struct rib_head* rh, struct route_info* ri) {
    if (!rh || !ri || !ri->ri_dst) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    /* For simplicity, implement as delete + add */
    int result = route_delete(rh, ri->ri_dst, ri->ri_netmask);
    if (result != ROUTE_OK && result != ROUTE_ENOENT) {
        return result;
    }

    result = route_add(rh, ri);
    if (result == ROUTE_OK) {
        rh->rh_stats.rs_changes++;
    }

    return result;
}

int route_walk(struct rib_head* rh, route_walker_f walker, void* arg) {
    if (!rh || !walker) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    struct walk_ctx ctx = {
        .walker = walker,
        .arg = arg,
        .count = 0
    };

    rh->rh_rnh->rnh_walktree(&rh->rh_rnh->rh, route_walk_callback, &ctx);

    return ctx.count;
}

struct radix_node_head* radix_node_head_create(void) {
    struct radix_node_head* rnh = NULL;

    if (rn_inithead((void**)&rnh, 0) != 1) {
        return NULL;
    }

    return rnh;
}

void radix_node_head_destroy(struct radix_node_head* rnh) {
    if (rnh) {
        rn_detachhead((void**)&rnh);
    }
}

int route_get_stats(struct rib_head* rh, struct route_stats* stats) {
    if (!rh || !stats) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    *stats = rh->rh_stats;
    return ROUTE_OK;
}

void route_print_table(struct rib_head* rh) {
    if (!rh) return;

    printf("Route Table (Family: %d, FIB: %u):\n", rh->rh_family, rh->rh_fibnum);
    printf("Statistics:\n");
    printf("  Nodes: %lu\n", rh->rh_stats.rs_nodes);
    printf("  Lookups: %lu (hits: %lu, misses: %lu)\n",
           rh->rh_stats.rs_lookups, rh->rh_stats.rs_hits, rh->rh_stats.rs_misses);
    printf("  Operations: %lu adds, %lu deletes, %lu changes\n",
           rh->rh_stats.rs_adds, rh->rh_stats.rs_deletes, rh->rh_stats.rs_changes);
}

int route_validate_table(struct rib_head* rh) {
    if (!rh) {
        errno = EINVAL;
        return ROUTE_EINVAL;
    }

    /* TODO: Implement tree consistency validation */
    return ROUTE_OK;
}