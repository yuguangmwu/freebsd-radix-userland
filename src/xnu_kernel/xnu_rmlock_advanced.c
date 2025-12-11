/*
 * Advanced rmlock Functions - Additional Implementation
 */

/*
 * Standard write lock (wrapper for advanced)
 */
static inline void
xnu_rm_wlock(struct xnu_rmlock *rm)
{
    xnu_rm_wlock_advanced(rm, false);  /* Non-fair by default for performance */
}

/*
 * Non-blocking write lock attempt
 */
bool
xnu_rm_try_wlock(struct xnu_rmlock *rm)
{
    if (!rm) {
        return false;
    }

    /* Check if any readers are active */
    if (rm->current_readers > 0) {
        return false;
    }

    /* Try to acquire exclusive lock */
    if (!lck_rw_try_lock_exclusive(rm->rw_lock)) {
        return false;
    }

    /* Check epoch readers (might still be some) */
    lck_spin_lock(rm->read_epoch->epoch_lock);
    uint32_t total_epoch_readers = 0;
    for (uint32_t i = 0; i < rm->read_epoch->max_cpus; i++) {
        total_epoch_readers += rm->read_epoch->cpu_states[i].active_readers;
    }
    lck_spin_unlock(rm->read_epoch->epoch_lock);

    if (total_epoch_readers > 0) {
        /* Epoch readers still active - can't get exclusive access */
        lck_rw_unlock_exclusive(rm->rw_lock);
        return false;
    }

    /* Success - set current writer */
    lck_spin_lock(rm->writer_lock);
    rm->current_writer = current_thread();
    rm->writer_start_time = mach_absolute_time();
    lck_spin_unlock(rm->writer_lock);

    OSIncrementAtomic64(&rm->stat_write_locks);
    return true;
}

/*
 * Release write lock
 */
static inline void
xnu_rm_wunlock(struct xnu_rmlock *rm)
{
    if (!rm) {
        panic("xnu_rm_wunlock: invalid rmlock");
    }

    /* Clear current writer */
    lck_spin_lock(rm->writer_lock);
    rm->current_writer = THREAD_NULL;
    lck_spin_unlock(rm->writer_lock);

    /* Release exclusive lock */
    lck_rw_unlock_exclusive(rm->rw_lock);

    /* Wake up any waiting writers */
    lck_spin_lock(rm->writer_lock);
    xnu_rm_wakeup_writers(rm);
    lck_spin_unlock(rm->writer_lock);

    printf("XNU rmlock: released write lock '%s'\n", rm->name);
}

/*
 * Advanced read lock release
 */
static inline void
xnu_rm_runlock(struct xnu_rmlock *rm, struct xnu_rm_priotracker *tracker)
{
    if (!rm || !tracker) {
        panic("xnu_rm_runlock: invalid parameters");
    }

    if (tracker->fast_path) {
        /* Fast path: exit epoch and untrack reader */
        xnu_rm_untrack_reader(rm, tracker);
        xnu_epoch_exit_preempt(rm->read_epoch, &tracker->epoch_tracker);
    } else {
        /* Slow path: release actual lock */
        lck_rw_unlock_shared(rm->rw_lock);
    }

    /* Clear tracker */
    tracker->rmlock = NULL;
    tracker->fast_path = false;
}

/*
 * Apply priority boost to long-running readers
 */
void
xnu_rm_apply_priority_boost(struct xnu_rmlock *rm)
{
    uint64_t current_time = mach_absolute_time();
    uint32_t boosted_readers = 0;

    lck_spin_lock(rm->reader_lock);

    for (uint32_t i = 0; i < rm->max_readers; i++) {
        struct xnu_reader_state *reader = &rm->active_readers[i];

        if (reader->thread != THREAD_NULL && !reader->priority_boost) {
            uint64_t hold_time = current_time - reader->enter_time;

            if (hold_time > rm->bias_threshold) {
                /* Apply priority boost to encourage quick completion */
                thread_precedence_policy_data_t policy;
                policy.importance = thread_get_importance(reader->thread) + 1;

                thread_policy_set(reader->thread, THREAD_PRECEDENCE_POLICY,
                                 (thread_policy_t)&policy,
                                 THREAD_PRECEDENCE_POLICY_COUNT);

                reader->priority_boost = true;
                boosted_readers++;
            }
        }
    }

    lck_spin_unlock(rm->reader_lock);

    if (boosted_readers > 0) {
        OSIncrementAtomic64(&rm->stat_priority_boost);
        printf("XNU rmlock: applied priority boost to %u readers in '%s'\n",
               boosted_readers, rm->name);
    }
}

/*
 * Performance tuning based on runtime characteristics
 */
void
xnu_rm_tune_performance(struct xnu_rmlock *rm)
{
    uint64_t read_fast, read_slow, contentions;

    lck_spin_lock(rm->stat_lock);
    read_fast = rm->stat_read_fast;
    read_slow = rm->stat_read_slow;
    contentions = rm->stat_contentions;
    lck_spin_unlock(rm->stat_lock);

    /* Adaptive bias adjustment */
    if (read_fast > 0 && read_slow > 0) {
        double fast_ratio = (double)read_fast / (read_fast + read_slow);

        if (fast_ratio < 0.8 && contentions > 100) {
            /* Fast path not effective - increase bias threshold */
            rm->bias_threshold *= 2;

            if (rm->bias_threshold > 10000000) {  /* 10ms max */
                rm->bias_threshold = 10000000;
            }

            printf("XNU rmlock: increased bias threshold for '%s' to %llu ns\n",
                   rm->name, rm->bias_threshold);
        } else if (fast_ratio > 0.95 && contentions < 10) {
            /* Fast path very effective - decrease bias threshold */
            rm->bias_threshold /= 2;

            if (rm->bias_threshold < 100000) {  /* 100μs min */
                rm->bias_threshold = 100000;
            }
        }
    }
}

/*
 * Get current reader count
 */
uint32_t
xnu_rm_get_reader_count(struct xnu_rmlock *rm)
{
    uint32_t total_readers = 0;

    if (!rm) {
        return 0;
    }

    /* Count tracked readers */
    lck_spin_lock(rm->reader_lock);
    total_readers += rm->current_readers;
    lck_spin_unlock(rm->reader_lock);

    /* Count epoch readers */
    for (uint32_t i = 0; i < rm->read_epoch->max_cpus; i++) {
        total_readers += rm->read_epoch->cpu_states[i].active_readers;
    }

    return total_readers;
}

#endif /* KERNEL */