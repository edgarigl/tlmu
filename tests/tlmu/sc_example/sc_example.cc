/*
 * TLMu System-C TLM-2.0 example app.
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

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"

using namespace sc_core;
using namespace std;

#include "iconnect.h"
#include "tlmu_sc.h"
#include "memory.h"
#include "magicdev.h"

#define NR_CPUS		3
#define NR_DEVICES	3

SC_MODULE(Top)
{
	tlmu_sc   *cpu[NR_CPUS];
	tlm_utils::simple_initiator_socket<Top> to_cris_sk;
	tlm_utils::simple_initiator_socket<Top> to_cris_irq_sk;
#if NR_CPUS >= 2
	tlm_utils::simple_initiator_socket<Top> to_arm_sk;
	tlm_utils::simple_initiator_socket<Top> to_arm_irq_sk;
#endif
#if NR_CPUS >= 3
	tlm_utils::simple_initiator_socket<Top> to_mipsel_sk;
	tlm_utils::simple_initiator_socket<Top> to_mipsel_irq_sk;
#endif
	iconnect<NR_CPUS, NR_DEVICES>	*bus;
	memory  *mem[2];
	magicdev *magic;

	Top(sc_module_name name) :
#if NR_CPUS >= 2
		to_arm_sk("toarm"),
		to_arm_irq_sk("toarm_irq"),
#endif
#if NR_CPUS >= 3
		to_mipsel_sk("tomipsel"),
		to_mipsel_irq_sk("tomipsel_irq"),
#endif
		to_cris_sk("tocris"),
		to_cris_irq_sk("tocris_irq")
	{
		unsigned int i, j;

		cpu[0] = new tlmu_sc("cris0", "libtlmu-cris.so",
					NULL,
					"crisv10",
					200 * TLMU_MHZ,
					"cris-guest/guest",
					tlmu_sc::TRACING_OFF,
					NULL,
					TLMU_BOOT_RUNNING,
					100 * 1000ULL);
#if NR_CPUS >= 2
		cpu[1] = new tlmu_sc("arm0", "libtlmu-arm.so",
					NULL,
					"arm926",
					200 * TLMU_MHZ,
					"arm-guest/guest",
					tlmu_sc::TRACING_OFF,
					NULL,
					TLMU_BOOT_RUNNING,
					100 * 1000ULL);
#endif
#if NR_CPUS >= 3
		cpu[2] = new tlmu_sc("mipsel0", "libtlmu-mipsel.so",
					NULL,
					"24Kc",
					200 * TLMU_MHZ,
					"mipsel-guest/guest",
					tlmu_sc::TRACING_OFF,
					NULL,
					TLMU_BOOT_RUNNING,
					100 * 1000ULL);
#endif
		bus   = new iconnect<NR_CPUS, NR_DEVICES> ("bus");

		for (i = 0; i < NR_CPUS; i++) {
			cpu[i]->from_tlmu_sk.bind(*(bus->t_sk[i]));
		}
		magic = new magicdev("magicdev");

		static struct {
			const char *name;
			uint64_t base, size;
			int rw;
			int access_delay_ns;
		} rams[] = {
			{"rom", 0x18000000ULL, 128 * 1024, 0, 5},
			{"ram", 0x19000000ULL, 128 * 1024, 1, 5},
		};

		for (i = 0; i < sizeof rams / sizeof rams[0]; i++) {
			mem[i] = new memory(rams[i].name,
					sc_time(rams[i].access_delay_ns, SC_NS),
					rams[i].size);
			bus->memmap(rams[i].base, rams[i].size,
					ADDRMODE_RELATIVE, -1, mem[i]->socket);
			for (j = 0; j < NR_CPUS; j++) {
				cpu[j]->map_ram(rams[i].name,
					rams[i].base, rams[i].size, rams[i].rw);
			}
		}

		bus->memmap(0x10500000ULL, 1 * 1024,
				ADDRMODE_RELATIVE, -1, magic->socket);

		/* Dummy IRQ connections.  */
		cpu[0]->to_tlmu_sk.bind(to_cris_sk);
		cpu[0]->to_tlmu_irq_sk.bind(to_cris_irq_sk);
#if NR_CPUS >= 2
		cpu[1]->to_tlmu_sk.bind(to_arm_sk);
		cpu[1]->to_tlmu_irq_sk.bind(to_arm_irq_sk);
#endif
#if NR_CPUS >= 3
		cpu[2]->to_tlmu_sk.bind(to_mipsel_sk);
		cpu[2]->to_tlmu_irq_sk.bind(to_mipsel_irq_sk);
#endif
		m_qk.set_global_quantum(sc_time(1, SC_US));
	}

private:
	tlm_utils::tlm_quantumkeeper m_qk;
};

int sc_main(int argc, char* argv[])
{
	Top *top;

	sc_set_time_resolution(1, SC_NS);

	top = new Top("top");
	sc_start();
	return 0;
}
