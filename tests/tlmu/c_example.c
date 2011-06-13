/*
 * TLMu C example app.
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

#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>
#include <dlfcn.h>

#include "tlmu.h"

/* Every core is connected to a shared bus that maps:

   0x10500000 Magic simulator device (Write only)
   0x18000000 128KB ROM
   0x19000000 128KB RAM
*/

uint32_t rom[128 * 1024 / 4];
uint32_t ram[128 * 1024 / 4];

struct tlmu_wrap {
	struct tlmu q;
	const char *name;
};

void tlm_get_dmi_ptr(void *o, uint64_t addr, struct tlmu_dmi *dmi)
{
	if (addr >= 0x19000000 && addr <= (0x19000000 + sizeof ram)) {
		dmi->ptr = (void *) &ram[0];
		dmi->base = 0x19000000;
		dmi->size = 128 * 1024;
		dmi->prot = TLMU_DMI_PROT_READ | TLMU_DMI_PROT_WRITE;
	}
}

void tlm_bus_write(void *o, int dbg, int64_t clk,
                  uint64_t addr, const void *data, int len)
{
	struct tlmu_wrap *t = o;

	/* Magic simulator device.  */
	if (addr >= 0x10500000 && addr <= 0x10500100) {
		addr -= 0x10500000;
		switch (addr) {
		case 0x0:
			printf("%s: TRACE: %x\n", t->name,
				*(uint32_t *)data);
			break;
		case 0x4:
			putchar(*(uint32_t *)data);
			break;
		default:
			printf("%s: STOP: %x\n", t->name,
					*(uint32_t *)data);
			tlmu_exit(&t->q);
			break;
		}
	}

	if (addr >= 0x18000000 && addr <= (0x18000000 + sizeof rom)) {
		/* Disallow writes to ROM in non debug mode.  */
		if (!dbg)
			return;

		unsigned char *dst = (void *) &rom[0];
		addr -= 0x18000000;
		memcpy(&dst[addr], data, len);
	} else
        if (addr >= 0x19000000 && addr <= (0x19000000 + sizeof ram)) {
		unsigned char *dst = (void *) ram;
		addr -= 0x19000000;
		memcpy(&dst[addr], data, len);
        }
}

int tlm_bus_access1(void *o, int dbg, int64_t clk, int rw,
			uint64_t addr, void *data, int len)
{
	if (rw) {
		tlm_bus_write(o, dbg, clk, addr, data, len);
		return 1;
	}

	/* Read.  */
	if (addr >= 0x18000000 && addr <= (0x18000000 + sizeof rom)) {
		unsigned char *src = (void *) rom;
		addr -= 0x18000000;
		memcpy(data, &src[addr], len);
	} else
        if (addr >= 0x19000000 && addr <= (0x19000000 + sizeof ram)) {
		unsigned char *src = (void *) ram;
		addr -= 0x19000000;
		memcpy(data, &src[addr], len);
        }
	return 1;
}

int tlm_bus_access(void *o, int64_t clk, int rw,
			uint64_t addr, void *data, int len)
{
	return tlm_bus_access1(o, 0, clk, rw, addr, data, len);
}

void tlm_bus_access_dbg(void *o, int64_t clk, int rw,
			uint64_t addr, void *data, int len)
{
	tlm_bus_access1(o, 1, clk, rw, addr, data, len);
}

void *run_tlmu(void *p)
{
	struct tlmu_wrap *t = p;
	tlmu_run(&t->q);
	return NULL;
}

void tlm_sync(void *o, int64_t time_ns)
{
}

int main(int argc, char **argv)
{
	int i;
	int err;
	struct {
		char *soname;
		char *name;
		char *cputype;
		char *elfimage;

		struct tlmu_wrap t;
		pthread_t tid;
	} sys[] = {
	{"libtlmu-arm.so", "ARM", "arm926", "arm-guest/guest"},
	{"libtlmu-cris.so", "CRIS", "crisv10", "cris-guest/guest"},
	{"libtlmu-mipsel.so", "MIPS", "24Kc", "mipsel-guest/guest"},
	{NULL, NULL, NULL, NULL}
	};

	i = 0;
	while (sys[i].name) {
		sys[i].t.name = sys[i].name;

		tlmu_init(&sys[i].t.q, sys[i].t.name);
		err = tlmu_load(&sys[i].t.q, sys[i].soname);
		if (err) {
			printf("failed to load tlmu %s\n", sys[i].soname);
			exit(1);
		}

		/* Use the bare CPU core.  */
		tlmu_append_arg(&sys[i].t.q, "-M");
		tlmu_append_arg(&sys[i].t.q, "tlm-mach");

		tlmu_append_arg(&sys[i].t.q, "-icount");
		tlmu_append_arg(&sys[i].t.q, "1");

#if 0
		/* Enable exec tracing.  */
		tlmu_append_arg(&sys[i].t.q, "-d");
		tlmu_append_arg(&sys[i].t.q, "in_asm,exec,cpu");
#endif

		tlmu_append_arg(&sys[i].t.q, "-cpu");
		tlmu_append_arg(&sys[i].t.q, sys[i].cputype);

		tlmu_append_arg(&sys[i].t.q, "-kernel");
		tlmu_append_arg(&sys[i].t.q, sys[i].elfimage);

		/*
		 * Register our per instance pointer carried back in
		 * callbacks.
		 */
		tlmu_set_opaque(&sys[i].t.q, &sys[i].t);

		/* Register our callbacks.  */
		tlmu_set_bus_access_cb(&sys[i].t.q, tlm_bus_access);
		tlmu_set_bus_access_dbg_cb(&sys[i].t.q, tlm_bus_access_dbg);
		tlmu_set_bus_get_dmi_ptr_cb(&sys[i].t.q, tlm_get_dmi_ptr);
		tlmu_set_sync_cb(&sys[i].t.q, tlm_sync);

		/* Tell TLMu how often it should break out from executing
		 * guest code and synchronize.  */
		tlmu_set_sync_period_ns(&sys[i].t.q, 1 * 100 * 1000ULL);
		/* Tell TLMu if the CPU should start in running or sleeping
		 * mode.  */
		tlmu_set_boot_state(&sys[i].t.q, TLMU_BOOT_RUNNING);

		/*
		 * Tell TLMu what memory areas that map actual RAM. This needs
		 * to be done for RAM's that are not internal to the TLMu
		 * emulator, but managed by the main emulator or by other
		 * TLMu instances.
		 */
		tlmu_map_ram(&sys[i].t.q, "rom", 0x18000000ULL, 128 * 1024, 0);
		tlmu_map_ram(&sys[i].t.q, "ram", 0x19000000ULL, 128 * 1024, 1);

		pthread_create(&sys[i].tid, NULL, run_tlmu, &sys[i].t);
		i++;
	}

	i = 0;
	while (sys[i].name) {
		pthread_join(sys[i].tid, NULL);
		i++;
	}
	return 0;
}
