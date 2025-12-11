/*
 * FreeBSD System Includes Compatibility
 *
 * This file provides userland replacements for FreeBSD kernel headers
 */

#ifndef _SYS_COMPAT_H_
#define _SYS_COMPAT_H_

/* Standard system includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

/* Fake kernel includes that don't exist in userland */

/* sys/param.h equivalents */
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef __offsetof
#define __offsetof(type, field) offsetof(type, field)
#endif

/* sys/systm.h equivalents */
#define CTASSERT(x) _Static_assert(x, #x)

/* sys/mbuf.h stubs */
struct mbuf;

/* sys/lock.h stubs */
struct lock_object {
    const char* lo_name;
};

/* sys/proc.h stubs */
struct proc;
struct thread;
extern struct thread* curthread;

/* sys/kernel.h stubs */
enum sysinit_elem_order {
    SI_ORDER_FIRST = 0x0000000,
    SI_ORDER_SECOND = 0x0000001,
    SI_ORDER_THIRD = 0x0000002,
    SI_ORDER_FOURTH = 0x0000003,
    SI_ORDER_MIDDLE = 0x1000000,
    SI_ORDER_ANY = 0xfffffff
};

enum sysinit_sub_id {
    SI_SUB_DUMMY = 0x0000000,
    SI_SUB_PROTO_BEGIN = 0x1000000,
    SI_SUB_PROTO_IF = 0x1000100,
};

typedef void (*sysinit_cfunc_t)(const void*);

#define SYSINIT(uniquifier, subsystem, order, func, ident)

/* sys/domain.h stubs */
struct domain;
struct protosw;

/* net/if.h stubs */
#define IFF_UP          0x1
#define IFF_BROADCAST   0x2
#define IFF_DEBUG       0x4
#define IFF_LOOPBACK    0x8
#define IFF_POINTOPOINT 0x10

/* net/if_var.h stubs */
struct ifaddr;
struct ifmultiaddr;

/* net/vnet.h stubs */
#define VNET_DECLARE(type, name) extern type name
#define VNET_DEFINE(type, name) type name

#endif /* _SYS_COMPAT_H_ */