#include <inttypes.h>
#include <stdlib.h>
#include "tlmu-qemuif.h"

/* The main SystemC opaque handler. Passed on most callbacks from QEMU to
   SystemC.  */
void *tlm_opaque;
/* Callbacks whenever QEMU needs to make a bus access to the SystemC
   world.  */
int (*tlm_bus_access_cb)(void *o, int64_t clk, int rw,
                          uint64_t addr, void *data, int len);
void (*tlm_bus_access_dbg_cb)(void *o, int64_t clk, int rw, uint64_t addr,
                              void *data, int len);

void (*tlm_get_dmi_ptr_cb)(void *o, uint64_t addr,
                           struct tlmu_dmi *dmi) = 0;

/* Used to call out into the SystemC world every time the CPU gets
   a chance to synchronize. Typically done at TB exit and at
   external bus accesses.  */
void (*tlm_sync)(void *o, uint64_t time_ns);

/* Used to manage timers. Each QEMU instance would like to register it's
   own SIGALRM handler, but that doesn't work when we are all sharing a
   single address space and signal handlers. So, we implement a TLM -clock
   that calls out to the SystemC workd for timer management. SystemC will
   call our callback when timers expire.  */
void *tlm_timer_opaque;
void (*tlm_timer_start)(void *q, void *o, void (*cb)(void * o), int64_t delta);

/* The time between preemptive QEMU syncs, e.g the VCPU gets preemted to
   sync.  */
uint64_t tlm_sync_period_ns = 0;

int tlm_boot_state;

uint64_t tlm_image_load_base = 0;
uint64_t tlm_image_load_size = 0;

int tlm_iodev_is_ram(int iodev) __attribute__((weak));
int tlm_iodev_is_ram(int iodev)
{
    return 0;
}
