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
#include <string.h>
#include <unistd.h>

#include <sqlite3.h>

#include "sqlbox.h"
#include "extern.h"

int
sqlbox_step(struct sqlbox *box, size_t stmtid)
{
#if 0
	uint32_t	 v = htonl(stmtid);
	const char	*frame;
	size_t		 framesz, bufsz = 0;
	char		*buf = NULL;

	if (!sqlbox_write_frame
	    (box, SQLBOX_OP_STEP, (char *)&v, sizeof(uint32_t))) {
		sqlbox_warnx(&box->cfg, "step: sqlbox_write_frame");
		return 0;
	}

	/* This will read the entire result set. */

	if (sqlbox_read_frame(box, &buf, &bufsz, &frame, &framesz) <= 0) {
		sqlbox_warnx(&box->cfg, "step: sqlbox_read_frame");
		free(buf);
		return 0;
	}

	if ((p = calloc(1, sizeof(struct sqlbox_res))) == NULL) {
		sqlbox_warn(&box->cfg, "step: calloc");
		free(buf);
		return 0;
	}
	p->buf = buf;
	p->id = stmtid;

	if (!sqlbox_bound_unpack
	    (box, &p->boundsz, &p->bound, p->buf, p->bufsz)) {
		sqlbox_warnx(&box->cfg, "step: sqlbox_bound_unpack");
		free(p);
		free(buf);
		return 0;
	}

#endif
	return 1;
}

/*
 * Prepare and bind parameters to a statement in one step.
 * Return TRUE on success, FALSE on failure (nothing is allocated).
 */
int
sqlbox_op_step(struct sqlbox *box, const char *buf, size_t sz)
{
	return 0;
}
