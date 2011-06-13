/*
 * Interface towards the raw TLMu shared objects.
 *
 * Copyright (c) 2011 Edgar E. Iglesias.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <dlfcn.h>

#include "tlmu.h"

static timer_t tlmu_hosttimer;
pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct tlmu_timer *timers = NULL;

static void tlmu_hosttimer_start(int64_t delta_ns)
{
	struct itimerspec timeout;

	timeout.it_interval.tv_sec = 0;
	timeout.it_interval.tv_nsec = 0;
	timeout.it_value.tv_sec =  delta_ns / 1000000000;
	timeout.it_value.tv_nsec = delta_ns % 1000000000;
	if (timer_settime(tlmu_hosttimer, 0, &timeout, NULL)) {
		perror("settime");
		exit(1);
	}
}

static int64_t tlmu_timers_run(int64_t current_ns)
{
	struct tlmu_timer *t = timers;
	int64_t next_deadline = INT64_MAX;

	while (t) {
		if (t->pending) {
			if (current_ns > t->expire_time) {
				t->pending = 0;
				t->cb(t->o);
			} else {
				if (t->expire_time < next_deadline) {
					next_deadline = t->expire_time;
				}
			}
		}
		t = t->next;
	}
	return next_deadline;
}

static void tlmu_hosttimer_handler(int host_signum)
{
	struct timespec tp;
	int64_t current_ns;
	int64_t next_ns;

//	printf("%s:\n", __func__);
	clock_gettime(CLOCK_REALTIME, &tp);

	current_ns = tp.tv_sec * 1000000000LL + tp.tv_nsec;
	next_ns = tlmu_timers_run(current_ns);
	tlmu_hosttimer_start(next_ns - current_ns);
}

static void tlmu_hosttimer_block(void)
{
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
		perror("sigprocmask");
		exit(1);
	}
}

static void tlmu_hosttimer_unblock(void)
{
	sigset_t mask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1) {
		perror("sigprocmask");
		exit(1);
	}
}

static void tlmu_timer_start(void *o,
			void *cb_o, void (*cb)(void *), int64_t delta_ns)
{
	struct tlmu *q = o;
	struct timespec tp;
	int64_t current_ns, next_ns;

//	printf("%s: delta=%ld\n", __func__, delta_ns);
	if (delta_ns < 0) {
		cb(cb_o);
		return;
	}

	tlmu_hosttimer_block();
	pthread_mutex_lock(&timer_mutex);
	if (clock_gettime(CLOCK_REALTIME, &tp)) {
		perror("clock_gettime");
		exit(1);
	}

	current_ns = tp.tv_sec * 1000000000LL + tp.tv_nsec;

	q->timer.expire_time = current_ns + delta_ns;
	q->timer.o = cb_o;
	q->timer.cb = cb;
	q->timer.pending = 1;

	/* FIXME: Walk the timers and get the nearset deadline.  */
	next_ns = tlmu_timers_run(current_ns);
	tlmu_hosttimer_start(next_ns - current_ns);

	pthread_mutex_unlock(&timer_mutex);
	tlmu_hosttimer_unblock();
}

static void tlmu_timers_init(void)
{
	struct sigevent ev;
	struct sigaction act;

	sigfillset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = tlmu_hosttimer_handler;

	sigaction(SIGALRM, &act, NULL);

	memset(&ev, 0, sizeof(ev));
	ev.sigev_value.sival_int = 0;
	ev.sigev_notify = SIGEV_SIGNAL;
	ev.sigev_signo = SIGALRM;
	if (timer_create(CLOCK_REALTIME, &ev, &tlmu_hosttimer)) {
		perror("timer_create\n");
		exit(1);
	}
}


void tlmu_init(struct tlmu *t, const char *name)
{
	static int init = 0; /* protected by timer mutex.  */

	memset(t, 0, sizeof *t);
	t->name = name;

	/* Setup the default args.  */
	tlmu_append_arg(t, "TLMu");

	tlmu_append_arg(t, "-qmp");
	tlmu_append_arg(t, "null");

	tlmu_append_arg(t, "-clock");
	tlmu_append_arg(t, "tlm");

	/* Link in our timer as non-pending.  */
	pthread_mutex_lock(&timer_mutex);
	if (!init) {
		tlmu_timers_init();
		tlmu_hosttimer_unblock();
		init = 1;
	}

	t->timer.next = timers;
	timers = &t->timer;
	pthread_mutex_unlock(&timer_mutex);
}

static void copylib(const char *path, const char *newpath)
{
	int s = -1, d = -1;
	ssize_t r, wr;
	const char *ld_path = NULL;
	Dl_info info;
	void *handle;
	void *addr;
	int ret;

	handle = dlopen(path, RTLD_LOCAL | RTLD_DEEPBIND | RTLD_NOW);
	if (!handle) {
		perror("path");
		goto err;
	}

	addr = dlsym(handle, "vl_main");
	ret = dladdr(addr, &info);
	if (!ret) {
		perror("dladdr");
		fprintf(stderr, "vl_main doesn't exist in TLMu??\n");
		goto err;
	}

	ld_path = strdup(info.dli_fname);
	dlclose(handle);

	/* Now copy it into our per instance store.  */
	s = open(ld_path, O_RDONLY);
	if (s < 0) {
		perror(ld_path);
		goto err;
	}

	d = open(newpath, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG);
	if (d < 0) {
		if (errno != EEXIST) {
			perror(newpath);
		}
		goto err;
	}
	do {
		char buf[4 * 1024];
		r = read(s, buf, sizeof buf);
		if (r < 0 && (errno == EINTR || errno == EAGAIN))
			continue;
		/* TODO: handle partial writes.  */
		if (r > 0) {
			wr = write(d, buf, r);
		}
	} while (r);
err:
	free((void *) ld_path);
	close(s);
	close(d);
}

int tlmu_load(struct tlmu *q, const char *soname)
{
	char *libname;
	char *logname;
	int err = 0;
	int n;

	mkdir(".tlmu", S_IRWXU | S_IRWXG);

	n = asprintf(&libname, ".tlmu/%s-%s", soname, q->name);
	copylib(soname, libname);

	q->dl_handle = dlopen(libname, RTLD_LOCAL | RTLD_DEEPBIND | RTLD_NOW);
//	err = unlink(libname);
	if (err) {
		perror(libname);
	}
	free(libname);
	if (!q->dl_handle)
		return 1;

	q->main = dlsym(q->dl_handle, "vl_main");
	q->tlm_set_log_filename = dlsym(q->dl_handle, "cpu_set_log_filename");
	q->tlm_image_load_base = dlsym(q->dl_handle, "tlm_image_load_base");
	q->tlm_image_load_size = dlsym(q->dl_handle, "tlm_image_load_size");
	q->tlm_map_ram = dlsym(q->dl_handle, "tlm_map_ram");
	q->tlm_opaque = dlsym(q->dl_handle, "tlm_opaque");
	q->tlm_notify_event = dlsym(q->dl_handle, "tlm_notify_event");
	q->tlm_timer_opaque = dlsym(q->dl_handle, "tlm_timer_opaque");
	q->tlm_timer_start = dlsym(q->dl_handle, "tlm_timer_start");
	q->tlm_sync = dlsym(q->dl_handle, "tlm_sync");
	q->tlm_sync_period_ns = dlsym(q->dl_handle, "tlm_sync_period_ns");
	q->tlm_boot_state = dlsym(q->dl_handle, "tlm_boot_state");
	q->tlm_bus_access_cb = dlsym(q->dl_handle, "tlm_bus_access_cb");
	q->tlm_bus_access_dbg_cb = dlsym(q->dl_handle, "tlm_bus_access_dbg_cb");
	q->tlm_bus_access = dlsym(q->dl_handle, "tlm_bus_access");
	q->tlm_bus_access_dbg = dlsym(q->dl_handle, "tlm_bus_access_dbg");
	q->tlm_get_dmi_ptr_cb = dlsym(q->dl_handle, "tlm_get_dmi_ptr_cb");
	q->tlm_get_dmi_ptr = dlsym(q->dl_handle, "tlm_get_dmi_ptr");
	tlmu_set_timer_start_cb(q, q, tlmu_timer_start);
	if (!q->main
		|| !q->tlm_map_ram
		|| !q->tlm_set_log_filename
		|| !q->tlm_image_load_base
		|| !q->tlm_image_load_size
		|| !q->tlm_opaque
		|| !q->tlm_notify_event
		|| !q->tlm_timer_start
		|| !q->tlm_sync
		|| !q->tlm_sync_period_ns
		|| !q->tlm_boot_state
		|| !q->tlm_bus_access_cb
		|| !q->tlm_bus_access_dbg_cb
		|| !q->tlm_bus_access
		|| !q->tlm_bus_access_dbg
		|| !q->tlm_get_dmi_ptr_cb
		|| !q->tlm_get_dmi_ptr) {
		dlclose(q->dl_handle);
		return 1;
	}

	n = asprintf(&logname, ".tlmu/%s-%s.log", soname, q->name);
	tlmu_set_log_filename(q, logname);
	free(logname);
	return 0;
}

void tlmu_notify_event(struct tlmu *q, enum tlmu_event ev, void *d)
{
	q->tlm_notify_event(ev, d);
}

void tlmu_set_opaque(struct tlmu *q, void *o)
{
	*q->tlm_opaque = o;
}

void tlmu_set_bus_access_cb(struct tlmu *q,
		int (*access)(void *, int64_t, int, uint64_t, void *, int))
{
	*q->tlm_bus_access_cb = access;
}

void tlmu_set_bus_access_dbg_cb(struct tlmu *q,
		void (*access)(void *, int64_t, int, uint64_t, void *, int))
{
	*q->tlm_bus_access_dbg_cb = access;
}

void tlmu_set_bus_get_dmi_ptr_cb(struct tlmu *q,
			void (*dmi)(void *, uint64_t, struct tlmu_dmi*))
{
	*q->tlm_get_dmi_ptr_cb = dmi;
}

void tlmu_set_sync_period_ns(struct tlmu *q, uint64_t period_ns)
{
	*q->tlm_sync_period_ns = period_ns;
}

void tlmu_set_boot_state(struct tlmu *q, int v)
{
	*q->tlm_boot_state = v;
}

void tlmu_set_sync_cb(struct tlmu *q, void (*cb)(void *, int64_t))
{
	*q->tlm_sync = cb;
}

void tlmu_set_timer_start_cb(struct tlmu *q, void *o,
	void (*cb)(void *o, void *cb_o, void (*tcb)(void *o), int64_t d_ns))
{
	*q->tlm_timer_opaque = o;
	*q->tlm_timer_start = cb;
}

int tlmu_bus_access(struct tlmu *q, int rw, uint64_t addr, void *data, int len)
{
	return q->tlm_bus_access(rw, addr, data, len);
}

void tlmu_bus_access_dbg(struct tlmu *q,
			int rw, uint64_t addr, void *data, int len)
{
	q->tlm_bus_access_dbg(rw, addr, data, len);
}

int tlmu_get_dmi_ptr(struct tlmu *q, struct tlmu_dmi *dmi)
{
	return q->tlm_get_dmi_ptr(dmi);
}

void tlmu_map_ram(struct tlmu *q, const char *name,
		uint64_t addr, uint64_t size, int rw)
{
	q->tlm_map_ram(name, addr, size, rw);
}

void tlmu_set_log_filename(struct tlmu *q, const char *f)
{
	q->tlm_set_log_filename(f);
}

void tlmu_set_image_load_params(struct tlmu *q, uint64_t base, uint64_t size)
{
	*q->tlm_image_load_base = base;
	*q->tlm_image_load_size = size;
}

void tlmu_append_arg(struct tlmu *t, const char *arg)
{
	int i = 0;

	while (t->argv[i])
		i++;

	if (i + 1 >= (sizeof t->argv / sizeof t->argv[0])) {
		assert(0);
	}

	t->argv[i] = arg;
	t->argv[i + 1] = NULL;
}

void tlmu_run(struct tlmu *t)
{
	int argc = 0;
	int done;

	while (t->argv[argc])
		argc++;

	done = setjmp(t->top);
	if (!done)
		t->main(0, 1, 1, argc, t->argv, NULL);
}

void tlmu_exit(struct tlmu *t)
{
	longjmp(t->top, 1);
}
