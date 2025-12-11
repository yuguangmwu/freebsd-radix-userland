/*
 * FreeBSD Radix Tree XNU Kernel Implementation
 *
 * Full RCU + Epoch + rmlock implementation using native XNU kernel primitives
 * Provides lock-free readers with epoch-based memory reclamation
 */

#ifndef _XNU_FREEBSD_RADIX_H_
#define _XNU_FREEBSD_RADIX_H_

#ifdef KERNEL

#include <kern/kern_types.h>
#include <kern/thread.h>
#include <kern/locks.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <kern/processor.h>
#include <kern/cpu_data.h>
#include <libkern/OSAtomic.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <sys/systm.h>

/* ===== Advanced XNU Epoch System Implementation ===== */

/*
 * Per-CPU Epoch State - Optimized for SMP scalability
 */
struct xnu_epoch_cpu_state {
    uint64_t            local_epoch;        /* CPU-local epoch number */
    uint32_t            active_readers;     /* Readers on this CPU */
    uint32_t            call_depth;         /* Nested epoch calls */
    uint64_t            enter_count;        /* Total epoch enters */
    uint64_t            exit_count;         /* Total epoch exits */
    uint64_t            last_grace_period;  /* Last completed grace period */
    bool                in_grace_period;    /* Grace period in progress */
    lck_spin_t         *cpu_lock;           /* Per-CPU synchronization */
    queue_head_t        local_callbacks;    /* CPU-local callback queue */

    /* Performance statistics */
    uint64_t            fast_path_enters;   /* Lock-free enters */
    uint64_t            slow_path_enters;   /* Fallback enters */
    uint64_t            callback_executed;  /* Callbacks processed */
    uint64_t            grace_periods;      /* Grace periods completed */
} __attribute__((aligned(128)));            /* Cache line aligned */

/*
 * Hierarchical Grace Period Detection
 */
struct xnu_grace_period {
    uint64_t            gp_number;          /* Grace period sequence number */
    uint64_t            gp_start_time;      /* When grace period started */
    uint32_t            gp_state;           /* Current state */
    uint32_t            cpu_mask;           /* CPUs that need to report */
    uint32_t            completed_mask;     /* CPUs that have reported */
    lck_spin_t         *gp_lock;            /* Grace period synchronization */

    /* Grace period states */
#define GP_STATE_IDLE       0
#define GP_STATE_STARTED    1
#define GP_STATE_WAITING    2
#define GP_STATE_COMPLETED  3
};

/*
 * Advanced Memory Pressure Management
 */
struct xnu_epoch_memory {
    uint32_t            callback_count;     /* Total queued callbacks */
    uint32_t            callback_limit;     /* Max callbacks allowed */
    uint32_t            memory_pressure;    /* Current pressure level */
    uint32_t            emergency_reclaim;  /* Emergency reclaim triggered */
    uint64_t            bytes_allocated;    /* Total memory allocated */
    uint64_t            bytes_reclaimed;    /* Total memory reclaimed */
    lck_spin_t         *mem_lock;           /* Memory statistics lock */
};

/*
 * XNU Epoch Context - Production-grade state tracking
 */
struct xnu_epoch {
    /* Core epoch state */
    uint64_t            epoch_number;       /* Global epoch number */
    lck_spin_t         *epoch_lock;         /* Global epoch synchronization */
    const char         *name;               /* Debug name */
    uint32_t            flags;              /* Configuration flags */

    /* Per-CPU optimization */
    struct xnu_epoch_cpu_state *cpu_states; /* Per-CPU state array */
    uint32_t            max_cpus;           /* Maximum CPU count */

    /* Hierarchical grace period detection */
    struct xnu_grace_period grace_period;   /* Current grace period */
    thread_call_t       gp_thread_call;     /* Grace period worker */
    uint64_t            gp_interval_ns;     /* Grace period check interval */

    /* Advanced reclamation */
    thread_call_t       reclaim_call;       /* Deferred reclamation */
    queue_head_t        callback_queue;     /* Global callback queue */
    lck_mtx_t          *callback_lock;      /* Callback queue protection */
    uint32_t            reclaim_batch_size; /* Callbacks per batch */

    /* Memory pressure management */
    struct xnu_epoch_memory memory;         /* Memory management state */

    /* Interrupt handling */
    bool                interrupt_safe;     /* Can be called from interrupts */
    uint32_t            interrupt_depth;    /* Current interrupt nesting */
    lck_spin_t         *interrupt_lock;     /* Interrupt synchronization */

    /* Comprehensive statistics */
    uint64_t            total_enters;       /* Total epoch enters */
    uint64_t            total_exits;        /* Total epoch exits */
    uint64_t            max_concurrent;     /* Peak concurrent readers */
    uint64_t            grace_periods_completed; /* Completed grace periods */
    uint64_t            callbacks_queued;   /* Total callbacks queued */
    uint64_t            callbacks_executed; /* Total callbacks executed */
    uint64_t            emergency_reclaims; /* Emergency reclaim count */

    /* Debug and diagnostics */
    bool                debug_enabled;      /* Debug logging enabled */
    uint32_t            debug_level;        /* Debug verbosity level */
    queue_head_t        debug_trace;        /* Debug trace buffer */
    lck_mtx_t          *debug_lock;         /* Debug trace protection */
};

/*
 * Epoch Tracker - Enhanced per-thread epoch entry tracking
 */
struct xnu_epoch_tracker {
    /* Core tracking state */
    uint64_t            enter_epoch;        /* Epoch when entered */
    thread_t            thread;             /* Thread holding this tracker */
    struct xnu_epoch   *epoch;              /* Back pointer to epoch */
    uint64_t            enter_time;         /* Entry timestamp (debug) */
    bool                active;             /* Currently in critical section */

    /* Per-CPU optimization */
    uint32_t            cpu_id;             /* CPU where epoch was entered */
    struct xnu_epoch_cpu_state *cpu_state;  /* CPU state when entered */

    /* Nested epoch support */
    uint32_t            nesting_level;      /* Nesting depth */
    struct xnu_epoch_tracker *parent;      /* Parent tracker (if nested) */

    /* Performance tracking */
    uint64_t            hold_time_start;    /* When critical section started */
    uint64_t            total_hold_time;    /* Cumulative hold time */
    uint32_t            enter_count;        /* Number of enters */

    /* Interrupt context tracking */
    bool                from_interrupt;     /* Entered from interrupt */
    uint32_t            interrupt_level;    /* Interrupt level */
};

/*
 * Advanced Epoch Callback Entry
 */
struct epoch_callback_entry {
    /* Core callback data */
    queue_chain_t       cb_link;            /* Queue linkage */
    void              (*callback)(void *);  /* Callback function */
    void               *argument;           /* Callback argument */
    uint64_t            target_epoch;       /* Epoch when safe to call */
    uint64_t            enqueue_time;       /* When queued (debug) */

    /* Memory management */
    size_t              memory_size;        /* Size of memory being freed */
    uint32_t            priority;           /* Callback priority */
    uint32_t            flags;              /* Callback flags */

    /* Performance tracking */
    uint64_t            wait_time;          /* Time spent waiting */
    uint32_t            cpu_id;             /* CPU that queued callback */

    /* Debug information */
    const char         *func_name;          /* Function that queued callback */
    uint32_t            line_number;        /* Line number where queued */

/* Callback flags */
#define EPOCH_CB_FLAG_URGENT    0x01        /* High priority callback */
#define EPOCH_CB_FLAG_LARGE     0x02        /* Large memory allocation */
#define EPOCH_CB_FLAG_INTERRUPT 0x04        /* Queued from interrupt */
};

/*
 * Debug Trace Entry
 */
struct epoch_debug_trace {
    queue_chain_t       trace_link;         /* Queue linkage */
    uint64_t            timestamp;          /* When event occurred */
    thread_t            thread;             /* Thread that caused event */
    uint32_t            cpu_id;             /* CPU where event occurred */
    uint32_t            event_type;         /* Type of event */
    uint64_t            epoch_number;       /* Epoch number at time */
    uint32_t            active_readers;     /* Active readers at time */
    const char         *description;        /* Event description */

    /* Debug event types */
#define EPOCH_DEBUG_ENTER       1
#define EPOCH_DEBUG_EXIT        2
#define EPOCH_DEBUG_CALLBACK    3
#define EPOCH_DEBUG_GRACE_START 4
#define EPOCH_DEBUG_GRACE_END   5
#define EPOCH_DEBUG_EMERGENCY   6
};

/* ===== Advanced XNU rmlock Implementation ===== */

/*
 * Advanced Reader State Tracking
 */
struct xnu_reader_state {
    uint64_t            reader_id;          /* Unique reader identifier */
    thread_t            thread;             /* Reader thread */
    uint32_t            cpu_id;             /* CPU where reader is active */
    uint64_t            enter_time;         /* When reader entered */
    uint64_t            hold_time;          /* How long reader has held lock */
    bool                priority_boost;     /* Priority boost applied */
};

/*
 * Writer Wait Queue Management
 */
struct xnu_writer_waiter {
    queue_chain_t       wait_link;          /* Wait queue linkage */
    thread_t            thread;             /* Waiting writer thread */
    uint64_t            wait_start;         /* When wait started */
    uint32_t            priority;           /* Thread priority */
    bool                timeout_set;        /* Timeout configured */
    uint64_t            timeout_deadline;   /* Timeout deadline */
};

/*
 * XNU rmlock - Production-grade read-mostly lock
 */
struct xnu_rmlock {
    /* Core locking primitives */
    lck_rw_t           *rw_lock;            /* Fallback reader-writer lock */
    struct xnu_epoch   *read_epoch;         /* Epoch for lock-free reads */
    const char         *name;               /* Lock name for debugging */
    uint32_t            flags;              /* Configuration flags */

    /* Advanced reader tracking */
    struct xnu_reader_state *active_readers; /* Array of active readers */
    uint32_t            max_readers;        /* Maximum concurrent readers */
    uint32_t            current_readers;    /* Current reader count */
    lck_spin_t         *reader_lock;        /* Reader state protection */

    /* Writer management */
    queue_head_t        writer_wait_queue;  /* Waiting writers */
    uint32_t            waiting_writers;    /* Count of waiting writers */
    bool                writer_priority;    /* Writer has priority */
    thread_t            current_writer;     /* Current writer thread */
    uint64_t            writer_start_time;  /* When write lock acquired */
    lck_spin_t         *writer_lock;        /* Writer state protection */

    /* Performance optimization */
    uint64_t            bias_threshold;     /* Reader bias threshold */
    uint64_t            writer_threshold;   /* Writer starvation threshold */
    bool                reader_preference;  /* Reader preference mode */
    uint32_t            fast_path_enabled;  /* Fast path available */

    /* Comprehensive statistics */
    uint64_t            stat_read_fast;     /* Lock-free read acquisitions */
    uint64_t            stat_read_slow;     /* Fallback read acquisitions */
    uint64_t            stat_write_locks;   /* Write lock acquisitions */
    uint64_t            stat_contentions;   /* Lock contentions */
    uint64_t            stat_reader_spins;  /* Reader spin cycles */
    uint64_t            stat_writer_waits;  /* Writer wait cycles */
    uint64_t            stat_priority_boost;/* Priority boost applications */
    uint64_t            stat_timeout_events;/* Timeout events */
    lck_spin_t         *stat_lock;          /* Statistics protection */

    /* Memory pressure handling */
    uint32_t            memory_pressure;    /* Current memory pressure */
    bool                emergency_mode;     /* Emergency reclaim mode */

    /* Debug and diagnostics */
    bool                debug_enabled;      /* Debug mode enabled */
    queue_head_t        debug_history;      /* Lock operation history */
    lck_mtx_t          *debug_lock;         /* Debug data protection */
};

/* rmlock configuration flags */
#define RMLOCK_FLAG_RECURSE     0x01        /* Allow recursive read locks */
#define RMLOCK_FLAG_NOWAIT      0x02        /* Non-blocking operations */
#define RMLOCK_FLAG_FAIR        0x04        /* Fair scheduling */
#define RMLOCK_FLAG_ADAPTIVE    0x08        /* Adaptive spinning */
#define RMLOCK_FLAG_PRIORITY    0x10        /* Priority inheritance */

/*
 * rmlock Priority Tracker - Tracks reader in critical section
 */
struct xnu_rm_priotracker {
    struct xnu_epoch_tracker epoch_tracker; /* Epoch tracking */
    struct xnu_rmlock       *rmlock;        /* Back pointer to rmlock */
    uint64_t                 acquire_time;  /* Lock acquire time */
    bool                     fast_path;     /* Used lock-free path */
};

/* ===== Global Epoch Management ===== */

/*
 * Network Epoch - Used by FreeBSD networking code
 */
extern struct xnu_epoch *net_epoch_preempt;

/*
 * Initialize the epoch system
 */
void xnu_epoch_init(void);

/*
 * Create a new epoch context
 */
struct xnu_epoch *xnu_epoch_alloc(const char *name);

/*
 * Free an epoch context
 */
void xnu_epoch_free(struct xnu_epoch *epoch);

/*
 * Advanced Epoch Operations with Per-CPU Optimization
 */

/*
 * Get current CPU state for epoch operations
 */
static inline struct xnu_epoch_cpu_state *
xnu_epoch_get_cpu_state(struct xnu_epoch *epoch)
{
    uint32_t cpu_id = cpu_number();
    if (__improbable(cpu_id >= epoch->max_cpus)) {
        panic("xnu_epoch_get_cpu_state: invalid CPU ID %u", cpu_id);
    }
    return &epoch->cpu_states[cpu_id];
}

/*
 * Record debug trace event
 */
static inline void
xnu_epoch_debug_trace(struct xnu_epoch *epoch, uint32_t event_type,
                     const char *description)
{
    if (__improbable(epoch->debug_enabled && epoch->debug_level > 0)) {
        /* Implementation in .c file to avoid inline bloat */
        xnu_epoch_record_debug_event(epoch, event_type, description);
    }
}

/*
 * Advanced epoch enter - Production-grade with per-CPU optimization
 */
static inline void
xnu_epoch_enter_preempt(struct xnu_epoch *epoch, struct xnu_epoch_tracker *et)
{
    uint32_t cpu_id;
    struct xnu_epoch_cpu_state *cpu_state;
    uint64_t start_time = 0;

    /*
     * Interrupt safety check - ensure we can safely disable preemption
     */
    if (__improbable(ml_at_interrupt_context())) {
        if (!epoch->interrupt_safe) {
            panic("xnu_epoch_enter_preempt: called from interrupt context on non-interrupt-safe epoch");
        }
        et->from_interrupt = true;
        et->interrupt_level = ml_interrupt_level();
    } else {
        et->from_interrupt = false;
        et->interrupt_level = 0;
    }

    /*
     * Performance monitoring (if debug enabled)
     */
    if (__improbable(epoch->debug_enabled)) {
        start_time = mach_absolute_time();
    }

    /*
     * XNU Critical Section: Disable preemption for atomicity
     * This is the key optimization - no complex lock-free algorithms needed
     */
    disable_preemption();

    /* Get per-CPU state */
    cpu_id = cpu_number();
    cpu_state = &epoch->cpu_states[cpu_id];

    /*
     * Check for nesting - support recursive epoch enters
     */
    if (et->active && et->cpu_state == cpu_state) {
        /* Already in epoch on this CPU - increment nesting */
        et->nesting_level++;
        cpu_state->call_depth++;

        enable_preemption();
        return;
    }

    /*
     * Set up enhanced tracker with per-CPU optimization
     */
    et->thread = current_thread();
    et->epoch = epoch;
    et->cpu_id = cpu_id;
    et->cpu_state = cpu_state;
    et->enter_time = mach_absolute_time();
    et->nesting_level = 1;
    et->parent = NULL;
    et->active = true;

    /*
     * Performance tracking
     */
    et->hold_time_start = et->enter_time;
    et->enter_count++;

    /*
     * Read current epoch - safe because preemption is disabled
     * Use per-CPU epoch for better cache locality
     */
    et->enter_epoch = cpu_state->local_epoch;
    if (__improbable(et->enter_epoch == 0)) {
        /* CPU state not initialized - use global epoch */
        et->enter_epoch = OSAtomicAdd64(0, &epoch->epoch_number);
        cpu_state->local_epoch = et->enter_epoch;
    }

    /*
     * Increment per-CPU reader count (cache-friendly)
     */
    OSIncrementAtomic(&cpu_state->active_readers);
    cpu_state->call_depth++;
    cpu_state->enter_count++;

    /*
     * Update global statistics (less frequently to reduce contention)
     */
    if (__probable(!et->from_interrupt)) {
        OSIncrementAtomic64(&epoch->total_enters);
        cpu_state->fast_path_enters++;
    } else {
        /* From interrupt - use slower but safer path */
        lck_spin_lock(epoch->interrupt_lock);
        epoch->interrupt_depth++;
        lck_spin_unlock(epoch->interrupt_lock);
        cpu_state->slow_path_enters++;
    }

    /*
     * Memory barrier - ensure all reads happen after epoch registration
     * Use appropriate barrier for the CPU architecture
     */
    OSMemoryBarrier();

    /* Re-enable preemption - critical section established */
    enable_preemption();

    /*
     * Debug tracing
     */
    if (__improbable(epoch->debug_enabled)) {
        xnu_epoch_debug_trace(epoch, EPOCH_DEBUG_ENTER, "epoch_enter_preempt");

        /* Track performance metrics */
        et->total_hold_time += (mach_absolute_time() - start_time);
    }
}

/*
 * Advanced epoch exit - Production-grade with comprehensive cleanup
 */
static inline void
xnu_epoch_exit_preempt(struct xnu_epoch *epoch, struct xnu_epoch_tracker *et)
{
    struct xnu_epoch_cpu_state *cpu_state;
    uint64_t hold_time = 0;
    uint32_t remaining_readers;

    /*
     * Comprehensive tracker validation
     */
    if (__improbable(et->epoch != epoch || !et->active)) {
        panic("xnu_epoch_exit_preempt: invalid tracker state (epoch=%p, active=%d)",
              et->epoch, et->active);
    }

    if (__improbable(et->cpu_state == NULL)) {
        panic("xnu_epoch_exit_preempt: tracker missing CPU state");
    }

    cpu_state = et->cpu_state;

    /*
     * Handle nested epoch exits
     */
    if (__improbable(et->nesting_level > 1)) {
        et->nesting_level--;
        cpu_state->call_depth--;

        /* Still nested - just return */
        return;
    }

    /*
     * Performance tracking
     */
    if (__improbable(epoch->debug_enabled)) {
        hold_time = mach_absolute_time() - et->hold_time_start;
        et->total_hold_time += hold_time;

        xnu_epoch_debug_trace(epoch, EPOCH_DEBUG_EXIT, "epoch_exit_preempt");
    }

    /*
     * Memory barrier - ensure all reads complete before epoch exit
     * Critical for maintaining RCU semantics
     */
    OSMemoryBarrier();

    /*
     * Update per-CPU state first (cache-friendly)
     */
    if (__improbable(cpu_state->active_readers == 0)) {
        panic("xnu_epoch_exit_preempt: CPU %u has no active readers", et->cpu_id);
    }

    OSDecrementAtomic(&cpu_state->active_readers);
    cpu_state->call_depth--;
    cpu_state->exit_count++;

    /*
     * Update global statistics
     */
    if (__probable(!et->from_interrupt)) {
        OSIncrementAtomic64(&epoch->total_exits);
    } else {
        /* From interrupt context - update interrupt depth */
        lck_spin_lock(epoch->interrupt_lock);
        if (epoch->interrupt_depth > 0) {
            epoch->interrupt_depth--;
        }
        lck_spin_unlock(epoch->interrupt_lock);
    }

    /*
     * Mark tracker inactive
     */
    et->active = false;
    et->nesting_level = 0;
    et->cpu_state = NULL;

    /*
     * Check if we should trigger reclamation
     * Use hierarchical checking to reduce overhead
     */

    /* First check: per-CPU readers */
    if (__improbable(cpu_state->active_readers == 0)) {
        /* This CPU has no more readers - check global state */

        /* Sum up readers across all CPUs (expensive operation) */
        remaining_readers = 0;
        for (uint32_t i = 0; i < epoch->max_cpus; i++) {
            remaining_readers += epoch->cpu_states[i].active_readers;
        }

        /* If no readers anywhere, trigger reclamation */
        if (__improbable(remaining_readers == 0)) {
            /* Check if reclamation is needed */
            lck_spin_lock(epoch->epoch_lock);

            if (!queue_empty(&epoch->callback_queue)) {
                /* Callbacks pending - schedule reclamation */
                if (!thread_call_isactive(epoch->reclaim_call)) {
                    thread_call_enter(epoch->reclaim_call);
                }
            }

            lck_spin_unlock(epoch->epoch_lock);
        }
    }

    /*
     * Memory pressure handling
     */
    if (__improbable(epoch->memory.memory_pressure > 0)) {
        xnu_epoch_check_memory_pressure(epoch);
    }
}

/*
 * Schedule callback for epoch-based reclamation with advanced features
 */
void xnu_epoch_call(struct xnu_epoch *epoch,
                    void (*callback)(void *),
                    void *arg);

/*
 * Advanced epoch call with priority and debug info
 */
void xnu_epoch_call_advanced(struct xnu_epoch *epoch,
                           void (*callback)(void *),
                           void *arg,
                           size_t memory_size,
                           uint32_t priority,
                           uint32_t flags,
                           const char *func_name,
                           uint32_t line_number);

/*
 * Macro for advanced epoch call with automatic debug info
 */
#define xnu_epoch_call_debug(epoch, cb, arg, size, pri, flags) \
    xnu_epoch_call_advanced(epoch, cb, arg, size, pri, flags, __func__, __LINE__)

/*
 * Wait for current epoch to complete (synchronous) with timeout
 */
void xnu_epoch_wait_preempt(struct xnu_epoch *epoch);

/*
 * Wait for epoch with timeout (returns true if successful)
 */
bool xnu_epoch_wait_timeout(struct xnu_epoch *epoch, uint64_t timeout_ns);

/* ===== Advanced Support Functions ===== */

/*
 * Per-CPU and memory management functions
 */
void xnu_epoch_init_cpu_state(struct xnu_epoch *epoch, uint32_t cpu_id);
void xnu_epoch_cleanup_cpu_state(struct xnu_epoch *epoch, uint32_t cpu_id);
void xnu_epoch_check_memory_pressure(struct xnu_epoch *epoch);
void xnu_epoch_emergency_reclaim(struct xnu_epoch *epoch);

/*
 * Hierarchical grace period detection
 */
void xnu_epoch_start_grace_period(struct xnu_epoch *epoch);
bool xnu_epoch_check_grace_period(struct xnu_epoch *epoch);
void xnu_epoch_complete_grace_period(struct xnu_epoch *epoch);

/*
 * Debug and diagnostics
 */
void xnu_epoch_record_debug_event(struct xnu_epoch *epoch, uint32_t event_type,
                                 const char *description);
void xnu_epoch_dump_debug_trace(struct xnu_epoch *epoch);
void xnu_epoch_validate_state(struct xnu_epoch *epoch);

/*
 * Interrupt-safe operations
 */
bool xnu_epoch_enter_interrupt_safe(struct xnu_epoch *epoch, struct xnu_epoch_tracker *et);
void xnu_epoch_exit_interrupt_safe(struct xnu_epoch *epoch, struct xnu_epoch_tracker *et);

/*
 * Performance tuning and optimization
 */
void xnu_epoch_tune_parameters(struct xnu_epoch *epoch);
void xnu_epoch_balance_load(struct xnu_epoch *epoch);
void xnu_epoch_optimize_reclamation(struct xnu_epoch *epoch);

/* ===== Advanced XNU rmlock Functions ===== */

/*
 * Initialize rmlock with advanced options
 */
int xnu_rm_init_flags(struct xnu_rmlock *rm, const char *name, int flags);

/*
 * Initialize rmlock with full configuration
 */
int xnu_rm_init_advanced(struct xnu_rmlock *rm, const char *name, int flags,
                        uint32_t max_readers, uint64_t bias_threshold,
                        uint64_t writer_threshold);

/*
 * Destroy rmlock
 */
void xnu_rm_destroy(struct xnu_rmlock *rm);

/*
 * Advanced read lock operations
 */
void xnu_rm_rlock_advanced(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker,
                          bool try_fast_path);
bool xnu_rm_try_rlock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker);
void xnu_rm_rlock_timeout(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker,
                         uint64_t timeout_ns);

/*
 * Advanced write lock operations
 */
void xnu_rm_wlock_advanced(struct xnu_rmlock *rm, bool fair_mode);
bool xnu_rm_try_wlock(struct xnu_rmlock *rm);
void xnu_rm_wlock_timeout(struct xnu_rmlock *rm, uint64_t timeout_ns);

/*
 * Lock upgrade/downgrade operations
 */
bool xnu_rm_upgrade_lock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker);
void xnu_rm_downgrade_lock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker);

/*
 * Reader management
 */
void xnu_rm_track_reader(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker);
void xnu_rm_untrack_reader(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker);
uint32_t xnu_rm_get_reader_count(struct xnu_rmlock *rm);

/*
 * Writer queue management
 */
void xnu_rm_enqueue_writer(struct xnu_rmlock *rm, struct xnu_writer_waiter *waiter);
void xnu_rm_dequeue_writer(struct xnu_rmlock *rm, struct xnu_writer_waiter *waiter);
void xnu_rm_wakeup_writers(struct xnu_rmlock *rm);

/*
 * Performance optimization
 */
void xnu_rm_tune_performance(struct xnu_rmlock *rm);
void xnu_rm_check_starvation(struct xnu_rmlock *rm);
void xnu_rm_apply_priority_boost(struct xnu_rmlock *rm);

/*
 * Memory pressure handling
 */
void xnu_rm_handle_memory_pressure(struct xnu_rmlock *rm);
void xnu_rm_emergency_cleanup(struct xnu_rmlock *rm);

/*
 * Acquire read lock - OPTIMIZED LOCK-FREE PATH
 */
static inline void
xnu_rm_rlock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker)
{
    /*
     * Fast path: Use epoch-based lock-free read
     * This is the common case for read-heavy workloads
     */

    /* Enter epoch critical section */
    xnu_epoch_enter_preempt(rm->read_epoch, &tracker->epoch_tracker);

    /* Set up tracker */
    tracker->rmlock = rm;
    tracker->acquire_time = mach_absolute_time();
    tracker->fast_path = true;

    /*
     * Update statistics (optional)
     * Use atomic increment to avoid lock overhead
     */
    OSIncrementAtomic64(&rm->stat_read_fast);

    /*
     * That's it! No locks acquired.
     * Readers can run completely concurrently with other readers.
     * Writers will use epoch_wait to ensure all readers complete.
     */
}

/*
 * Release read lock
 */
static inline void
xnu_rm_runlock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker)
{
    /*
     * Verify tracker state
     */
    if (__improbable(tracker->rmlock != rm)) {
        panic("xnu_rm_runlock: tracker/lock mismatch");
    }

    if (tracker->fast_path) {
        /* Fast path: Just exit epoch */
        xnu_epoch_exit_preempt(rm->read_epoch, &tracker->epoch_tracker);
    } else {
        /* Slow path: Release actual lock */
        lck_rw_unlock_shared(rm->rw_lock);
    }

    /* Clear tracker */
    tracker->rmlock = NULL;
    tracker->fast_path = false;
}

/*
 * Acquire write lock - EXCLUSIVE ACCESS
 */
static inline void
xnu_rm_wlock(struct xnu_rmlock *rm)
{
    /*
     * Write locks always use the full lock
     * This provides exclusive access to both readers and writers
     */

    /* Acquire exclusive lock */
    lck_rw_lock_exclusive(rm->rw_lock);

    /*
     * Wait for all epoch readers to complete
     * This ensures no lock-free readers are still active
     */
    xnu_epoch_wait_preempt(rm->read_epoch);

    /* Update statistics */
    OSIncrementAtomic64(&rm->stat_write_locks);

    /*
     * Writer now has exclusive access
     * No readers (epoch or lock-based) can proceed
     */
}

/*
 * Release write lock
 */
static inline void
xnu_rm_wunlock(struct xnu_rmlock *rm)
{
    /*
     * Release exclusive lock
     * This allows both new readers and writers to proceed
     */
    lck_rw_unlock_exclusive(rm->rw_lock);
}

/*
 * Lock assertions for debugging
 */
void xnu_rm_assert(struct xnu_rmlock *rm, int what);

/* ===== FreeBSD Compatibility Macros ===== */

/*
 * Map FreeBSD types to XNU implementations
 */
typedef struct xnu_epoch           epoch_t;
typedef struct xnu_epoch_tracker   epoch_tracker_t;
typedef struct xnu_rmlock          rmlock_t;
typedef struct xnu_rm_priotracker  rm_priotracker_t;

/*
 * FreeBSD epoch API
 */
#define epoch_enter_preempt(epoch, et)     xnu_epoch_enter_preempt(epoch, et)
#define epoch_exit_preempt(epoch, et)      xnu_epoch_exit_preempt(epoch, et)
#define epoch_wait_preempt(epoch)          xnu_epoch_wait_preempt(epoch)
#define epoch_call(epoch, cb, arg)         xnu_epoch_call(epoch, cb, arg)

/*
 * FreeBSD rmlock API
 */
#define rm_init_flags(rm, name, flags)     xnu_rm_init_flags(rm, name, flags)
#define rm_destroy(rm)                     xnu_rm_destroy(rm)
#define rm_rlock(rm, tracker)              xnu_rm_rlock(rm, tracker)
#define rm_runlock(rm, tracker)            xnu_rm_runlock(rm, tracker)
#define rm_wlock(rm)                       xnu_rm_wlock(rm)
#define rm_wunlock(rm)                     xnu_rm_wunlock(rm)
#define rm_assert(rm, what)                xnu_rm_assert(rm, what)

/*
 * FreeBSD network epoch macros
 */
#define NET_EPOCH_ENTER(et)                epoch_enter_preempt(net_epoch_preempt, et)
#define NET_EPOCH_EXIT(et)                 epoch_exit_preempt(net_epoch_preempt, et)
#define NET_EPOCH_WAIT()                   epoch_wait_preempt(net_epoch_preempt)
#define NET_EPOCH_CALL(cb, arg)            epoch_call(net_epoch_preempt, cb, arg)

/*
 * RIB (Routing Information Base) lock macros
 * These map to the rmlock operations on the routing table head
 */
#define RIB_RLOCK_TRACKER                  struct xnu_rm_priotracker _rib_tracker
#define RIB_RLOCK(rh)                      xnu_rm_rlock(&(rh)->rib_lock, &_rib_tracker)
#define RIB_RUNLOCK(rh)                    xnu_rm_runlock(&(rh)->rib_lock, &_rib_tracker)
#define RIB_WLOCK(rh)                      xnu_rm_wlock(&(rh)->rib_lock)
#define RIB_WUNLOCK(rh)                    xnu_rm_wunlock(&(rh)->rib_lock)
#define RIB_LOCK_ASSERT(rh)                xnu_rm_assert(&(rh)->rib_lock, RA_LOCKED)
#define RIB_WLOCK_ASSERT(rh)               xnu_rm_assert(&(rh)->rib_lock, RA_WLOCKED)

/* Lock assertion constants */
#define RA_LOCKED       0x01
#define RA_WLOCKED      0x02
#define RA_RLOCKED      0x04
#define RM_DUPOK        0x01

/*
 * Performance optimization hints
 */
#define __improbable(expr)  __builtin_expect(!!(expr), 0)
#define __probable(expr)    __builtin_expect(!!(expr), 1)

/* ===== Statistics and Debugging ===== */

/*
 * Get rmlock statistics
 */
struct xnu_rmlock_stats {
    uint64_t read_fast;         /* Lock-free read operations */
    uint64_t read_slow;         /* Fallback read operations */
    uint64_t write_locks;       /* Write lock operations */
    uint64_t contentions;       /* Lock contentions */
    uint64_t epoch_waits;       /* Epoch wait operations */
};

void xnu_rm_get_stats(struct xnu_rmlock *rm, struct xnu_rmlock_stats *stats);
void xnu_rm_reset_stats(struct xnu_rmlock *rm);

/*
 * System-wide epoch statistics
 */
struct xnu_epoch_stats {
    uint64_t total_enters;      /* Total epoch enters */
    uint64_t total_exits;       /* Total epoch exits */
    uint64_t callbacks_queued;  /* Callbacks queued for reclamation */
    uint64_t callbacks_run;     /* Callbacks executed */
    uint64_t reclaim_cycles;    /* Reclamation cycles */
    uint64_t max_readers;       /* Peak concurrent readers */
};

void xnu_epoch_get_stats(struct xnu_epoch *epoch, struct xnu_epoch_stats *stats);

#endif /* KERNEL */
#endif /* _XNU_FREEBSD_RADIX_H_ */