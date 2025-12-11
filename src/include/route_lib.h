/*
 * FreeBSD Routing Library - Public API
 *
 * This header defines the public interface for the FreeBSD routing library
 * ported to userland for testing and experimentation.
 */

#ifndef _ROUTE_LIB_H_
#define _ROUTE_LIB_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct radix_node_head;
struct route_entry;
struct rib_head;

/* Route library initialization and cleanup */
int route_lib_init(void);
void route_lib_cleanup(void);

/* Routing table management */
struct rib_head* route_table_create(int family, u_int fibnum);
void route_table_destroy(struct rib_head* rh);

/* Route operations */
typedef enum {
    RT_OP_ADD,
    RT_OP_DELETE,
    RT_OP_CHANGE,
    RT_OP_GET
} route_op_t;

struct route_info {
    struct sockaddr* ri_dst;      /* Destination address */
    struct sockaddr* ri_netmask;  /* Network mask */
    struct sockaddr* ri_gateway;  /* Gateway address */
    int ri_flags;                 /* Route flags */
    int ri_ifindex;              /* Interface index */
    u_int ri_fibnum;             /* FIB number */
};

/* Route manipulation functions */
int route_add(struct rib_head* rh, struct route_info* ri);
int route_delete(struct rib_head* rh, struct sockaddr* dst, struct sockaddr* netmask);
int route_lookup(struct rib_head* rh, struct sockaddr* dst, struct route_info* ri_out);
int route_change(struct rib_head* rh, struct route_info* ri);

/* Route enumeration */
typedef int (*route_walker_f)(struct route_info* ri, void* arg);
int route_walk(struct rib_head* rh, route_walker_f walker, void* arg);

/* Radix tree operations (low-level interface) */
struct radix_node_head* radix_node_head_create(void);
void radix_node_head_destroy(struct radix_node_head* rnh);

/* Statistics and debugging */
struct route_stats {
    u_long rs_lookups;     /* Number of lookups */
    u_long rs_hits;        /* Number of hits */
    u_long rs_misses;      /* Number of misses */
    u_long rs_adds;        /* Number of adds */
    u_long rs_deletes;     /* Number of deletes */
    u_long rs_changes;     /* Number of changes */
    u_long rs_nodes;       /* Number of nodes */
};

int route_get_stats(struct rib_head* rh, struct route_stats* stats);
void route_print_table(struct rib_head* rh);
int route_validate_table(struct rib_head* rh);

/* Error codes */
#define ROUTE_OK         0
#define ROUTE_EINVAL    -1
#define ROUTE_ENOENT    -2
#define ROUTE_EEXIST    -3
#define ROUTE_ENOMEM    -4
#define ROUTE_ENOTSUPP  -5

/* Route flags (compatible with FreeBSD RTF_* flags) */
#define ROUTE_RTF_UP         0x1     /* Route usable */
#define ROUTE_RTF_GATEWAY    0x2     /* Destination is a gateway */
#define ROUTE_RTF_HOST       0x4     /* Host entry (net otherwise) */
#define ROUTE_RTF_REJECT     0x8     /* Host or net unreachable */
#define ROUTE_RTF_DYNAMIC    0x10    /* Created dynamically by redirect */
#define ROUTE_RTF_MODIFIED   0x20    /* Modified dynamically by redirect */
#define ROUTE_RTF_BLACKHOLE  0x1000  /* Just discard pkts (during updates) */
#define ROUTE_RTF_PROTO1     0x8000  /* Protocol specific routing flag */
#define ROUTE_RTF_PROTO2     0x4000  /* Protocol specific routing flag */
#define ROUTE_RTF_PROTO3     0x40000 /* Protocol specific routing flag */

#ifdef __cplusplus
}
#endif

#endif /* _ROUTE_LIB_H_ */