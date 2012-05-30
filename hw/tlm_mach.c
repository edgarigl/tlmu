/*
 * QEMU machine for wrapping QEMU cpus as TLM initiators.
 *
 * Copyright (c) 2011 Edgar E. Iglesias, Axis Communications AB.
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

#include "sysbus.h"
#include "net.h"
#include "flash.h"
#include "boards.h"
#include "sysemu.h"
#include "etraxfs.h"
#include "loader.h"
#include "elf.h"

#ifdef TARGET_MIPS
#include "mips.h"
#include "mips_cpudevs.h"
#endif

#ifdef TARGET_ARM
#include "arm-misc.h"
#endif

#include "tlm.h"
#include "tlm_mem.h"

#define D(x)

#define VIRT_TO_PHYS_ADDEND (0LL)

static uint64_t bootstrap_pc;

static void configure_cpu(CPUState *env)
{
#ifdef TARGET_MIPS
    /* TODO: make this configurable somehow??  */
    /* Timer interrupt on line 4.  */
    env->CP0_IntCtl = 0xdc000000;

    /* The 34Kc on the ARTPEC-5 uses a vectored external interrupt ctrl.  */
    env->CP0_Config3 |= (1 << CP0C3_VEIC);
#endif
}

static void main_cpu_reset(void *opaque)
{
    CPUState *env = opaque;
    cpu_reset(env);
    configure_cpu(env);

#ifdef TARGET_CRIS
    env->pc = bootstrap_pc;
#elif defined(TARGET_MIPS)
    env->active_tc.PC = bootstrap_pc;
#elif defined(TARGET_ARM)
    env->regs[15] = bootstrap_pc;
#endif

    if (tlm_boot_state == TLMU_BOOT_SLEEPING) {
        cpu_interrupt(env, CPU_INTERRUPT_HALT);
    }
}

static uint64_t translate_kaddr(void *opaque, uint64_t addr)
{
    return addr + VIRT_TO_PHYS_ADDEND;
}

static
void tlm_mach_init_common (ram_addr_t ram_size,
                       const char *boot_device,
                       const char *kernel_filename, const char *kernel_cmdline,
                       const char *initrd_filename, const char *cpu_model)
{
    CPUState *env_;
    qemu_irq *cpu_irq;
    int kernel_size;

    /* init CPUs */
    if (cpu_model == NULL) {
        fprintf(stderr, "FATAL: CPU model not chosen.\n");
        exit(1);
    }

    env_ = cpu_init(cpu_model);

    if (!env_) {
        fprintf(stderr, "FATAL: Unable to create cpu %s env=%p\n", cpu_model, env_);
        exit(1);
    }
    qemu_register_reset(main_cpu_reset, env_);

    configure_cpu(env_);
#ifdef TARGET_CRIS
    cpu_irq = cris_pic_init_cpu(env_);
    tlm_map(env_, 0x0ULL, 0xffffffffULL,
            tlm_sync_period_ns, cpu_irq, 1, &env_->interrupt_vector);
#elif defined(TARGET_MIPS)
    cpu_irq = NULL;
    cpu_mips_irq_init_cpu(env_);
    cpu_mips_clock_init(env_);
    tlm_map(env_, 0x0ULL, 0xffffffffULL,
            tlm_sync_period_ns, cpu_irq, 0, NULL);
#elif defined(TARGET_ARM)
    cpu_irq = arm_pic_init_cpu(env_);
    tlm_map(env_, 0x0ULL, 0xffffffffULL,
            tlm_sync_period_ns, cpu_irq, 2, NULL);
#endif

    tlm_register_rams();

   if (kernel_filename) {
        uint64_t entry, low, high;

        kernel_size = load_elf(kernel_filename, translate_kaddr, NULL,
                               &entry, &low, &high, 0, ELF_MACHINE, 0);

        bootstrap_pc = entry;
        if (kernel_size < 0) {
            kernel_size = load_image_targphys(kernel_filename,
                                              tlm_image_load_base,
                                              tlm_image_load_size);
            low = bootstrap_pc = tlm_image_load_base;
            high = tlm_image_load_base + kernel_size;
        }

        if (kernel_size < 0) {
            fprintf(stderr, "Unable to open %s\n", kernel_filename);
            exit(1);
        }
    } else {
#ifdef TARGET_CRIS
        bootstrap_pc = 0x0;
#endif
#ifdef TARGET_MIPS
        bootstrap_pc = 0x1fc00000LL;
#endif
#ifdef TARGET_ARM
        bootstrap_pc = 0x0;
#endif
    }
}

static
void tlm_mach_init (ram_addr_t ram_size,
                const char *boot_device,
                const char *kernel_filename, const char *kernel_cmdline,
                const char *initrd_filename, const char *cpu_model)
{
    tlm_mach_init_common(ram_size, boot_device, kernel_filename, kernel_cmdline,
                         initrd_filename, cpu_model);
}

static QEMUMachine tlm_mach_machine = {
    .name = "tlm-mach",
    .desc = "TLM Machine",
    .init = tlm_mach_init,
};

static void tlm_mach_machine_init(void)
{
    qemu_register_machine(&tlm_mach_machine);
}

machine_init(tlm_mach_machine_init);
