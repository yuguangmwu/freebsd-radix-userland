/*
 * FreeBSD Radix Function Stubs
 *
 * These are minimal implementations to get our integration test compiling.
 * We'll replace these with actual FreeBSD radix.c code incrementally.
 */

#include "compat_shim.h"
#include "radix_adapter.h"

/* FreeBSD radix structures for stub compatibility */
struct radix_node {
    struct radix_node *rn_parent;
    struct radix_node *rn_left;
    struct radix_node *rn_right;
    struct radix_node *rn_dupedkey;
    caddr_t rn_key;
    caddr_t rn_mask;
    short rn_bit;
    char rn_bmask;
    u_char rn_flags;
    void *rn_mklist;
};

struct radix_head {
    struct radix_node *rnh_treetop;
    struct radix_node *(*rnh_addaddr)(void *v, const void *mask,
                                     struct radix_head *head, struct radix_node nodes[]);
    struct radix_node *(*rnh_deladdr)(const void *v, const void *mask,
                                     struct radix_head *head);
    struct radix_node *(*rnh_matchaddr)(const void *v, struct radix_head *head);
    struct radix_node *(*rnh_lookup)(const void *v, const void *mask,
                                    struct radix_head *head);
    int (*rnh_walktree)(struct radix_head *head, int (*f)(struct radix_node *, void *), void *w);
    void (*rnh_close)(struct radix_node *rn, struct radix_head *head);
    struct radix_node rnh_nodes[3];
    void *rnh_masks;
};

/* Simple stub implementations for FreeBSD radix functions */

/* Radix tree node structure (simplified for stub) */
struct radix_stub_entry {
    struct sockaddr_storage dst;
    struct sockaddr_storage mask;
    void *data;
    struct radix_stub_entry *next;
};

/* Radix tree head structure (compatible with adapter) */
struct radix_stub_head {
    struct radix_head rh;    /* Must be first for casting compatibility */
    struct radix_stub_entry *entries;
    int count;
};

/* Forward declarations for stub functions */
static struct radix_node *stub_addaddr(void *v, const void *mask, struct radix_head *head, struct radix_node nodes[]);
static struct radix_node *stub_deladdr(const void *v, const void *mask, struct radix_head *head);
static struct radix_node *stub_matchaddr(const void *v, struct radix_head *head);
static struct radix_node *stub_lookup(const void *v, const void *mask, struct radix_head *head);
static int stub_walktree(struct radix_head *head, int (*f)(struct radix_node *, void *), void *w);
static int addr_matches(struct sockaddr *addr, struct sockaddr *dst, struct sockaddr *mask);

/* Initialize a radix tree head (stub implementation) */
int rn_inithead(void **head, int off) {
    (void)off; /* Ignore offset for now */

    struct radix_stub_head *rh = bsd_malloc(sizeof(struct radix_stub_head), M_RTABLE, M_WAITOK | M_ZERO);
    if (!rh) {
        return 0;  /* FreeBSD returns 0 on failure */
    }

    rh->entries = NULL;
    rh->count = 0;

    /* Initialize function pointers for compatibility with adapter */
    rh->rh.rnh_addaddr = stub_addaddr;
    rh->rh.rnh_deladdr = stub_deladdr;
    rh->rh.rnh_matchaddr = stub_matchaddr;
    rh->rh.rnh_lookup = stub_lookup;
    rh->rh.rnh_walktree = stub_walktree;
    rh->rh.rnh_treetop = NULL;
    rh->rh.rnh_masks = NULL;

    *head = rh;

    return 1;  /* FreeBSD returns 1 on success */
}

/* Destroy a radix tree head (stub implementation) */
int rn_detachhead(void **head) {
    if (!head || !*head) {
        return 0;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)*head;

    /* Free all entries */
    struct radix_stub_entry *entry = rh->entries;
    while (entry) {
        struct radix_stub_entry *next = entry->next;
        bsd_free(entry, M_RTABLE);
        entry = next;
    }

    /* Free the head */
    bsd_free(rh, M_RTABLE);
    *head = NULL;

    return 1;
}

/* Add a route (stub function for adapter compatibility) */
static struct radix_node *stub_addaddr(void *v, const void *mask, struct radix_head *head, struct radix_node nodes[]) {
    (void)nodes; /* Ignore for stub */

    if (!v || !head) {
        return NULL;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)head;
    struct sockaddr *sa_dst = (struct sockaddr *)v;
    struct sockaddr *sa_mask = (struct sockaddr *)mask;

    /* Allocate new entry */
    struct radix_stub_entry *entry = bsd_malloc(sizeof(struct radix_stub_entry), M_RTABLE, M_WAITOK | M_ZERO);
    if (!entry) {
        return NULL;
    }

    /* Copy destination */
    memcpy(&entry->dst, sa_dst, sa_dst->sa_len);

    /* Copy mask if provided */
    if (sa_mask) {
        memcpy(&entry->mask, sa_mask, sa_mask->sa_len);
    }

    /* Add to linked list (simple implementation) */
    entry->next = rh->entries;
    rh->entries = entry;
    rh->count++;

    /* Return the entry as a radix_node (cast) */
    return (struct radix_node *)entry;
}

/* Delete a route (stub function for adapter compatibility) */
static struct radix_node *stub_deladdr(const void *v, const void *mask, struct radix_head *head) {
    if (!v || !head) {
        return NULL;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)head;
    struct sockaddr *lookup_dst = (struct sockaddr *)v;
    struct sockaddr *lookup_mask = (struct sockaddr *)mask;

    struct radix_stub_entry **prev = &rh->entries;
    struct radix_stub_entry *entry = rh->entries;

    while (entry) {
        struct sockaddr *dst = (struct sockaddr *)&entry->dst;
        struct sockaddr *entry_mask = (struct sockaddr *)&entry->mask;

        /* Check for exact match */
        int dst_match = (memcmp(dst, lookup_dst, lookup_dst->sa_len) == 0);
        int mask_match = 1;

        if (lookup_mask && entry_mask->sa_len > 0) {
            mask_match = (memcmp(entry_mask, lookup_mask, lookup_mask->sa_len) == 0);
        } else if (lookup_mask || entry_mask->sa_len > 0) {
            mask_match = 0;
        }

        if (dst_match && mask_match) {
            /* Remove from linked list */
            *prev = entry->next;
            rh->count--;
            return (struct radix_node *)entry;
        }

        prev = &entry->next;
        entry = entry->next;
    }

    return NULL;
}

/* Match a route (stub function for adapter compatibility) */
static struct radix_node *stub_matchaddr(const void *v, struct radix_head *head) {
    if (!v || !head) {
        return NULL;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)head;
    struct sockaddr *lookup = (struct sockaddr *)v;

    struct radix_stub_entry *best_match = NULL;
    uint32_t best_mask = 0;

    /* Linear search for best match (inefficient but correct for stub) */
    struct radix_stub_entry *entry = rh->entries;
    while (entry) {
        struct sockaddr *dst = (struct sockaddr *)&entry->dst;
        struct sockaddr *mask = (struct sockaddr *)&entry->mask;

        if (addr_matches(lookup, dst, mask)) {
            /* Check if this is a more specific match */
            if (mask->sa_family == AF_INET) {
                struct sockaddr_in *m = (struct sockaddr_in *)mask;
                uint32_t mask_ip = ntohl(m->sin_addr.s_addr);
                if (mask_ip >= best_mask) {
                    best_mask = mask_ip;
                    best_match = entry;
                }
            } else if (!best_match) {
                best_match = entry;
            }
        }
        entry = entry->next;
    }

    return (struct radix_node *)best_match;
}

/* Lookup with exact match (stub function for adapter compatibility) */
static struct radix_node *stub_lookup(const void *v, const void *mask, struct radix_head *head) {
    if (!v || !head) {
        return NULL;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)head;
    struct sockaddr *lookup_dst = (struct sockaddr *)v;
    struct sockaddr *lookup_mask = (struct sockaddr *)mask;

    struct radix_stub_entry *entry = rh->entries;

    while (entry) {
        struct sockaddr *dst = (struct sockaddr *)&entry->dst;
        struct sockaddr *entry_mask = (struct sockaddr *)&entry->mask;

        /* Check for exact match */
        int dst_match = (memcmp(dst, lookup_dst, lookup_dst->sa_len) == 0);
        int mask_match = 1;

        if (lookup_mask && entry_mask->sa_len > 0) {
            mask_match = (memcmp(entry_mask, lookup_mask, lookup_mask->sa_len) == 0);
        } else if (lookup_mask || entry_mask->sa_len > 0) {
            mask_match = 0;
        }

        if (dst_match && mask_match) {
            return (struct radix_node *)entry;
        }

        entry = entry->next;
    }

    return NULL;
}

/* Tree walk (stub function for adapter compatibility) */
static int stub_walktree(struct radix_head *head, int (*f)(struct radix_node *, void *), void *w) {
    if (!head || !f) {
        return -1;
    }

    struct radix_stub_head *rh = (struct radix_stub_head *)head;
    struct radix_stub_entry *entry = rh->entries;
    int result = 0;

    while (entry && result == 0) {
        result = f((struct radix_node *)entry, w);
        entry = entry->next;
    }

    return result;
}

/* Helper function to match IP addresses with mask */
static int addr_matches(struct sockaddr *addr, struct sockaddr *dst, struct sockaddr *mask) {
    if (!addr || !dst) {
        return 0;
    }

    if (addr->sa_family != dst->sa_family) {
        return 0;
    }

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *a = (struct sockaddr_in *)addr;
        struct sockaddr_in *d = (struct sockaddr_in *)dst;
        struct sockaddr_in *m = (struct sockaddr_in *)mask;

        uint32_t addr_ip = ntohl(a->sin_addr.s_addr);
        uint32_t dst_ip = ntohl(d->sin_addr.s_addr);
        uint32_t mask_ip = mask ? ntohl(m->sin_addr.s_addr) : 0xFFFFFFFF;

        return (addr_ip & mask_ip) == (dst_ip & mask_ip);
    }

    return 0;
}