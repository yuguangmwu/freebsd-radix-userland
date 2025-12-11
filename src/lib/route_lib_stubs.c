/*
 * FreeBSD Routing Library - Real Implementation using Radix Trees
 *
 * This provides actual routing functionality using the FreeBSD radix tree infrastructure.
 */

#include "../include/route_lib.h"
#include "../kernel_compat/compat_shim.h"
#include "../freebsd/radix.h"
#include <stdio.h>
#include <string.h>

/* Real implementation using radix trees */

/* Enhanced rib_head structure with real functionality */
struct rib_head {
    struct radix_node_head *rnh;
    int family;
    u_int fibnum;
    struct route_stats stats;
    struct rmlock lock;
};

/* Route entry structure for storing in radix tree */
struct route_entry {
    struct radix_node rn[2];  /* Must be first - required by radix tree */
    struct sockaddr_storage dst_storage;    /* Actual destination storage */
    struct sockaddr_storage netmask_storage; /* Actual netmask storage */
    struct sockaddr_storage gateway_storage; /* Actual gateway storage */
    struct sockaddr *dst;         /* Points to dst_storage */
    struct sockaddr *netmask;     /* Points to netmask_storage */
    struct sockaddr *gateway;     /* Points to gateway_storage */
    int flags;
    int ifindex;
    u_int fibnum;
};

/* Global state for real implementation */
static int lib_initialized = 0;
static struct route_stats global_stats = {0};

/* Helper function to copy sockaddr structures */
static struct sockaddr* copy_sockaddr(struct sockaddr_storage* storage, const struct sockaddr* src) {
    if (!src || !storage) {
        return NULL;
    }

    memcpy(storage, src, src->sa_len);
    return (struct sockaddr*)storage;
}

/* Adapter function to convert radix node to route_info for walking */
static int radix_walker_adapter(struct radix_node* rn, void* arg) {
    struct {
        route_walker_f wrapper_walker;
        void* wrapper_arg;
    } *wd = arg;

    /* Skip internal radix nodes */
    if (rn->rn_flags & RNF_ROOT) {
        return 0; /* Continue */
    }

    /* Get the route entry from the radix node */
    struct route_entry* re = (struct route_entry*)rn;

    /* Convert to route_info structure */
    struct route_info ri;
    memset(&ri, 0, sizeof(ri));
    ri.ri_dst = re->dst;
    ri.ri_netmask = re->netmask;
    ri.ri_gateway = re->gateway;
    ri.ri_flags = re->flags;
    ri.ri_ifindex = re->ifindex;
    ri.ri_fibnum = re->fibnum;

    /* Call the wrapper walker */
    return wd->wrapper_walker(&ri, wd->wrapper_arg);
}

/* Route library initialization and cleanup */
int route_lib_init(void) {
    if (lib_initialized) {
        return 0; /* Already initialized */
    }

    printf("[ROUTE_LIB] Initializing real routing library with radix trees\n");
    memset(&global_stats, 0, sizeof(global_stats));
    lib_initialized = 1;
    return 0; /* Success */
}

void route_lib_cleanup(void) {
    if (!lib_initialized) {
        return;
    }

    printf("[ROUTE_LIB] Cleaning up real routing library\n");
    lib_initialized = 0;
}

/* Routing table management */
struct rib_head* route_table_create(int family, u_int fibnum) {
    printf("[ROUTE_LIB] Creating real routing table: family=%d, fibnum=%u\n", family, fibnum);

    /* Allocate rib_head structure */
    struct rib_head* rh = bsd_malloc(sizeof(*rh), M_RTABLE, M_WAITOK | M_ZERO);
    if (!rh) {
        return NULL;
    }

    /* Initialize radix tree head */
    struct radix_node_head *rnh = NULL;
    if (!rn_inithead((void**)&rnh, 32)) {
        printf("[ROUTE_LIB] Failed to initialize radix head\n");
        bsd_free(rh, M_RTABLE);
        return NULL;
    }

    /* Initialize the routing table */
    rh->rnh = rnh;
    rh->family = family;
    rh->fibnum = fibnum;
    memset(&rh->stats, 0, sizeof(rh->stats));

    /* Initialize rmlock for thread safety */
    if (rm_init_flags(&rh->lock, "route_table_lock", RM_DUPOK) != 0) {
        rn_detachhead((void**)&rnh);
        bsd_free(rh, M_RTABLE);
        return NULL;
    }

    printf("[ROUTE_LIB] Successfully created routing table at %p\n", (void*)rh);
    return rh;
}

void route_table_destroy(struct rib_head* rh) {
    printf("[ROUTE_LIB] Destroying routing table: rh=%p\n", (void*)rh);

    if (!rh) {
        return;
    }

    /* Clean up the radix tree head */
    if (rh->rnh) {
        rn_detachhead((void**)&rh->rnh);
    }

    /* Destroy the lock */
    rm_destroy(&rh->lock);

    bsd_free(rh, M_RTABLE);
}

/* Route manipulation functions - REAL IMPLEMENTATION using radix trees */
int route_add(struct rib_head* rh, struct route_info* ri) {
    printf("[ROUTE_LIB] Adding route with REAL radix implementation: rh=%p, dst=%p\n", (void*)rh, (void*)ri->ri_dst);

    if (!rh || !ri || !ri->ri_dst) {
        return ROUTE_EINVAL; /* Invalid parameter */
    }

    /* Acquire writer lock for thread safety */
    rm_wlock(&rh->lock);

    /* Allocate route entry with radix nodes as first member */
    struct route_entry* re = bsd_malloc(sizeof(*re), M_RTABLE, M_WAITOK | M_ZERO);
    if (!re) {
        rm_wunlock(&rh->lock);
        return ROUTE_ENOMEM; /* Out of memory */
    }

    /* Copy route information to persistent storage */
    re->dst = copy_sockaddr(&re->dst_storage, ri->ri_dst);
    re->netmask = copy_sockaddr(&re->netmask_storage, ri->ri_netmask);
    re->gateway = copy_sockaddr(&re->gateway_storage, ri->ri_gateway);
    re->flags = ri->ri_flags;
    re->ifindex = ri->ri_ifindex;
    re->fibnum = rh->fibnum;

    /* Add to radix tree using REAL FreeBSD radix functions */
    struct radix_node* rn = rh->rnh->rnh_addaddr(
        (struct sockaddr*)re->dst,
        (struct sockaddr*)re->netmask,
        &rh->rnh->rh,
        re->rn  /* Pass the radix nodes from route_entry */
    );

    if (!rn) {
        printf("[ROUTE_LIB] REAL radix tree insertion failed\n");
        bsd_free(re, M_RTABLE);
        rm_wunlock(&rh->lock);
        return ROUTE_EEXIST; /* Route already exists or insertion failed */
    }

    /* Update statistics */
    rh->stats.rs_adds++;
    rh->stats.rs_nodes++;
    global_stats.rs_adds++;
    global_stats.rs_nodes++;

    rm_wunlock(&rh->lock);
    printf("[ROUTE_LIB] Successfully stored route in REAL radix tree\n");
    return ROUTE_OK; /* Success */
}

int route_delete(struct rib_head* rh, struct sockaddr* dst, struct sockaddr* netmask) {
    printf("[ROUTE_LIB] Deleting route with REAL radix implementation: rh=%p, dst=%p, netmask=%p\n", (void*)rh, (void*)dst, (void*)netmask);

    if (!rh || !dst) {
        return ROUTE_EINVAL; /* Invalid parameter */
    }

    /* Debug: Print the addresses we're trying to delete */
    if (dst->sa_family == AF_INET) {
        struct sockaddr_in* sin_dst = (struct sockaddr_in*)dst;
        char dst_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin_dst->sin_addr, dst_str, sizeof(dst_str));
        printf("[ROUTE_LIB] Attempting to delete route dst=%s", dst_str);

        if (netmask && netmask->sa_family == AF_INET) {
            struct sockaddr_in* sin_mask = (struct sockaddr_in*)netmask;
            char mask_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sin_mask->sin_addr, mask_str, sizeof(mask_str));
            printf(" netmask=%s", mask_str);
        }
        printf("\n");
    }

    /* Acquire writer lock for thread safety */
    rm_wlock(&rh->lock);

    /* First find the route to get the exact stored addresses */
    struct radix_node* find_rn = rh->rnh->rnh_matchaddr((struct sockaddr*)dst, &rh->rnh->rh);
    if (!find_rn) {
        printf("[ROUTE_LIB] Route not found in REAL radix tree\n");
        rm_wunlock(&rh->lock);
        return ROUTE_ENOENT; /* Not found */
    }

    struct route_entry* re = (struct route_entry*)find_rn;

    /* Use the exact stored addresses for deletion */
    struct radix_node* rn = rh->rnh->rnh_deladdr(
        re->dst,      /* Use the exact stored dst address */
        re->netmask,  /* Use the exact stored netmask address */
        &rh->rnh->rh
    );

    if (!rn) {
        printf("[ROUTE_LIB] Route deletion failed even with exact addresses!\n");
        rm_wunlock(&rh->lock);
        return ROUTE_ENOENT; /* Not found */
    }

    /* Verify we got the same route entry back */
    if (rn != find_rn) {
        printf("[ROUTE_LIB] WARNING: Deleted different route than found!\n");
    }

    /* Update statistics */
    rh->stats.rs_deletes++;
    rh->stats.rs_nodes--;
    global_stats.rs_deletes++;
    global_stats.rs_nodes--;

    /* Free the route entry */
    bsd_free(re, M_RTABLE);

    rm_wunlock(&rh->lock);
    printf("[ROUTE_LIB] Successfully deleted route from REAL radix tree\n");
    return ROUTE_OK; /* Success */
}

int route_lookup(struct rib_head* rh, struct sockaddr* dst, struct route_info* ri_out) {
    printf("[ROUTE_LIB] Looking up route with REAL radix implementation: rh=%p, dst=%p\n", (void*)rh, (void*)dst);

    if (!rh || !dst) {
        global_stats.rs_misses++;
        return ROUTE_EINVAL; /* Invalid parameter */
    }

    /* Acquire reader lock for thread safety */
    struct rm_priotracker tracker;
    rm_rlock(&rh->lock, &tracker);

    /* Lookup in radix tree using REAL FreeBSD radix functions */
    struct radix_node* rn = rh->rnh->rnh_matchaddr(
        (struct sockaddr*)dst,
        &rh->rnh->rh
    );

    if (rn) {
        /* Route found - get the route entry */
        struct route_entry* re = (struct route_entry*)rn;

        /* Update statistics */
        rh->stats.rs_lookups++;
        rh->stats.rs_hits++;
        global_stats.rs_lookups++;
        global_stats.rs_hits++;

        /* Fill output structure if provided */
        if (ri_out) {
            memset(ri_out, 0, sizeof(*ri_out));
            ri_out->ri_dst = re->dst;
            ri_out->ri_netmask = re->netmask;
            ri_out->ri_gateway = re->gateway;
            ri_out->ri_flags = re->flags;
            ri_out->ri_ifindex = re->ifindex;
            ri_out->ri_fibnum = re->fibnum;
        }

        rm_runlock(&rh->lock, &tracker);
        printf("[ROUTE_LIB] Route found in REAL radix tree\n");
        return ROUTE_OK; /* Success */
    } else {
        /* Route not found */
        rh->stats.rs_lookups++;
        rh->stats.rs_misses++;
        global_stats.rs_lookups++;
        global_stats.rs_misses++;

        rm_runlock(&rh->lock, &tracker);
        printf("[ROUTE_LIB] Route not found in REAL radix tree\n");
        return ROUTE_ENOENT; /* Not found */
    }
}

int route_change(struct rib_head* rh, struct route_info* ri) {
    printf("[ROUTE_LIB] Changing route with REAL radix implementation: rh=%p, dst=%p\n", (void*)rh, (void*)ri->ri_dst);

    if (!rh || !ri) {
        return -1; /* Error */
    }

    /* For route change, we delete the old route and add the new one */
    int result = route_delete(rh, ri->ri_dst, ri->ri_netmask);
    if (result == 0) {
        result = route_add(rh, ri);
        if (result == 0) {
            /* Update statistics - adjust for the delete/add pair */
            rh->stats.rs_changes++;
            global_stats.rs_changes++;
            /* Correct the add/delete stats since this is a change */
            rh->stats.rs_adds--;
            rh->stats.rs_deletes--;
            global_stats.rs_adds--;
            global_stats.rs_deletes--;
            printf("[ROUTE_LIB] Successfully changed route using REAL radix tree\n");
        }
    }

    return result;
}

/* Route enumeration */
int route_walk(struct rib_head* rh, route_walker_f walker, void* arg) {
    printf("[ROUTE_LIB] Walking routes with REAL radix implementation: rh=%p, walker=%p\n", (void*)rh, (void*)walker);

    if (!rh || !walker) {
        return -1; /* Error */
    }

    /* Structure to pass both the walker function and argument to radix tree walker */
    struct {
        route_walker_f wrapper_walker;
        void* wrapper_arg;
    } walker_data;

    walker_data.wrapper_walker = walker;
    walker_data.wrapper_arg = arg;

    /* Acquire reader lock for thread safety */
    struct rm_priotracker tracker;
    rm_rlock(&rh->lock, &tracker);

    /* Walk the radix tree using REAL FreeBSD radix functions */
    int result = rh->rnh->rnh_walktree(&rh->rnh->rh, radix_walker_adapter, &walker_data);

    rm_runlock(&rh->lock, &tracker);

    printf("[ROUTE_LIB] Route walk completed using REAL radix tree: result=%d\n", result);
    return result;
}

/* Radix tree operations (low-level interface) */
struct radix_node_head* radix_node_head_create(void) {
    printf("[STUB] radix_node_head_create\n");

    struct radix_node_head *rnh = NULL;
    if (!rn_inithead((void**)&rnh, 32)) {
        return NULL;
    }

    return rnh;
}

void radix_node_head_destroy(struct radix_node_head* rnh) {
    printf("[STUB] radix_node_head_destroy: rnh=%p\n", (void*)rnh);

    if (rnh) {
        rn_detachhead((void**)&rnh);
    }
}

/* Statistics and debugging */
int route_get_stats(struct rib_head* rh, struct route_stats* stats) {
    printf("[STUB] route_get_stats: rh=%p\n", (void*)rh);

    if (!stats) {
        return -1; /* Error */
    }

    *stats = global_stats;
    return 0; /* Success */
}

void route_print_table(struct rib_head* rh) {
    printf("[STUB] route_print_table: rh=%p\n", (void*)rh);
    printf("  Stub routing table contents (no real routes)\n");
}

int route_validate_table(struct rib_head* rh) {
    printf("[STUB] route_validate_table: rh=%p\n", (void*)rh);
    return 0; /* Always valid in stub mode */
}