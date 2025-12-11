/*
 * Debug 1M Route Failures
 *
 * Investigate the 6 deletion failures in the 1M route test
 */

#include "src/test/test_framework.h"
#include "src/kernel_compat/compat_shim.h"
#include "src/freebsd/radix.h"
#include <arpa/inet.h>
#include <time.h>

/* Use the exact same route generation logic from the scale test */
static struct sockaddr_in *make_route_key(uint32_t route_id) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) return NULL;

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;

    /* Sequential pattern - exactly as in the scale test */
    if (route_id < 65536) {
        sa->sin_addr.s_addr = htonl(0x0A000000 | (route_id << 8));
    } else if (route_id < 131072) {
        uint32_t offset = route_id - 65536;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xAC000000 | (major << 16) | (minor << 8));
    } else if (route_id < 196608) {
        uint32_t offset = route_id - 131072;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xC1000000 | (major << 16) | (minor << 8));
    } else if (route_id < 262144) {
        uint32_t offset = route_id - 196608;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xCC000000 | (major << 16) | (minor << 8));
    } else if (route_id < 327680) {
        uint32_t offset = route_id - 262144;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xCD000000 | (major << 16) | (minor << 8));
    } else if (route_id < 393216) {
        uint32_t offset = route_id - 327680;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xCE000000 | (major << 16) | (minor << 8));
    } else if (route_id < 458752) {
        uint32_t offset = route_id - 393216;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xCF000000 | (major << 16) | (minor << 8));
    } else if (route_id < 524288) {
        uint32_t offset = route_id - 458752;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD0000000 | (major << 16) | (minor << 8));
    } else if (route_id < 589824) {
        uint32_t offset = route_id - 524288;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD1000000 | (major << 16) | (minor << 8));
    } else if (route_id < 655360) {
        uint32_t offset = route_id - 589824;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD2000000 | (major << 16) | (minor << 8));
    } else if (route_id < 720896) {
        uint32_t offset = route_id - 655360;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD3000000 | (major << 16) | (minor << 8));
    } else if (route_id < 786432) {
        uint32_t offset = route_id - 720896;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD4000000 | (major << 16) | (minor << 8));
    } else if (route_id < 851968) {
        uint32_t offset = route_id - 786432;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD5000000 | (major << 16) | (minor << 8));
    } else if (route_id < 917504) {
        uint32_t offset = route_id - 851968;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD6000000 | (major << 16) | (minor << 8));
    } else if (route_id < 983040) {
        uint32_t offset = route_id - 917504;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD7000000 | (major << 16) | (minor << 8));
    } else if (route_id < 1048576) {
        uint32_t offset = route_id - 983040;
        uint32_t major = offset / 256;
        uint32_t minor = offset % 256;
        sa->sin_addr.s_addr = htonl(0xD8000000 | (major << 16) | (minor << 8));
    } else {
        uint32_t range = route_id / 65536;
        uint32_t offset = route_id % 65536;
        sa->sin_addr.s_addr = htonl(0xF0000000 | ((range & 0xFF) << 16) | (offset << 8));
    }

    return sa;
}

static struct sockaddr_in *make_route_mask(void) {
    struct sockaddr_in *sa = bsd_malloc(sizeof(*sa), M_RTABLE, M_WAITOK | M_ZERO);
    if (!sa) return NULL;

    sa->sin_len = sizeof(*sa);
    sa->sin_family = AF_INET;
    sa->sin_addr.s_addr = htonl(0xFFFFFF00);  /* /24 mask */

    return sa;
}

/* Tree walk callback to identify remaining nodes */
struct debug_walk_ctx {
    int total_count;
    int leaf_count;
    uint32_t failed_route_ids[10];  /* Store up to 10 failed route IDs */
    int failed_count;
};

static int debug_walk_callback(struct radix_node *rn, void *arg) {
    struct debug_walk_ctx *ctx = (struct debug_walk_ctx*)arg;
    ctx->total_count++;

    if (rn->rn_key) {
        struct sockaddr_in *sa = (struct sockaddr_in*)rn->rn_key;
        ctx->leaf_count++;

        printf("Remaining node: %s/24 (flags=0x%x)\n",
               inet_ntoa(sa->sin_addr), rn->rn_flags);

        /* Try to reverse-engineer the route_id from the IP */
        uint32_t addr = ntohl(sa->sin_addr.s_addr);
        uint32_t route_id = 0;

        if ((addr & 0xFF000000) == 0x0A000000) {  /* 10.x.x.0 */
            route_id = (addr >> 8) & 0xFFFF;
        } else if ((addr & 0xFF000000) == 0xAC000000) {  /* 172.x.x.0 */
            uint32_t major = (addr >> 16) & 0xFF;
            uint32_t minor = (addr >> 8) & 0xFF;
            route_id = 65536 + (major * 256) + minor;
        } else if ((addr & 0xFF000000) == 0xC1000000) {  /* 193.x.x.0 */
            uint32_t major = (addr >> 16) & 0xFF;
            uint32_t minor = (addr >> 8) & 0xFF;
            route_id = 131072 + (major * 256) + minor;
        } else {
            printf("  -> Unknown address range, can't determine route_id\n");
        }

        if (ctx->failed_count < 10) {
            ctx->failed_route_ids[ctx->failed_count] = route_id;
            ctx->failed_count++;
        }

        printf("  -> Estimated route_id: %u\n", route_id);
    }

    return 0;
}

int main(void) {
    printf("=== Debugging 1M Route Deletion Failures ===\n\n");

    /* Initialize */
    kernel_compat_init();

    struct radix_node_head *rnh = NULL;
    int offset = offsetof(struct sockaddr_in, sin_addr);

    if (rn_inithead((void **)&rnh, offset) != 1) {
        printf("❌ rn_inithead failed\n");
        return 1;
    }

    printf("Phase 1: Adding 1M routes (abbreviated)...\n");

    int successful_adds = 0;
    uint32_t *added_routes = malloc(1000000 * sizeof(uint32_t));  /* Track which routes were added */

    /* Add routes with progress tracking */
    for (uint32_t i = 0; i < 1000000; i++) {
        struct sockaddr_in *key = make_route_key(i);
        struct sockaddr_in *mask = make_route_mask();

        if (key && mask) {
            struct radix_node *nodes = bsd_malloc(2 * sizeof(struct radix_node), M_RTABLE, M_WAITOK | M_ZERO);
            if (nodes) {
                struct radix_node *rn = rnh->rnh_addaddr(key, mask, &rnh->rh, nodes);
                if (rn) {
                    added_routes[successful_adds] = i;
                    successful_adds++;
                } else {
                    bsd_free(nodes, M_RTABLE);
                    bsd_free(key, M_RTABLE);
                    bsd_free(mask, M_RTABLE);
                }
            }
        }

        if ((i + 1) % 100000 == 0) {
            printf("  Added %d routes so far...\n", successful_adds);
        }
    }

    printf("✅ Added %d/1000000 routes\n\n", successful_adds);

    printf("Phase 2: Deleting routes (reverse order)...\n");

    int successful_deletes = 0;
    int failed_deletes = 0;
    uint32_t failed_route_ids[20];  /* Track failed deletions */

    /* Delete in reverse order, exactly like the scale test */
    for (int i = successful_adds - 1; i >= 0; i--) {
        uint32_t route_id = added_routes[i];
        struct sockaddr_in *del_key = make_route_key(route_id);
        struct sockaddr_in *del_mask = make_route_mask();

        if (del_key && del_mask) {
            struct radix_node *deleted = rnh->rnh_deladdr(del_key, del_mask, &rnh->rh);
            if (deleted) {
                successful_deletes++;
            } else {
                if (failed_deletes < 20) {
                    failed_route_ids[failed_deletes] = route_id;
                }
                failed_deletes++;
                if (failed_deletes <= 10) {  /* Only print first 10 failures */
                    printf("❌ Failed to delete route_id %u (%s/24)\n",
                           route_id, inet_ntoa(del_key->sin_addr));
                }
            }
        }

        if (del_key) bsd_free(del_key, M_RTABLE);
        if (del_mask) bsd_free(del_mask, M_RTABLE);
    }

    printf("\n✅ Deleted %d/%d routes\n", successful_deletes, successful_adds);
    printf("❌ Failed to delete %d routes\n\n", failed_deletes);

    printf("Phase 3: Analyzing remaining nodes...\n");

    struct debug_walk_ctx ctx = {0};
    rnh->rnh_walktree(&rnh->rh, debug_walk_callback, &ctx);

    printf("\n=== Analysis Results ===\n");
    printf("Total remaining nodes: %d\n", ctx.total_count);
    printf("Leaf nodes (routes): %d\n", ctx.leaf_count);
    printf("Internal nodes: %d\n", ctx.total_count - ctx.leaf_count);

    if (ctx.failed_count > 0) {
        printf("\nRemaining route IDs: ");
        for (int i = 0; i < ctx.failed_count; i++) {
            printf("%u ", ctx.failed_route_ids[i]);
        }
        printf("\n");

        /* Test if these routes can be deleted with fresh keys */
        printf("\nTesting fresh deletion attempts...\n");
        for (int i = 0; i < ctx.failed_count && i < 5; i++) {
            uint32_t route_id = ctx.failed_route_ids[i];
            struct sockaddr_in *fresh_key = make_route_key(route_id);
            struct sockaddr_in *fresh_mask = make_route_mask();

            if (fresh_key && fresh_mask) {
                printf("Attempting fresh delete of route_id %u (%s/24)...\n",
                       route_id, inet_ntoa(fresh_key->sin_addr));

                struct radix_node *deleted = rnh->rnh_deladdr(fresh_key, fresh_mask, &rnh->rh);
                if (deleted) {
                    printf("  ✅ SUCCESS with fresh key!\n");
                } else {
                    printf("  ❌ Still failed with fresh key\n");
                }

                bsd_free(fresh_key, M_RTABLE);
                bsd_free(fresh_mask, M_RTABLE);
            }
        }
    }

    /* Final cleanup */
    rn_detachhead((void **)&rnh);
    free(added_routes);

    printf("\n=== Conclusion ===\n");
    if (failed_deletes == ctx.leaf_count) {
        printf("All %d deletion failures correspond to remaining leaf nodes.\n", failed_deletes);
        printf("This suggests a consistent pattern in route generation or deletion logic.\n");
    } else {
        printf("Mismatch: %d deletion failures vs %d remaining leaves.\n", failed_deletes, ctx.leaf_count);
    }

    return 0;
}