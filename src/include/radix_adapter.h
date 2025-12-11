/*
 * Radix Tree Adapter Header
 */

#ifndef _RADIX_ADAPTER_H_
#define _RADIX_ADAPTER_H_

#include "compat_shim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct radix_node;
struct radix_node_head;

/* Adapter functions for FreeBSD radix tree */

/* Initialize a radix tree head */
struct radix_node_head* userland_rn_inithead(void);

/* Destroy a radix tree head */
void userland_rn_destroy(struct radix_node_head *rnh);

/* Add a route to the radix tree */
struct radix_node* userland_rn_addroute(struct radix_node_head *rnh,
                                        struct sockaddr *dst,
                                        struct sockaddr *mask);

/* Look up a route in the radix tree */
struct radix_node* userland_rn_lookup(struct radix_node_head *rnh,
                                      struct sockaddr *dst);

/* Delete a route from the radix tree */
struct radix_node* userland_rn_delete(struct radix_node_head *rnh,
                                      struct sockaddr *dst,
                                      struct sockaddr *mask);

/* Walk the radix tree */
int userland_rn_walktree(struct radix_node_head *rnh,
                        int (*walker)(struct radix_node *, void *),
                        void *arg);

/* Helper functions */
int userland_make_sockaddr_in(const char *ip_str, struct sockaddr_in *addr);
int userland_make_netmask(int prefixlen, struct sockaddr_in *mask);

#ifdef __cplusplus
}
#endif

#endif /* _RADIX_ADAPTER_H_ */