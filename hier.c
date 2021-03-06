/*	$Id$ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#if HAVE_SYS_QUEUE
# include <sys/queue.h>
#endif 

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> /* memset */

#include <sqlite3.h>

#include "sqlbox.h"
#include "extern.h"

struct	sqlbox_role_node {
	size_t		 parent; /* parent role */
	size_t		*srcs; /* available sources */
	size_t		 srcsz; /* length of srcs */
	size_t		*stmts; /* available statements */
	size_t		 stmtsz; /* length of stmts */
};

struct	sqlbox_role_hier {
	struct sqlbox_role_node	*roles; /* per-role perms */
	size_t		  	 sz; /* number of roles */
	size_t			*sinks; /* "dead-end" nodes */
	size_t			 sinksz;
	size_t			*starts; /* "only-start" nodes */
	size_t			 startsz;
};

struct sqlbox_role_hier *
sqlbox_role_hier_alloc(size_t rolesz)
{
	struct sqlbox_role_hier	*p;
	size_t			 i;

	if ((p = calloc(1, sizeof(struct sqlbox_role_hier))) == NULL)
		return NULL;

	p->sz = rolesz;
	if (p->sz > 0 && (p->roles = calloc
	    (rolesz, sizeof(struct sqlbox_role_node))) == NULL) {
		free(p);
		return NULL;
	}
	for (i = 0; i < p->sz; i++)
		p->roles[i].parent = i;

	return p;
}

void
sqlbox_role_hier_free(struct sqlbox_role_hier *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	for (i = 0; i < p->sz; i++) {
		free(p->roles[i].srcs);
		free(p->roles[i].stmts);
	}

	free(p->roles);
	free(p->sinks);
	free(p->starts);
	free(p);
}

int
sqlbox_role_hier_stmt(struct sqlbox_role_hier *p, size_t role, size_t stmt)
{
	void	*pp;
	size_t	 i;

	/* Bounds check. */

	if (role >= p->sz)
		return 0;

	/* Ignore repeats. */

	for (i = 0; i < p->roles[role].stmtsz; i++)
		if (p->roles[role].stmts[i] == stmt)
			return 1;

	/* Reallocate and enqueue. */

	pp = reallocarray(p->roles[role].stmts, 
		p->roles[role].stmtsz + 1, sizeof(size_t));
	if (pp == NULL)
		return 0;
	p->roles[role].stmts = pp;
	p->roles[role].stmts[p->roles[role].stmtsz++] = stmt;
	return 1;
}


int
sqlbox_role_hier_src(struct sqlbox_role_hier *p, size_t role, size_t src)
{
	void	*pp;
	size_t	 i;

	/* Bounds check. */

	if (role >= p->sz)
		return 0;

	/* Ignore repeats. */

	for (i = 0; i < p->roles[role].srcsz; i++)
		if (p->roles[role].srcs[i] == src)
			return 1;

	/* Reallocate and enqueue. */

	pp = reallocarray(p->roles[role].srcs, 
		p->roles[role].srcsz + 1, sizeof(size_t));
	if (pp == NULL)
		return 0;
	p->roles[role].srcs = pp;
	p->roles[role].srcs[p->roles[role].srcsz++] = src;
	return 1;
}

int
sqlbox_role_hier_start(struct sqlbox_role_hier *p, size_t start)
{
	size_t	 i;
	void	*pp;

	/* Valid index. */

	if (start >= p->sz)
		return 0;

	/* Not already a child. */

	if (p->roles[start].parent != start)
		return 0;

	/* Not already a parent. */

	for (i = 0; i < p->sz; i++)
		if (i != start && p->roles[i].parent == start)
			return 0;

	/* Not a sink. */

	for (i = 0; i < p->sinksz; i++)
		if (p->sinks[i] == start)
			return 0;

	/* If not already set, set as a start. */

	for (i = 0; i < p->startsz; i++)
		if (p->starts[i] == start)
			return 1;
	pp = reallocarray(p->starts, p->startsz + 1, sizeof(size_t));
	if (pp == NULL)
		return 0;
	p->starts = pp;
	p->starts[p->startsz++] = start;
	return 1;
}

int
sqlbox_role_hier_sink(struct sqlbox_role_hier *p, size_t sink)
{
	size_t	 i;
	void	*pp;

	/* Valid index. */

	if (sink >= p->sz)
		return 0;

	/* Not already a child. */

	if (p->roles[sink].parent != sink)
		return 0;

	/* Not already a parent. */

	for (i = 0; i < p->sz; i++)
		if (i != sink && p->roles[i].parent == sink)
			return 0;

	/* Not a start. */

	for (i = 0; i < p->startsz; i++)
		if (p->starts[i] == sink)
			return 0;

	/* If not already set, set as a sink. */

	for (i = 0; i < p->sinksz; i++)
		if (p->sinks[i] == sink)
			return 1;
	pp = reallocarray(p->sinks, p->sinksz + 1, sizeof(size_t));
	if (pp == NULL)
		return 0;
	p->sinks = pp;
	p->sinks[p->sinksz++] = sink;
	return 1;
}

int
sqlbox_role_hier_child(struct sqlbox_role_hier *p, size_t parent, size_t child)
{
	size_t	 idx, i;

	/* Make sure not out of bounds or already set or a sink/start. */

	if (parent >= p->sz || child >= p->sz)
		return 0;
	if (p->roles[child].parent != child)
		return 0;

	for (i = 0; i < p->sinksz; i++)
		if (p->sinks[i] == parent || p->sinks[i] == child)
			return 0;
	for (i = 0; i < p->startsz; i++)
		if (p->starts[i] == parent || p->starts[i] == child)
			return 0;

	/* Ignore self-reference. */

	if (child == parent)
		return 1;

	/* Make sure no ancestors are the self. */

	for (idx = parent; ; idx = p->roles[idx].parent)
		if (p->roles[idx].parent == child)
			return 0;
		else if (p->roles[idx].parent == idx)
			break;

	p->roles[child].parent = parent;
	return 1;
}

void
sqlbox_role_hier_gen_free(struct sqlbox_roles *r)
{
	size_t	 i;

	if (r == NULL)
		return;

	for (i = 0; i < r->rolesz; i++) {
		free(r->roles[i].roles);
		free(r->roles[i].stmts);
		free(r->roles[i].srcs);
	}

	free(r->roles);
	memset(r, 0, sizeof(struct sqlbox_roles));
}

/*
 * A simple algorithm.
 * First, we're guaranteed that we have a set of DAGs.
 * For each node that has a parent, traverse upward, allowing all
 * ancestors to transition into the common descendent.
 * Then for each node, uniquely add all permissions of the ancestors to
 * the given node.
 */
int
sqlbox_role_hier_gen(const struct sqlbox_role_hier *p, 
	struct sqlbox_roles *r, size_t defrole)
{
	size_t			  i, j, k, idx, pidx;
	void			*pp;
	struct sqlbox_role	*rr;

	memset(r, 0, sizeof(struct sqlbox_roles));

	if (p->sz && defrole >= p->sz)
		return 0;
	if (p->sz == 0 && defrole != 0)
		return 0;

	r->roles = calloc(p->sz, sizeof(struct sqlbox_role));
	if (r->roles == NULL)
		goto err;
	r->rolesz = p->sz;
	r->defrole = defrole;

	/* 
	 * Start by collecting the number of outbound roles we're going
	 * to have per parent.
	 * Each parent needs to have space for each of its descendents.
	 */

	for (i = 0; i < p->sz; i++) {
		idx = i;
		while (p->roles[idx].parent != idx) {
			r->roles[p->roles[idx].parent].rolesz++;
			idx = p->roles[idx].parent;
		}
	}

	/* 
	 * Now allocate space.
	 * Reset the number of roles because we're going to use it as
	 * where to index into the role array in the next step.
	 * Remember for each non-sink to have transitions to all
	 * available sinks.
	 * Sinks never have outbound transitions.
	 * Each starter has links to all other nodes except itself (of
	 * course this is implied) and sinks.
	 */

	for (i = 0; i < p->sz; i++) {
		/* Sinks have nothing. */

		for (j = 0; j < p->sinksz; j++) 
			if (p->sinks[j] == i)
				break;
		if (j < p->sinksz) {
			assert(p->roles[i].parent == i);
			assert(r->roles[i].rolesz == 0);
			continue;
		}

		/* Starts have all nodes except other starts. */

		for (j = 0; j < p->startsz; j++) 
			if (p->starts[j] == i)
				break;
		if (j < p->startsz) {
			assert(p->roles[i].parent == i);
			assert(r->roles[i].rolesz == 0);
			r->roles[i].rolesz = 
				p->sz - p->startsz - p->sinksz;
		} 

		/* All nodes also have sinks. */

		r->roles[i].rolesz += p->sinksz;
		if (r->roles[i].rolesz == 0)
			continue;
		r->roles[i].roles = calloc
			(r->roles[i].rolesz, sizeof(size_t));
		if (r->roles[i].roles == NULL)
			goto err;
		r->roles[i].rolesz = 0;
	}

	/* Assign children. */

	for (i = 0; i < p->sz; i++) {
		idx = i;
		while (p->roles[idx].parent != idx) {
			pidx = p->roles[idx].parent;
			rr = &r->roles[pidx];
			rr->roles[rr->rolesz++] = i;
			idx = pidx;
		}
	}

	/* Assign sinks except to sinks themselves. */

	for (i = 0; i < p->sz; i++) {
		rr = &r->roles[i];
		for (j = 0; j < p->sinksz; j++) 
			if (p->sinks[j] == i)
				break;
		if (j < p->sinksz)
			continue;
		for (j = 0; j < p->sinksz; j++)
			rr->roles[rr->rolesz++] = p->sinks[j];
	}

	/* 
	 * Assign all nodes to starts except other starts (and sinks, as
	 * we've already done that).
	 */

	for (i = 0; i < p->startsz; i++) {
		rr = &r->roles[p->starts[i]];
		for (j = 0; j < p->sz; j++) {
			for (k = 0; k < p->startsz; k++)
				if (p->starts[k] == j)
					break;
			if (k < p->startsz)
				continue;
			for (k = 0; k < p->sinksz; k++)
				if (p->sinks[k] == j)
					break;
			if (k < p->sinksz)
				continue;
			rr->roles[rr->rolesz++] = j;
		}
	}

	/* Now we accumulate statements. */

	for (i = 0; i < p->sz; i++) {
		for (idx = i; ; idx = p->roles[idx].parent) {

			/* Look for all parent statements in the child. */

			for (j = 0; j < p->roles[idx].stmtsz; j++) {
				for (k = 0; k < r->roles[i].stmtsz; k++)
					if (p->roles[idx].stmts[j] ==
					    r->roles[i].stmts[k])
						break;
				if (k < r->roles[i].stmtsz)
					continue;

				/* Not found: append. */

				pp = reallocarray(r->roles[i].stmts,
					r->roles[i].stmtsz + 1,
					sizeof(size_t));
				if (pp == NULL)
					goto err;
				r->roles[i].stmts = pp;
				r->roles[i].stmts[r->roles[i].stmtsz] =
					p->roles[idx].stmts[j];
				r->roles[i].stmtsz++;
			}

			/* Now sources. */

			for (j = 0; j < p->roles[idx].srcsz; j++) {
				for (k = 0; k < r->roles[i].srcsz; k++)
					if (p->roles[idx].srcs[j] ==
					    r->roles[i].srcs[k])
						break;
				if (k < r->roles[i].srcsz)
					continue;

				/* Not found: append. */

				pp = reallocarray(r->roles[i].srcs,
					r->roles[i].srcsz + 1,
					sizeof(size_t));
				if (pp == NULL)
					goto err;
				r->roles[i].srcs = pp;
				r->roles[i].srcs[r->roles[i].srcsz] =
					p->roles[idx].srcs[j];
				r->roles[i].srcsz++;
			}

			/* Stop when we're at a root. */

			if (p->roles[idx].parent == idx)
				break;
		}
	}

	return 1;

err:
	sqlbox_role_hier_gen_free(r);
	return 0;
}
