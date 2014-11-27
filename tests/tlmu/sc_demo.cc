/*
 * Top level of the cosim example.
 *
 * Copyright (c) 2013 Xilinx Inc.
 * Written by Edgar E. Iglesias
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
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "systemc.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"

using namespace sc_core;
using namespace sc_dt;
using namespace std;

#include "iconnect.h"
#include "memory.h"
#include "debugdev.h"
#include "demo-dma.h"
extern "C" {
#include "remote-port-proto.h"
#include "remote-port-sk.h"
};
#include "remote-port-tlm.h"
#include "tlm2apb-bridge.h"

#define NR_MASTERS	2
#define NR_DEVICES	3

SC_MODULE(Top)
{
	SC_HAS_PROCESS(Top);
	remoteport_tlm rp;
	iconnect<NR_MASTERS, NR_DEVICES>	*bus;
	debugdev *debug;
	demodma *dma;

	sc_signal<bool> rst, irq_dma, irq_debug;

	Top(sc_module_name name, const char *sk_descr, sc_time quantum) :
		rp("rp0", -1, sk_descr, 1, 2, 0),
		rst("rst")
	{
		unsigned int i, j;

		m_qk.set_global_quantum(quantum);

		rp.rst(rst);

		bus   = new iconnect<NR_MASTERS, NR_DEVICES> ("bus");
		debug = new debugdev("debug");
		dma = new demodma("demodma");

		bus->memmap(0x16000000ULL, 0x10 - 1,
				ADDRMODE_RELATIVE, -1, debug->socket);
		bus->memmap(0x16000310ULL, 0x10 - 1,
				ADDRMODE_RELATIVE, -1, dma->tgt_socket);
		bus->memmap(0x0LL, 0xffffffff - 1,
				ADDRMODE_RELATIVE, -1, rp.to_rp_sk);

		rp.from_rp_sk[0]->bind(*(bus->t_sk[0]));
		rp.to_rp_irqs[0](irq_dma);
		rp.to_rp_irqs[1](irq_debug);
		dma->init_socket.bind(*(bus->t_sk[1]));
		dma->irq(irq_dma);
		debug->irq(irq_debug);
	}

private:
	tlm_utils::tlm_quantumkeeper m_qk;
};

void usage(void)
{
	cout << "tlm socket-path sync-quantum-ns" << endl;
}

static sc_trace_file *trace_fp = NULL;

int sc_main(int argc, char* argv[])
{
	Top *top;
	uint64_t sync_quantum;

	if (argc < 3) {
		sync_quantum = 10000;
	} else {
		sync_quantum = strtoull(argv[2], NULL, 10);
	}

	sc_set_time_resolution(1, SC_PS);

	top = new Top("top", argv[1], sc_time((double) sync_quantum, SC_NS));
	if (argc < 3) {
		sc_start(1, SC_PS);
		sc_stop();
		usage();
		exit(EXIT_FAILURE);
	}

	top->rst.write(true);
	sc_start(1, SC_US);
	top->rst.write(false);

	sc_start();
	if (trace_fp) {
		sc_close_vcd_trace_file(trace_fp);
	}
	return 0;
}
