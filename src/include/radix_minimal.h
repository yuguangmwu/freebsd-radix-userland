/*
 * Minimal Radix Tree Public API
 */

#ifndef _RADIX_MINIMAL_H_
#define _RADIX_MINIMAL_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize the radix tree subsystem */
int radix_init(void);

/* Cleanup the radix tree subsystem */
void radix_cleanup(void);

/* Add a route to the radix tree */
int radix_add_route(const char *dest_ip, int prefixlen, const char *gateway_ip);

/* Look up a route in the radix tree */
int radix_lookup_route(const char *dest_ip, char *gateway_buf, size_t gateway_buflen);

/* Delete a route from the radix tree */
int radix_delete_route(const char *dest_ip, int prefixlen);

/* Walk through all routes */
int radix_walk_routes(int (*walker)(const char *dest_ip, int prefixlen,
                                   const char *gateway_ip, void *arg),
                     void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _RADIX_MINIMAL_H_ */