/*
 * FreeBSD Routing Library Userland Adapter
 *
 * This header adapts FreeBSD kernel routing code for userland compilation
 */

#ifndef _FREEBSD_ROUTE_ADAPTER_H_
#define _FREEBSD_ROUTE_ADAPTER_H_

/* Ensure we're in userland mode */
#undef _KERNEL

/* Include our compatibility layer first */
#include "../kernel_compat/kernel_compat.h"
#include "../kernel_compat/sys_compat.h"

/* Define FreeBSD kernel options that affect compilation */
#define opt_inet_h_   1
#define opt_inet6_h_  1
#define opt_mrouting_h_ 1
#define opt_route_h_ 1

/* Redefine kernel includes to our compatibility layer */
#define sys_param_h_ 1
#define sys_systm_h_ 1
#define sys_malloc_h_ 1
#define sys_mbuf_h_ 1
#define sys_socket_h_ 1
#define sys_sysctl_h_ 1
#define sys_syslog_h_ 1
#define sys_sysproto_h_ 1
#define sys_proc_h_ 1
#define sys_devctl_h_ 1
#define sys_domain_h_ 1
#define sys_eventhandler_h_ 1
#define sys_kernel_h_ 1
#define sys_lock_h_ 1
#define sys_rmlock_h_ 1
#define sys_ck_h_ 1
#define sys_epoch_h_ 1
#define sys_counter_h_ 1

/* Network includes */
#define net_if_h_ 1
#define net_if_var_h_ 1
#define net_if_dl_h_ 1
#define net_ethernet_h_ 1
#define net_pfkeyv2_h_ 1
#define net_route_h_ 1
#define net_radix_h_ 1
#define net_vnet_h_ 1
#define netinet_in_h_ 1
#define netinet6_in6_var_h_ 1

/* Disable features we don't need/support in userland */
#undef INET
#undef INET6
#undef MROUTING

/* FreeBSD memory allocation types (unused in userland but needed for compilation) */
#define MALLOC_DECLARE(type)  /* Empty declaration for userland */
MALLOC_DECLARE(M_RTABLE);
#define M_RTABLE ((void*)1)
#define M_IFADDR ((void*)2)
#define M_IFMADDR ((void*)3)

/* Thread/process stubs */
struct thread {
    int dummy;
};
struct proc {
    int dummy;
};

extern struct thread* curthread;

/* Additional stubs for missing functionality */
struct eventhandler_entry {
    void* dummy;
};

#define EVENTHANDLER_REGISTER(name, func, arg, priority) ((void)0)
#define EVENTHANDLER_DEREGISTER(name, tag) ((void)0)

/* Module initialization stubs */
#define DECLARE_MODULE(name, data, sub, order)
#define MODULE_VERSION(name, version)

/* DDB (debugger) stubs */
#define DB_COMMAND(cmd_name, func_name) void func_name(void)
#define DB_SHOW_COMMAND(cmd_name, func_name) void func_name(void)

/* ifnet (network interface) stubs we might need */
struct ifaddr {
    struct sockaddr* ifa_addr;
    struct sockaddr* ifa_netmask;
    struct ifnet* ifa_ifp;
    int ifa_flags;
};

/* Network buffer stubs */
struct mbuf {
    struct mbuf* m_next;
    int m_len;
    int m_flags;
    caddr_t m_data;
};

/* Interface link layer entry stubs */
struct llentry {
    struct sockaddr_storage ll_addr;
    struct ifnet* lle_ifp;
    int lle_state;
};

/* Some functions we need to stub out */
#define ifnet_byindex(idx) NULL
#define if_ref(ifp) ((void)0)
#define if_rele(ifp) ((void)0)

/* Disable complex kernel features */
#define VIMAGE 1
#define CURVNET_SET(vnet) ((void)0)
#define CURVNET_RESTORE() ((void)0)

#endif /* _FREEBSD_ROUTE_ADAPTER_H_ */