/*
 * Kernel Compatibility Shim - Clean Header Firewall
 *
 * This header establishes a clean boundary between userland and FreeBSD kernel code.
 * Strategy: Include all userland headers FIRST, then carefully map kernel concepts.
 */

#ifndef _KERNEL_COMPAT_SHIM_H_
#define _KERNEL_COMPAT_SHIM_H_

/* === Phase 1: Include ALL userland headers FIRST === */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

/* Network headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Platform specific */
#ifdef __APPLE__
#include <pthread.h>
#endif

/* === Phase 2: Force kernel mode for FreeBSD code === */
#ifndef _KERNEL
#define _KERNEL 1
#endif

/* === Phase 3: Handle type conflicts with aliases === */
/* Never redefine types that exist in userland - use aliases + macros */

/* BSD types that might conflict with userland */
#ifndef _BSD_TYPES_DEFINED_
#define _BSD_TYPES_DEFINED_

typedef uint8_t     bsd_u_char;
typedef uint16_t    bsd_u_short;
typedef uint32_t    bsd_u_int;
typedef char*       bsd_caddr_t;
typedef const char* bsd_c_caddr_t;
typedef unsigned int bsd_rt_gen_t;

/* Map FreeBSD kernel names to our safe aliases */
#define u_char      bsd_u_char
#define u_short     bsd_u_short
#define u_int       bsd_u_int
#define caddr_t     bsd_caddr_t
#define c_caddr_t   bsd_c_caddr_t
#define rt_gen_t    bsd_rt_gen_t

/* Note: u_long exists in macOS sys/types.h, so we DON'T redefine it */

#endif /* _BSD_TYPES_DEFINED_ */

/* === Phase 4: Memory Management Shim === */
#define M_WAITOK    0x0000
#define M_NOWAIT    0x0001
#define M_ZERO      0x0100

/* Memory type tags (ignored in userland) */
#define M_RTABLE    1
#define M_IFADDR    2
#define M_IFMADDR   3

/* Simple wrapper to handle M_ZERO flag */
static inline void* bsd_malloc(size_t size, int type, int flags) {
    (void)type; /* Ignored in userland */
    void* ptr = malloc(size);
    if (ptr && (flags & M_ZERO)) {
        memset(ptr, 0, size);
    }
    if (!ptr && (flags & M_WAITOK)) {
        fprintf(stderr, "FATAL: malloc failed\n");
        abort();
    }
    return ptr;
}

static inline void bsd_free(void* ptr, int type) {
    (void)type; /* Ignored in userland */
    free(ptr);
}

/* Override FreeBSD malloc/free */
#undef malloc
#undef free
#define malloc(size, type, flags) bsd_malloc(size, type, flags)
#define free(ptr, type) bsd_free(ptr, type)

/* === Phase 5: Locking Primitives (No-op for single-threaded) === */
struct rmlock { int dummy; };
struct rm_priotracker { int dummy; };
struct mtx { int dummy; };

#define rm_init_flags(lock, name, flags) do { (void)(lock); } while(0)
#define rm_destroy(lock) do { (void)(lock); } while(0)
#define rm_rlock(lock, tracker) do { (void)(lock); (void)(tracker); } while(0)
#define rm_runlock(lock, tracker) do { (void)(lock); (void)(tracker); } while(0)
#define rm_wlock(lock) do { (void)(lock); } while(0)
#define rm_wunlock(lock) do { (void)(lock); } while(0)
#define rm_assert(lock, what) do { (void)(lock); (void)(what); } while(0)

#define RIB_RLOCK(rh) do { (void)(rh); } while(0)
#define RIB_RUNLOCK(rh) do { (void)(rh); } while(0)
#define RIB_WLOCK(rh) do { (void)(rh); } while(0)
#define RIB_WUNLOCK(rh) do { (void)(rh); } while(0)
#define RIB_LOCK_ASSERT(rh) do { (void)(rh); } while(0)
#define RIB_WLOCK_ASSERT(rh) do { (void)(rh); } while(0)

/* Lock flags */
#define RM_DUPOK    0x01
#define RA_LOCKED   0x01
#define RA_WLOCKED  0x02

/* === Phase 6: Kernel Macros === */
#define KASSERT(exp, msg) assert(exp)
#define MPASS(exp) assert(exp)

/* Kernel optimization hints */
#define __predict_false(exp) (exp)
#define __predict_true(exp) (exp)

/* Disable problematic kernel logging */
#undef log
#define log(level, fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define panic(msg) do { fprintf(stderr, "PANIC: %s\n", msg); abort(); } while(0)

/* Utility macros */
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#define bootverbose 0
#define cold 0
#define hz 1000

/* === Phase 7: List/Queue Macros (Minimal BSD queue.h subset) === */
#ifndef _BSD_QUEUE_MINIMAL_
#define _BSD_QUEUE_MINIMAL_

#define LIST_HEAD(name, type) \
    struct name { struct type *lh_first; }

#define LIST_ENTRY(type) \
    struct { struct type *le_next; struct type **le_prev; }

#define STAILQ_HEAD(name, type) \
    struct name { struct type *stqh_first; struct type **stqh_last; }

#define STAILQ_ENTRY(type) \
    struct { struct type *stqe_next; }

#endif /* _BSD_QUEUE_MINIMAL_ */

/* === Phase 8: System/Network Stubs === */
extern int maxfib;
extern volatile time_t time_second;

/* Minimal ifnet stub */
struct ifnet {
    char if_xname[16];
    int if_index;
    int if_flags;
    int if_mtu;
};

/* Minimal ifaddr stub */
struct ifaddr {
    struct sockaddr* ifa_addr;
    struct sockaddr* ifa_netmask;
    struct ifnet* ifa_ifp;
    int ifa_flags;
};

/* Network interface stubs */
#define ifnet_byindex(idx) ((struct ifnet*)NULL)
#define if_ref(ifp) do { (void)(ifp); } while(0)
#define if_rele(ifp) do { (void)(ifp); } while(0)

/* === Phase 9: VNET/Epoch Support (No-op) === */
struct vnet;
struct epoch;
typedef struct epoch* epoch_t;
typedef struct {} epoch_tracker_t;

extern epoch_t net_epoch_preempt;

#define VNET(sym) sym
#define VNET_DECLARE(type, name) extern type name
#define VNET_DEFINE(type, name) type name

#define NET_EPOCH_ENTER(et) do { (void)(et); } while(0)
#define NET_EPOCH_EXIT(et) do { (void)(et); } while(0)
#define NET_EPOCH_WAIT() do { } while(0)
#define NET_EPOCH_CALL(func, arg) func(arg)
#define NET_EPOCH_ASSERT() do { } while(0)

#define epoch_enter_preempt(epoch, et) do { (void)(epoch); (void)(et); } while(0)
#define epoch_exit_preempt(epoch, et) do { (void)(epoch); (void)(et); } while(0)

#define CURVNET_SET(vnet) do { (void)(vnet); } while(0)
#define CURVNET_RESTORE() do { } while(0)

/* === Phase 10: Counter Support === */
typedef uint64_t* counter_u64_t;

#define counter_u64_alloc(flags) ((counter_u64_t)calloc(1, sizeof(uint64_t)))
#define counter_u64_free(counter) free(counter)
#define counter_u64_add(counter, val) (*(counter) += (val))
#define counter_u64_fetch(counter) (*(counter))

/* === Phase 11: Callout Support === */
struct callout {
    int c_flags;
    void (*c_func)(void*);
    void* c_arg;
};

#define callout_init(c, flag) memset(c, 0, sizeof(*(c)))
#define callout_stop(c) do { (void)(c); } while(0)
#define callout_reset(c, to, func, arg) do { (void)(c); (void)(to); (void)(func); (void)(arg); } while(0)

/* === Phase 12: Sysctl Support === */
#define SYSCTL_NODE(parent, nbr, name, access, handler, descr)
#define SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr)
#define SYSCTL_INT(parent, nbr, name, access, ptr, val, descr)
#define CTLFLAG_RW 0x01
#define CTLFLAG_VNET 0x02

/* === Phase 13: Device Control === */
#define devctl_notify(subsystem, type, subtype, data) do { } while(0)

/* === Phase 14: Module System (No-op) === */
#define DECLARE_MODULE(name, data, sub, order)
#define MODULE_VERSION(name, version)
#define SYSINIT(uniquifier, subsystem, order, func, ident)

/* === Phase 15: DDB (Debugger) Stubs === */
#define DB_COMMAND(cmd_name, func_name) static void func_name(void)
#define DB_SHOW_COMMAND(cmd_name, func_name) static void func_name(void)

/* === Phase 16: Event Handler Stubs === */
struct eventhandler_entry {
    void* dummy;
};

#define EVENTHANDLER_REGISTER(name, func, arg, priority) ((void)0)
#define EVENTHANDLER_DEREGISTER(name, tag) ((void)0)

/* === Phase 17: Threading Stubs === */
struct thread {
    int dummy;
};

struct proc {
    int dummy;
};

extern struct thread* curthread;

/* Initialize global variables */
void kernel_compat_init(void);

/* Socket address utilities */
int sa_equal(const struct sockaddr* a, const struct sockaddr* b);

#endif /* _KERNEL_COMPAT_SHIM_H_ */