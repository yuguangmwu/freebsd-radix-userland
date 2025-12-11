/*
 * FreeBSD Kernel Compatibility Layer for Userland
 *
 * This file provides userland implementations of FreeBSD kernel
 * functions and macros needed for routing code compilation.
 */

#ifndef _KERNEL_COMPAT_H_
#define _KERNEL_COMPAT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

/* Basic FreeBSD types */
#ifndef u_char
typedef uint8_t u_char;
#endif
#ifndef u_short
typedef uint16_t u_short;
#endif
#ifndef u_int
typedef uint32_t u_int;
#endif
/* Note: u_long is already defined in macOS sys/types.h as unsigned long */
typedef unsigned int rt_gen_t;

/* caddr_t compatibility */
#ifndef caddr_t
typedef char* caddr_t;
#endif
typedef const char* c_caddr_t;

/* Memory allocation */
#define M_WAITOK    0x0000
#define M_NOWAIT    0x0001
#define M_ZERO      0x0100

#define malloc(size, type, flags) user_malloc(size, flags)
#define free(ptr, type) user_free(ptr)

void* user_malloc(size_t size, int flags);
void user_free(void* ptr);

/* Atomic operations */
#define atomic_add_int(ptr, val) __sync_add_and_fetch(ptr, val)
#define atomic_subtract_int(ptr, val) __sync_sub_and_fetch(ptr, val)

/* Locking primitives */
typedef pthread_mutex_t rmlock_t;
typedef pthread_mutex_t mtx_t;

struct rm_priotracker {
    int dummy;
};

#define rm_init_flags(lock, name, flags) pthread_mutex_init(lock, NULL)
#define rm_destroy(lock) pthread_mutex_destroy(lock)
#define rm_rlock(lock, tracker) pthread_mutex_lock(lock)
#define rm_runlock(lock, tracker) pthread_mutex_unlock(lock)
#define rm_wlock(lock) pthread_mutex_lock(lock)
#define rm_wunlock(lock) pthread_mutex_unlock(lock)
#define rm_assert(lock, what) ((void)0)

#define RM_DUPOK 0x01
#define RA_LOCKED 0x01
#define RA_WLOCKED 0x02

/* Kernel printf and logging */
#define printf user_printf
int user_printf(const char* fmt, ...);

#define log(level, fmt, ...) user_printf(fmt, ##__VA_ARGS__)
#define LOG_INFO 1

/* System parameters */
extern int maxfib;

/* VNET (Virtual Network) support */
struct vnet;
#define VNET(sym) sym
#define V_rt_tables rt_tables

/* Network interface stubs */
struct ifnet {
    char if_xname[16];
    int if_index;
    int if_flags;
    int if_mtu;
};

/* Counter support */
typedef uint64_t counter_u64_t;
#define counter_u64_alloc(flags) calloc(1, sizeof(uint64_t))
#define counter_u64_free(counter) free(counter)
#define counter_u64_add(counter, val) (*counter) += (val)
#define counter_u64_fetch(counter) (*counter)

/* Callout support */
struct callout {
    int c_flags;
    void (*c_func)(void*);
    void* c_arg;
};

#define callout_init(c, flag) memset(c, 0, sizeof(*c))
#define callout_stop(c) ((void)0)
#define callout_reset(c, to, func, arg) ((void)0)

/* Sysctl support */
#define SYSCTL_NODE(parent, nbr, name, access, handler, descr)
#define SYSCTL_UINT(parent, nbr, name, access, ptr, val, descr)
#define SYSCTL_INT(parent, nbr, name, access, ptr, val, descr)
#define CTLFLAG_RW 0x01
#define CTLFLAG_VNET 0x02

/* Epoch (RCU-like) support */
typedef struct epoch* epoch_t;
typedef struct {} epoch_tracker_t;

extern epoch_t net_epoch_preempt;

#define NET_EPOCH_ENTER(et) ((void)0)
#define NET_EPOCH_EXIT(et) ((void)0)
#define NET_EPOCH_WAIT() ((void)0)
#define NET_EPOCH_CALL(func, arg) func(arg)
#define NET_EPOCH_ASSERT() ((void)0)

#define epoch_enter_preempt(epoch, et) ((void)0)
#define epoch_exit_preempt(epoch, et) ((void)0)

/* CK (Concurrency Kit) support */
#define CK_STAILQ_HEAD(name, type) struct name { struct type* stqh_first; }
#define CK_STAILQ_INIT(head) ((head)->stqh_first = NULL)
#define CK_STAILQ_EMPTY(head) ((head)->stqh_first == NULL)

/* Device control */
#define devctl_notify(subsystem, type, subtype, data) ((void)0)

/* Kernel assertions */
#ifdef DEBUG
#define KASSERT(exp, msg) assert(exp)
#else
#define KASSERT(exp, msg) ((void)0)
#endif

/* Kernel optimization hints */
#define __predict_false(exp) (exp)
#define __predict_true(exp) (exp)

/* Time support */
typedef int64_t sbintime_t;
extern volatile time_t time_second;

/* Misc kernel functions */
#define bootverbose 0
#define cold 0
#define hz 1000

/* Socket buffer operations */
struct sockaddr;
int sa_equal(const struct sockaddr* a, const struct sockaddr* b);

/* Error definitions already in errno.h */

#endif /* _KERNEL_COMPAT_H_ */