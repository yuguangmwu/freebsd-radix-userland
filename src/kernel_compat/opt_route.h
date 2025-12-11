/*
 * opt_route.h - FreeBSD kernel option configuration for routing
 * Userland compatibility stub
 */

#ifndef _OPT_ROUTE_H_
#define _OPT_ROUTE_H_

/* Enable routing functionality in userland */
#define ROUTETABLES 1

/* Enable route aging functionality */
#define ROUTE_MPATH 1

#endif /* _OPT_ROUTE_H_ */