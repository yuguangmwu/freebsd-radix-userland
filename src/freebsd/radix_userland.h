/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Radix Tree Header - Userland Version
 */

#ifndef _RADIX_USERLAND_H_
#define	_RADIX_USERLAND_H_

/* We assume our compatibility shim has been included already */

/*
 * Radix search tree node layout.
 */
struct radix_node {
	struct	radix_mask *rn_mklist;	/* list of masks contained in subtree */
	struct	radix_node *rn_parent;	/* parent */
	short	rn_bit;			/* bit offset; -1-index(netmask) */
	char	rn_bmask;		/* node: mask for bit test*/
	u_char	rn_flags;		/* enumerated next */
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {			/* leaf only data: */
			caddr_t	rn_Key;	/* object of search */
			caddr_t	rn_Mask;	/* netmask, if present */
			struct	radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			/* node only data: */
			int	rn_Off;		/* where to start compare */
			struct	radix_node *rn_L;/* progeny */
			struct	radix_node *rn_R;/* progeny */
		} rn_node;
	}		rn_u;
#ifdef RN_DEBUG
	int rn_info;
	struct radix_node *rn_twin;
	struct radix_node *rn_ybro;
#endif
};

#define	rn_dupedkey	rn_u.rn_leaf.rn_Dupedkey
#define	rn_key		rn_u.rn_leaf.rn_Key
#define	rn_mask		rn_u.rn_leaf.rn_Mask
#define	rn_offset	rn_u.rn_node.rn_Off
#define	rn_left		rn_u.rn_node.rn_L
#define	rn_right	rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

struct radix_mask {
	short	rm_bit;			/* bit offset; -1-index(netmask) */
	char	rm_unused;		/* cf. rn_bmask */
	u_char	rm_flags;		/* cf. rn_flags */
	struct	radix_mask *rm_mklist;	/* more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}	rm_rmu;
	int	rm_refs;		/* # of references to this struct */
};

#define	rm_mask rm_rmu.rmu_mask
#define	rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */

typedef int walktree_f_t(struct radix_node *, void *);

struct radix_mask_head {
	struct	radix_head mask_nodes[1];
	struct	radix_node *mask_nodes[3];
};

struct radix_head {
	struct	radix_node rh_treetop;
	void	*rh_arg1;
	void	*rh_arg2;
	struct	radix_node *(*rnh_addaddr)	/* add node based on sockaddr */
		(void *v, const void *mask,
		     struct radix_head *head, struct radix_node nodes[]);
	struct	radix_node *(*rnh_addpkt)	/* add node based on packet hdr */
		(void *v, const void *mask,
		     struct radix_head *head, struct radix_node nodes[]);
	struct	radix_node *(*rnh_deladdr)	/* remove node based on sockaddr */
		(const void *v, const void *mask, struct radix_head *head);
	struct	radix_node *(*rnh_delpkt)	/* remove node based on packet hdr */
		(const void *v, const void *mask, struct radix_head *head);
	struct	radix_node *(*rnh_matchaddr)	/* locate based on sockaddr */
		(const void *v, struct radix_head *head);
	struct	radix_node *(*rnh_lookup)	/* locate based on sockaddr */
		(const void *v, const void *mask, struct radix_head *head);
	struct	radix_node *(*rnh_matchpkt)	/* locate based on packet hdr */
		(const void *v, struct radix_head *head);
	int	(*rnh_walktree)			/* traverse tree */
		(struct radix_head *head, walktree_f_t *f, void *w);
	int	(*rnh_walktree_from)		/* traverse tree below a */
		(struct radix_head *head, void *a, void *m,
		     walktree_f_t *f, void *w);
	void	(*rnh_close)	/* do something when the last ref drops */
		(struct radix_node *rn, struct radix_head *head);
	struct	radix_node rnh_nodes[3];	/* empty tree for common case */
	struct	radix_mask_head *rnh_masks;	/* Storage for our masks */
};

#ifndef _RADIX_KERNEL_DEFINED
#define	R_Malloc(p, t, n) (p = (t) malloc((unsigned int)(n)))
#define	R_Zalloc(p, t, n) (p = (t) calloc(1,(unsigned int)(n)))
#define	R_Free(p) free((char *)p);
#else
#define R_Malloc(p, t, n) (p = (t) malloc((n), M_RTABLE, M_NOWAIT))
#define R_Zalloc(p, t, n) (p = (t) malloc((n), M_RTABLE, M_NOWAIT | M_ZERO))
#define R_Free(p) free((caddr_t)p, M_RTABLE)
#endif

struct radix_node_head {
	struct	radix_head rh;
	struct	radix_node rnh_nodes[3];	/* empty tree for common case */
};

/* Function prototypes */
void	 rn_inithead_internal(struct radix_head *rh, struct radix_node *base_nodes, int off);
int	 rn_inithead(void **head, int off);
int	 rn_detachhead(void **head);
int	 rn_refines(const void *m_arg, const void *n_arg);
struct radix_node *rn_addroute(void *dst, const void *netmask,
			       struct radix_head *head, struct radix_node treenodes[2]);
struct radix_node *rn_delete(const void *dst, const void *netmask, struct radix_head *head);
struct radix_node *rn_lookup(const void *v_arg, const void *m_arg, struct radix_head *head);
struct radix_node *rn_match(const void *v_arg, struct radix_head *head);
struct radix_node *rn_nextprefix(struct radix_node *rn);
int	rn_walktree(struct radix_head *head, walktree_f_t *f, void *w);
int	rn_walktree_from(struct radix_head *h, void *a, void *m, walktree_f_t *f, void *w);

#endif	/* _RADIX_USERLAND_H_ */