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

/* === Phase 5: Level 2 Threading - Real pthread-based rmlock === */

/*
 * FreeBSD rmlock (Read-Mostly Lock) Implementation using pthread_rwlock_t
 *
 * Provides:
 * - Multiple concurrent readers
 * - Single exclusive writer
 * - Reader preference (FreeBSD rmlock behavior)
 * - Lock tracking and debugging
 */

/* Lock flags - must be defined before function implementations */
#define RM_DUPOK    0x01
#define RA_LOCKED   0x01
#define RA_WLOCKED  0x02

struct rmlock {
    pthread_rwlock_t    rw_lock;        /* Actual pthread read-write lock */
    const char         *name;           /* Lock name for debugging */
    pthread_mutex_t     stats_lock;     /* Protects statistics */
    uint32_t           readers;         /* Current reader count */
    uint32_t           writers;         /* Current writer count (0 or 1) */
    uint64_t           total_reads;     /* Total read lock acquisitions */
    uint64_t           total_writes;    /* Total write lock acquisitions */
#ifdef DEBUG_THREADING
    pthread_t          writer_thread;   /* Current writer thread */
    struct timespec    last_acquire;    /* Last lock acquire time */
#endif
};

struct rm_priotracker {
    pthread_t          thread_id;       /* Thread holding read lock */
    struct rmlock     *lock_ptr;        /* Back pointer to lock */
    struct timespec    acquire_time;    /* When read lock was acquired */
};

struct mtx {
    pthread_mutex_t    mutex;
    const char        *name;
};

/* === rmlock Function Implementations === */

static inline int
rm_init_flags(struct rmlock *rm, const char *name, int flags)
{
    int ret;

    memset(rm, 0, sizeof(*rm));
    rm->name = name ? name : "unnamed_rmlock";

    /* Initialize pthread read-write lock with reader preference */
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    #ifdef __APPLE__
    /* macOS doesn't have PTHREAD_RWLOCK_PREFER_READER_NP, but defaults to reader preference */
    #endif

    ret = pthread_rwlock_init(&rm->rw_lock, &attr);
    pthread_rwlockattr_destroy(&attr);
    if (ret != 0) {
        printf("[RMLOCK] ERROR: Failed to init rwlock '%s': %d\n", rm->name, ret);
        return ret;
    }

    ret = pthread_mutex_init(&rm->stats_lock, NULL);
    if (ret != 0) {
        pthread_rwlock_destroy(&rm->rw_lock);
        printf("[RMLOCK] ERROR: Failed to init stats lock '%s': %d\n", rm->name, ret);
        return ret;
    }

    printf("[RMLOCK] Initialized '%s' at %p (flags=0x%x)\n", rm->name, (void*)rm, flags);
    return 0;
}

static inline void
rm_destroy(struct rmlock *rm)
{
    if (rm->readers > 0 || rm->writers > 0) {
        printf("[RMLOCK] WARNING: Destroying '%s' with active locks (R:%u W:%u)\n",
               rm->name, rm->readers, rm->writers);
    }

    printf("[RMLOCK] Destroying '%s' (Total: R=%llu W=%llu)\n",
           rm->name, (unsigned long long)rm->total_reads, (unsigned long long)rm->total_writes);

    pthread_rwlock_destroy(&rm->rw_lock);
    pthread_mutex_destroy(&rm->stats_lock);
}

static inline void
rm_rlock(struct rmlock *rm, struct rm_priotracker *tracker)
{
    int ret;

    /* Acquire read lock */
    ret = pthread_rwlock_rdlock(&rm->rw_lock);
    if (ret != 0) {
        printf("[RMLOCK] FATAL: Read lock failed on '%s': %d\n", rm->name, ret);
        abort();
    }

    /* Set up tracker */
    tracker->thread_id = pthread_self();
    tracker->lock_ptr = rm;
    clock_gettime(CLOCK_MONOTONIC, &tracker->acquire_time);

    /* Update statistics */
    pthread_mutex_lock(&rm->stats_lock);
    rm->readers++;
    rm->total_reads++;
    pthread_mutex_unlock(&rm->stats_lock);

#ifdef DEBUG_THREADING_VERBOSE
    printf("[RMLOCK] Read lock acquired on '%s' by thread %lu (active readers: %u)\n",
           rm->name, (unsigned long)tracker->thread_id, rm->readers);
#endif
}

static inline void
rm_runlock(struct rmlock *rm, struct rm_priotracker *tracker)
{
    int ret;
    struct timespec now, hold_time;

    /* Calculate how long we held the lock */
    clock_gettime(CLOCK_MONOTONIC, &now);
    hold_time.tv_sec = now.tv_sec - tracker->acquire_time.tv_sec;
    hold_time.tv_nsec = now.tv_nsec - tracker->acquire_time.tv_nsec;
    if (hold_time.tv_nsec < 0) {
        hold_time.tv_sec--;
        hold_time.tv_nsec += 1000000000L;
    }

    /* Update statistics */
    pthread_mutex_lock(&rm->stats_lock);
    rm->readers--;
    pthread_mutex_unlock(&rm->stats_lock);

    /* Release read lock */
    ret = pthread_rwlock_unlock(&rm->rw_lock);
    if (ret != 0) {
        printf("[RMLOCK] FATAL: Read unlock failed on '%s': %d\n", rm->name, ret);
        abort();
    }

#ifdef DEBUG_THREADING_VERBOSE
    printf("[RMLOCK] Read lock released on '%s' by thread %lu (held %ld.%09ld sec)\n",
           rm->name, (unsigned long)tracker->thread_id, hold_time.tv_sec, hold_time.tv_nsec);
#endif
}

static inline void
rm_wlock(struct rmlock *rm)
{
    int ret;

    /* Acquire exclusive write lock */
    ret = pthread_rwlock_wrlock(&rm->rw_lock);
    if (ret != 0) {
        printf("[RMLOCK] FATAL: Write lock failed on '%s': %d\n", rm->name, ret);
        abort();
    }

    /* Update statistics */
    pthread_mutex_lock(&rm->stats_lock);
    rm->writers = 1;
    rm->total_writes++;
#ifdef DEBUG_THREADING
    rm->writer_thread = pthread_self();
    clock_gettime(CLOCK_MONOTONIC, &rm->last_acquire);
#endif
    pthread_mutex_unlock(&rm->stats_lock);

#ifdef DEBUG_THREADING_VERBOSE
    printf("[RMLOCK] Write lock acquired on '%s' by thread %lu\n",
           rm->name, (unsigned long)pthread_self());
#endif
}

static inline void
rm_wunlock(struct rmlock *rm)
{
    int ret;
    struct timespec now, hold_time;

#ifdef DEBUG_THREADING
    /* Calculate write lock hold time */
    clock_gettime(CLOCK_MONOTONIC, &now);
    hold_time.tv_sec = now.tv_sec - rm->last_acquire.tv_sec;
    hold_time.tv_nsec = now.tv_nsec - rm->last_acquire.tv_nsec;
    if (hold_time.tv_nsec < 0) {
        hold_time.tv_sec--;
        hold_time.tv_nsec += 1000000000L;
    }
#endif

    /* Update statistics */
    pthread_mutex_lock(&rm->stats_lock);
    rm->writers = 0;
    pthread_mutex_unlock(&rm->stats_lock);

    /* Release write lock */
    ret = pthread_rwlock_unlock(&rm->rw_lock);
    if (ret != 0) {
        printf("[RMLOCK] FATAL: Write unlock failed on '%s': %d\n", rm->name, ret);
        abort();
    }

#ifdef DEBUG_THREADING_VERBOSE
    printf("[RMLOCK] Write lock released on '%s' by thread %lu (held %ld.%09ld sec)\n",
           rm->name, (unsigned long)pthread_self(), hold_time.tv_sec, hold_time.tv_nsec);
#endif
}

static inline void
rm_assert(struct rmlock *rm, int what)
{
#ifdef DEBUG_THREADING
    pthread_mutex_lock(&rm->stats_lock);
    switch (what) {
    case RA_LOCKED:
        /* Assert either read or write locked */
        if (rm->readers == 0 && rm->writers == 0) {
            printf("[RMLOCK] ASSERTION FAILED: '%s' not locked\n", rm->name);
            pthread_mutex_unlock(&rm->stats_lock);
            abort();
        }
        break;
    case RA_WLOCKED:
        /* Assert write locked by current thread */
        if (rm->writers == 0) {
            printf("[RMLOCK] ASSERTION FAILED: '%s' not write locked\n", rm->name);
            pthread_mutex_unlock(&rm->stats_lock);
            abort();
        }
        if (rm->writer_thread != pthread_self()) {
            printf("[RMLOCK] ASSERTION FAILED: '%s' write locked by different thread\n", rm->name);
            pthread_mutex_unlock(&rm->stats_lock);
            abort();
        }
        break;
    }
    pthread_mutex_unlock(&rm->stats_lock);
#else
    /* No-op in non-debug builds */
    (void)rm;
    (void)what;
#endif
}

/* Keep the same RIB_* macros that FreeBSD code uses - they'll now call real locks */

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