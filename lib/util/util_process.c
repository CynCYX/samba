/*
 *  Unix SMB/CIFS implementation.
 *
 *  Process utils.
 *
 *  Copyright (c) 2013      Andreas Schneider <asn@samba.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "util_process.h"
#include "replace.h"

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

void process_set_title(const char *short_format, const char *long_format, ...)
{
#if defined(HAVE_PRCTL) && defined(PR_SET_NAME)
	if (short_format != NULL) {
		char short_comment[16] = {0,};
		va_list ap;

		va_start(ap, long_format);
		vsnprintf(short_comment, sizeof(short_comment), short_format, ap);
		va_end(ap);

		prctl(PR_SET_NAME, (unsigned long) short_comment, 0, 0, 0);
	}
#endif

	if (long_format != NULL) {
		char long_comment[256] = {0,};
		va_list ap;

		va_start(ap, long_format);
		vsnprintf(long_comment, sizeof(long_comment), long_format, ap);
		va_end(ap);

		setproctitle("%s", long_comment);
	}
}

int prctl_set_comment(const char *comment_format, ...)
{
	char comment[16];
	va_list ap;

	va_start(ap, comment_format);
	vsnprintf(comment, sizeof(comment), comment_format, ap);
	va_end(ap);

	process_set_title("%s", "%s", comment);
	return 0;
}
