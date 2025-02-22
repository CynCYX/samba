/*
   Unix SMB/CIFS implementation.

   Stress test for parallel NSS & libwbclient calls.

   Copyright (C) Ralph Wuerthner 2018

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <wbclient.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>

#define RUNTIME 10

struct thread_state {
	const char *username;
	time_t timeout;
	pthread_mutex_t lock;
	bool fail;
	int nss_loop_count;
	int wbc_loop_count;
};

static void *query_nss_thread(void *ptr)
{
	struct thread_state *state = ptr;
	char buf[1024];
	ssize_t nread, nwritten;
	int p[2];
	int rc;
	struct passwd pwd, *result;
	pid_t pid;

	while (time(NULL) < state->timeout) {
		rc = getpwnam_r(state->username,
				&pwd,
				buf,
				sizeof(buf),
				&result);
		if (rc != 0 || result == NULL) {
			pthread_mutex_lock(&state->lock);
			state->fail = true;
			pthread_mutex_unlock(&state->lock);
			fprintf(stderr,
				"getpwnam_r failed with rc='%s' result=%p\n",
				strerror(rc),
				result);
			break;
		}
		state->nss_loop_count++;
		pthread_mutex_lock(&state->lock);
		if (state->fail) {
			pthread_mutex_unlock(&state->lock);
			break;
		}
		pthread_mutex_unlock(&state->lock);
	}

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, p);
	if (rc != 0) {
		state->fail = true;
		return NULL;
	}

	/*
	 * Check getpwnam_r() still works after a fork,
	 * both in parent and child.
	 */

	pid = fork();
	if (pid == -1) {
		return NULL;
	}
	if (pid == 0) {
		/* Child */
		rc = getpwnam_r(state->username,
				&pwd,
				buf,
				sizeof(buf),
				&result);
		if (rc != 0 || result == NULL) {
			fprintf(stderr,
				"getpwnam_r failed with rc='%s' result=%p\n",
				strerror(rc),
				result);
			rc = 1;
			nwritten = write(p[0], &rc, sizeof(int));
			assert(nwritten == sizeof(int));
			exit(1);
		}
		printf("child: getpwnam_r in child succeeded\n");
		rc = 0;
		nwritten = write(p[0], &rc, sizeof(int));
		assert(nwritten == sizeof(int));
		exit(1);
	}

	/* Parent */

	/* Check result from child */
	nread = read(p[1], &rc, sizeof(int));
	if (nread != sizeof(int)) {
		fprintf(stderr,
			"read from child failed with errno='%s' nread=%zd\n",
			strerror(errno),
			nread);
		state->fail = true;
		return NULL;
	}

	if (rc != 0) {
		fprintf(stderr,
			"getpwnam_r failed in the child\n");
		state->fail = true;
		return NULL;
	}
	printf("parent: getpwnam_r in child succeeded\n");

	/* Verify getpwnam_r() in parent after fork */
	rc = getpwnam_r(state->username,
			&pwd,
			buf,
			sizeof(buf),
			&result);
	if (rc != 0 || result == NULL) {
		fprintf(stderr,
			"getpwnam_r failed with rc='%s' result=%p\n",
			strerror(rc),
			result);
		state->fail = true;
		return NULL;
	}
	printf("parent: getpwnam_r in parent succeeded\n");
	return NULL;
}

static void *query_wbc_thread(void *ptr)
{
	struct thread_state *state = ptr;
	struct passwd *ppwd;
	wbcErr wbc_status;
	pid_t pid;
	ssize_t nread, nwritten;
	int p[2];
	int rc;

	while (time(NULL) < state->timeout) {
		wbc_status = wbcGetpwnam(state->username, &ppwd);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			pthread_mutex_lock(&state->lock);
			state->fail = true;
			pthread_mutex_unlock(&state->lock);
			fprintf(stderr,
				"wbcGetpwnam failed with %s\n",
				wbcErrorString(wbc_status));
			break;
		}
		wbcFreeMemory(ppwd);
		state->wbc_loop_count++;
		pthread_mutex_lock(&state->lock);
		if (state->fail) {
			pthread_mutex_unlock(&state->lock);
			break;
		}
		pthread_mutex_unlock(&state->lock);
	}

	rc = socketpair(AF_UNIX, SOCK_STREAM, 0, p);
	if (rc != 0) {
		state->fail = true;
		return NULL;
	}

	/*
	 * Check wbcGetpwnam() still works after a fork,
	 * both in parent and child.
	 */

	pid = fork();
	if (pid == -1) {
		return NULL;
	}
	if (pid == 0) {
		/* Child */
		wbc_status = wbcGetpwnam(state->username, &ppwd);
		if (!WBC_ERROR_IS_OK(wbc_status)) {
			fprintf(stderr,
				"wbcGetpwnam failed with %s\n",
				wbcErrorString(wbc_status));
			rc = 1;
			nwritten = write(p[0], &rc, sizeof(int));
			assert(nwritten == sizeof(int));
			exit(1);
		}
		printf("child: wbcGetpwnam in child succeeded\n");
		rc = 0;
		nwritten = write(p[0], &rc, sizeof(int));
		assert(nwritten == sizeof(int));
		exit(1);
	}

	/* Parent */

	/* Check result from child */
	nread = read(p[1], &rc, sizeof(int));
	if (nread != sizeof(int)) {
		fprintf(stderr,
			"read from child failed with errno='%s' nread=%zd\n",
			strerror(errno),
			nread);
		state->fail = true;
		return NULL;
	}

	if (rc != 0) {
		fprintf(stderr,
			"wbcGetpwnam failed in the child\n");
		state->fail = true;
		return NULL;
	}
	printf("parent: wbcGetpwnam in child succeeded\n");

	/* Verify wbcGetpwnam() in parent after fork */
	wbc_status = wbcGetpwnam(state->username, &ppwd);
	if (!WBC_ERROR_IS_OK(wbc_status)) {
		fprintf(stderr,
			"wbcGetpwnam failed with %s\n",
			wbcErrorString(wbc_status));
		state->fail = true;
		return NULL;
	}
	printf("parent: wbcGetpwnam in parent succeeded\n");
	return NULL;
}

int main(int argc, char *argv[])
{
	int rc, n;
	struct thread_state state;
	pthread_t threads[2];

	if (argc < 2 ) {
		fprintf(stderr,"%s: missing domain user\n", argv[0]);
		return 1;
	}

	state.username = argv[1];
	state.timeout = time(NULL) + RUNTIME;
	rc = pthread_mutex_init(&state.lock, NULL);
	if (rc != 0) {
		fprintf(stderr,
			"pthread_mutex_init failed: %s\n",
			strerror(rc));
		exit(1);
	}
	state.fail = false;
	state.nss_loop_count = 0;
	state.wbc_loop_count = 0;

	printf("query domain user '%s'\n", state.username);

	/* create query threads */
	rc = pthread_create(&threads[0], NULL, query_nss_thread, &state);
	if (rc != 0) {
		fprintf(stderr,
			"creating NSS thread failed: %s\n",
			strerror(rc));
		exit(1);
	}
	rc = pthread_create(&threads[1], NULL, query_wbc_thread, &state);
	if (rc != 0) {
		fprintf(stderr,
			"creating libwbclient thread failed: %s\n",
			strerror(rc));
		exit(1);
	}

	/* wait for query threads to terminate */
	for (n = 0; n < 2; n++) {
		rc = pthread_join(threads[n], NULL);
		if (rc != 0) {
			fprintf(stderr,
				"joining query thread %i failed: %s\n",
				n,
				strerror(rc));
			exit(1);
		}
	}

	fprintf(state.fail ? stderr: stdout,
		"test %s with %i NSS and %i libwbclient calls\n",
		state.fail ? "failed" : "passed",
		state.nss_loop_count,
		state.wbc_loop_count);

	return state.fail;
}
