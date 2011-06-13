/*
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

#include <setjmp.h>
#include "tlmu-qemuif.h"

struct tlmu_timer {
	int64_t expire_time;
	int pending;

	/* Callback info.  */
	void *o;
	void (*cb)(void *o);

	struct tlmu_timer *next;
};

struct tlmu
{
	const char *name;
	jmp_buf top;

	/* We only need one timer per instance.  */
	struct tlmu_timer timer;

	void *dl_handle;

	/* TODO: Make this dynamic.  */
	const char *argv[100];

        int (*main)(int ignore_sigint, int no_sdl, int no_gui_timer,
	    int argc, const char **argv, char **envp);

	void (*tlm_map_ram)(const char *name,
			    uint64_t addr, uint64_t size, int rw);
	void **tlm_opaque;
	void **tlm_timer_opaque;
	uint64_t *tlm_image_load_base;
	uint64_t *tlm_image_load_size;

	void (*tlm_set_log_filename)(const char *f);
	void (*tlm_notify_event)(enum tlmu_event ev, void *d);
	void (**tlm_timer_start)(void *o,
			void *cb_o, void (*cb)(void *o), int64_t delta);
	void (**tlm_sync)(void *o, int64_t time_ns);
	uint64_t *tlm_sync_period_ns;
	int *tlm_boot_state;
	int (**tlm_bus_access_cb)(void *o, int64_t clk, int rw,
				uint64_t addr, void *data, int len);
	void (**tlm_bus_access_dbg_cb)(void *o, int64_t clk,
			int rw, uint64_t addr, void *data, int len);
	int (*tlm_bus_access)(int rw, uint64_t addr, void *data, int len);
	void (*tlm_bus_access_dbg)(int rw,
				uint64_t addr, void *data, int len);

	void (**tlm_get_dmi_ptr_cb)(void *o, uint64_t addr,
					struct tlmu_dmi *dmi);
	int (*tlm_get_dmi_ptr)(struct tlmu_dmi *dmi);
};

/*
 * Initialize a TLMu instance. Takes an instance name as argument.
 */
void tlmu_init(struct tlmu *t, const char *name);

/*
 * Load an emulator library.
 *
 * Returns zero on success and non-zero if failing to load the
 * emulator.
 */
int tlmu_load(struct tlmu *t, const char *soname);

/*
 * Append an argument to the TLMu instance's argv list.
 */
void tlmu_append_arg(struct tlmu *t, const char *arg);

/*
 * Register a per instance pointer that will be passed back on every
 * callback call.
 */
void tlmu_set_opaque(struct tlmu *t, void *o);

#ifndef TLMU_NO_DECLARE_CB_FUNC_PTR
/*
 * Register a callback function to be called when TLMu emulators need to
 * make bus accesses back into the main emulator.
 *
 * In the callback:
 *  o          - Is the registered instance pointer, see tlm_set_opaque().
 *  clk        - The current TLMu time. (-1 if invalid/unknown).
 *  rw         - 0 for reads, non-zero for write accesses.
 *  data       - Pointer to data
 *  len        - Requested transaction length
 *
 * The callback is expected to return 1 if the accessed unit supports DMI,
 * see tlmu_get_dmi_ptr for more info.
 */
void tlmu_set_bus_access_cb(struct tlmu *t,
		int (*access)(void *o, int64_t clk,
				int rw, uint64_t addr, void *data, int len));
/*
 * Register a callback for debug accesses. The callback works similarily as
 * the one for tlmu_set_bus_access_cb, but it doesn't have a return value.
 *
 * Debug accesses will be made by various debug units, for example the GDB
 * stub or the tracing units when disassembling guest code.
 */
void tlmu_set_bus_access_dbg_cb(struct tlmu *t,
		void (*access)(void *, int64_t, int, uint64_t, void *, int));
/*
 * Register a callback to be called when the TLMu emulator requests a
 * Direct Memory Interface (DMI) area.
 */
void tlmu_set_bus_get_dmi_ptr_cb(struct tlmu *t,
			void (*dmi)(void *, uint64_t, struct tlmu_dmi*));
/*
 * Register a callback function to be called at sync points.
 */
void tlmu_set_sync_cb(struct tlmu *t, void (*cb)(void *, int64_t));
#endif

void tlmu_notify_event(struct tlmu *t, enum tlmu_event ev, void *d);
void tlmu_set_sync_period_ns(struct tlmu *t, uint64_t period_ns);
void tlmu_set_boot_state(struct tlmu *t, int v);

int tlmu_bus_access(struct tlmu *t, int rw,
		uint64_t addr, void *data, int len);
void tlmu_bus_access_dbg(struct tlmu *t,
                        int rw, uint64_t addr, void *data, int len);
/*
 * Try to setup direct memory access to a RAM (or RAM like device).
 *
 * t      - pointer to the TLMu instance
 * dmi    - pointer to a tlmu_dmi structure to fill out.
 *
 * Return 1 if success.
 */
int tlmu_get_dmi_ptr(struct tlmu *t, struct tlmu_dmi *dmi);
void tlmu_set_timer_start_cb(struct tlmu *t, void *o,
	void (*cb)(void *o, void *cb_o, void (*tcb)(void *o), int64_t d_ns));
/*
 * Tell the TLMu instance that a given memory area is maps to RAM.
 *
 * t         - The TLMu instance
 * name      - An name for the RAM
 * addr      - Base address
 * size      - Size of RAM
 * rw        - Zero if ROM, one if writes are allowed.
 */
void tlmu_map_ram(struct tlmu *t, const char *name,
                uint64_t addr, uint64_t size, int rw);
/*
 * Set the per TLMu instance log filename.
 *
 * t         - The TLMu instance
 * f         - Log filename
 */
void tlmu_set_log_filename(struct tlmu *t, const char *f);
void tlmu_set_image_load_params(struct tlmu *t, uint64_t base, uint64_t size);

void tlmu_run(struct tlmu *t);
void tlmu_exit(struct tlmu *t);
static inline void tlmu_delete(struct tlmu *t)
{
	/* FIXME.  */
}

