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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "perf.h"
#include <sqlite3.h>

int
main(int argc, char *argv[])
{
	size_t		 	 i, rows = 10000;
	sqlite3			*db;
	sqlite3_stmt		*stmt = NULL;
	int			 c;
	int64_t			 v1, v2, v3, v4;
	const char		*stmts[] = {
		"CREATE TABLE foo (col1 INT, col2 INT, col3 INT, col4 INT)",
		"INSERT INTO foo (col1, col2, col3, col4) VALUES (?,?,?,?)",
	};

	if (pledge("stdio rpath cpath wpath flock fattr proc", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	while ((c = getopt(argc, argv, "n:")) != -1)
		switch (c) {
		case 'n':
			rows = atoi(optarg);
			break;
		default:
			return EXIT_FAILURE;
		}

	printf(">>> %zu insertions\n", rows);

	if (sqlite3_open_v2(":memory:", &db, 
	    SQLITE_OPEN_READWRITE, NULL) != SQLITE_OK)
		errx(EXIT_FAILURE, "sqlite3_open_v2");
	if (sqlite3_exec(db, stmts[0], NULL, NULL, NULL) != SQLITE_OK)
		errx(EXIT_FAILURE, "sqlite3_exec");
	if (sqlite3_prepare_v2(db, stmts[1], -1, &stmt, NULL) != SQLITE_OK)
		errx(EXIT_FAILURE, "sqlite3_prepare_v2");

	for (i = 0; i < rows; i++) {
#ifdef __OpenBSD__
		v1 = arc4random();
		v2 = arc4random();
		v3 = arc4random();
		v4 = arc4random();
#else
		v1 = random();
		v2 = random();
		v3 = random();
		v4 = random();
#endif
		if (sqlite3_reset(stmt))
			errx(EXIT_FAILURE, "sqlite3_reset");
		if (sqlite3_clear_bindings(stmt))
			errx(EXIT_FAILURE, "sqlite3_clear_bindings");
		if (sqlite3_bind_int64(stmt, 1, v1) != SQLITE_OK)
			errx(EXIT_FAILURE, "sqlite3_bind_int64");
		if (sqlite3_bind_int64(stmt, 2, v2) != SQLITE_OK)
			errx(EXIT_FAILURE, "sqlite3_bind_int64");
		if (sqlite3_bind_int64(stmt, 3, v3) != SQLITE_OK)
			errx(EXIT_FAILURE, "sqlite3_bind_int64");
		if (sqlite3_bind_int64(stmt, 4, v4) != SQLITE_OK)
			errx(EXIT_FAILURE, "sqlite3_bind_int64");
		if (sqlite3_step(stmt) != SQLITE_DONE)
			errx(EXIT_FAILURE, "sqlite3_step");
	}

	if (sqlite3_finalize(stmt))
		errx(EXIT_FAILURE, "sqlite3_finalize");
	if (sqlite3_close(db) != SQLITE_OK)
		errx(EXIT_FAILURE, "sqlite3_close");

	puts("<<< done");
	return EXIT_SUCCESS;
}
