/*
 * TLMu glue
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

#include "systemc.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"

#include "tlmu_sc.h"

using namespace sc_core;
using namespace std;

tlmu_sc::tlmu_sc(sc_module_name name, const char *soname,
			const char *mach_name,
			const char *cpu_model,
			uint64_t freq_hz,
			const char *elf_filename,
			int tracing,
			const char *gdb_conn,
			int boot_state,
			uint64_t sync_period_ns)
        : sc_module(name),
	  from_tlmu_sk("fromTLMuSocket"),
	  to_tlmu_sk("toTLMuSocket"),
	  to_tlmu_irq_sk("toTLMuIRQSocket"),
	  mach_name(mach_name),
	  cpu_model(cpu_model),
	  elf_filename(elf_filename),
	  tracing(tracing),
	  gdb_conn(gdb_conn)
{
	int err;
	
	from_tlmu_sk.register_invalidate_direct_mem_ptr(this,
			&tlmu_sc::invalidate_direct_mem_ptr);

	/* Accesses from System-C TLM into TLMu bus.  */
	to_tlmu_sk.register_b_transport(this, &tlmu_sc::to_tlmu_b_transport);
	to_tlmu_sk.register_transport_dbg(this, &tlmu_sc::to_tlmu_transport_dbg);
	to_tlmu_sk.register_get_direct_mem_ptr(this,
					&tlmu_sc::to_tlmu_get_direct_mem_ptr);
	/* Register the IRQ callbacks.  */
	to_tlmu_irq_sk.register_b_transport(this, &tlmu_sc::irq_b_transport);

	speed_factor = (1 * TLMU_GHZ);
	speed_factor /= freq_hz;
	last_sync = 0;

	tlmu_init(&q, name);

	err = tlmu_load(&q, soname);
	if (err) {
		printf("unable to load qemu lib! %s\n", soname);
		exit(1);
	}
	tlmu_set_opaque(&q, this);
	tlmu_set_sync_period_ns(&q, sync_period_ns);
	tlmu_set_bus_access_cb(&q, &tlmu_sc::bus_access);
	tlmu_set_bus_access_dbg_cb(&q, &tlmu_sc::bus_access_dbg);
	tlmu_set_bus_get_dmi_ptr_cb(&q, &tlmu_sc::get_dmi_ptr);
	tlmu_set_sync_cb(&q, &tlmu_sc::sync);
	tlmu_set_boot_state(&q, boot_state);

	SC_THREAD(process);
	m_qk.reset();
}

void tlmu_sc::set_image_load_params(uint64_t base, uint64_t size)
{
	tlmu_set_image_load_params(&q, base, size);
}

void tlmu_sc::sync_time(int64_t tlmu_time_ns)
{
	/* Did QEMU provide a valid time ?  */
	if (tlmu_time_ns != -1) {
		double delta_ns;
		delta_ns = tlmu_time_ns - last_sync;
		last_sync = tlmu_time_ns;

		/* We run QEMU with -icount 1, meaning QEMU will execute
		   one insn every 2ns (2^N where N is the icount value).
		   Here we transform delta_ns into a 1Ghz CPU freq.  */
		delta_ns /= 2;

		/* Now scale it according to the request freq.  */
		delta_ns *= speed_factor;
		m_qk.inc(sc_time(delta_ns, SC_NS));
	}
	if (m_qk.need_sync()) {
		m_qk.sync();
	}
}

void tlmu_sc::get_dmi_ptr(uint64_t addr, struct tlmu_dmi *dmi)
{
	tlm::tlm_generic_payload tr;
	tlm::tlm_dmi dmi_data;
	bool r;

	dmi_data.init();
	tr.set_address(addr);
	r = from_tlmu_sk->get_direct_mem_ptr(tr, dmi_data);
	if (r) {
		sc_time latency;
		double l;

		dmi->ptr = dmi_data.get_dmi_ptr();
		dmi->base = dmi_data.get_start_address();
		dmi->size = dmi_data.get_end_address() - dmi->base;
		dmi->prot = TLMU_DMI_PROT_NONE;

		if (dmi_data.is_read_allowed()) {
			dmi->prot |= TLMU_DMI_PROT_READ;
		}
		if (dmi_data.is_write_allowed()) {
			dmi->prot |= TLMU_DMI_PROT_WRITE;
		}

		/* Convert the latencies to insn counts, can we do better?  */
		latency = dmi_data.get_read_latency();
		l = latency.to_seconds() * 1000 * 1000 * 1000;
		dmi->read_latency = (unsigned int) ((l + 1) / 2);

		latency = dmi_data.get_write_latency();
		l = latency.to_seconds() * 1000 * 1000 * 1000;
		dmi->write_latency = (unsigned int) ((l + 1) / 2);
	}
}

void tlmu_sc::invalidate_direct_mem_ptr(sc_dt::uint64 start,
				sc_dt::uint64 end)
{
	struct tlmu_dmi dmi;

	dmi.base = start;
	dmi.size = end - start;
	tlmu_notify_event(&q, TLMU_TLM_EVENT_INVALIDATE_DMI, &dmi);
}

int tlmu_sc::bus_access(int64_t clk, int rw,
			uint64_t addr, void *data, int len)
{
	tlm::tlm_generic_payload tr;
	sc_time delay;

#if 0
	printf("%s: rw=%d addr=%lx len=%d data=%x\n", __func__,
			rw, addr, len, *(uint32_t *)data);
#endif
	tr.set_command(rw ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND );
	tr.set_address(addr);
	tr.set_data_ptr((unsigned char *)data);
	tr.set_data_length(len);
	tr.set_streaming_width(len);
	tr.set_dmi_allowed(false);
	tr.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	from_tlmu_sk->b_transport(tr, delay);

	m_qk.inc(delay);
	sync_time(clk);
	return tr.is_dmi_allowed();
}

void tlmu_sc::bus_access_dbg(int64_t clk, int rw,
				uint64_t addr, void *data, int len)
{
	tlm::tlm_generic_payload tr;

	//printf("%s: rw=%d addr=%lx len=%d\n", __func__, rw, addr, len);
	tr.set_command(rw ? tlm::TLM_WRITE_COMMAND : tlm::TLM_READ_COMMAND );
	tr.set_address(addr);
	tr.set_data_ptr((unsigned char *)data);
	tr.set_data_length(len);
	tr.set_streaming_width(len);
	tr.set_dmi_allowed(false);
	tr.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	from_tlmu_sk->transport_dbg(tr);
}

bool tlmu_sc::to_tlmu_get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
					tlm::tlm_dmi& dmi_data)
{
	struct tlmu_dmi dmi;
	int r;

	dmi.base = trans.get_address();
	r = tlmu_get_dmi_ptr(&q, &dmi);
	if (!r) {
		dmi_data.allow_none();
		dmi_data.set_start_address(0);
		dmi_data.set_end_address(0);
		return false;
	}

	switch (dmi.prot & (TLMU_DMI_PROT_READ | TLMU_DMI_PROT_WRITE)) {
		case TLMU_DMI_PROT_READ:
			dmi_data.allow_read();
			break;
		case TLMU_DMI_PROT_WRITE:
			dmi_data.allow_write();
			break;
		case TLMU_DMI_PROT_READ | TLMU_DMI_PROT_WRITE:
			dmi_data.allow_read_write();
			break;
		default:
			dmi_data.allow_none();
			return false;
	}

	dmi_data.set_dmi_ptr(reinterpret_cast<unsigned char*>(dmi.ptr));
	dmi_data.set_start_address(dmi.base);
	dmi_data.set_end_address(dmi.base + dmi.size - 1);
	dmi_data.set_read_latency(SC_ZERO_TIME);
	dmi_data.set_write_latency(SC_ZERO_TIME);
	return true;
}

void tlmu_sc::to_tlmu_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *be = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();
	int is_ram;
	int rw;

	if (be != NULL) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}
	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	rw = cmd == tlm::TLM_WRITE_COMMAND;

	is_ram = tlmu_bus_access(&q, rw, addr, data, len);
	if (is_ram) {
		trans.set_dmi_allowed(true);
	}
}

unsigned int tlmu_sc::to_tlmu_transport_dbg(tlm::tlm_generic_payload& trans)
{
	return 0;
}

/* Interrupt transport from SystemC into TLMu.  */
void tlmu_sc::irq_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *be = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();
	struct tlmu_irq qirq;

	if (be != NULL) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}
	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	memcpy(&qirq.data, data, 4);
	qirq.addr = addr;
	tlmu_notify_event(&q, TLMU_TLM_EVENT_IRQ, &qirq);
}

void tlmu_sc::map_ram(const char *name, uint64_t base, uint64_t size, int rw)
{
	tlmu_map_ram(&q, name, base, size, rw);
}

unsigned int tlmu_sc::irq_transport_dbg(tlm::tlm_generic_payload& trans)
{
	return 0;
}

void tlmu_sc::sync(int64_t time_ns)
{
	sync_time(time_ns);
}

void tlmu_sc::wake(void)
{
	tlmu_notify_event(&q, TLMU_TLM_EVENT_WAKE, NULL);
}

void tlmu_sc::sleep(void)
{
	tlmu_notify_event(&q, TLMU_TLM_EVENT_SLEEP, NULL);
}

void tlmu_sc::append_arg(const char *newarg)
{
	tlmu_append_arg(&q, newarg);
}

void tlmu_sc::process(void)
{
	tlmu_append_arg(&q, "-cpu");
	tlmu_append_arg(&q, cpu_model);

	tlmu_append_arg(&q, "-clock");
	tlmu_append_arg(&q, "tlm");

	/* Select the machine.  */
	tlmu_append_arg(&q, "-M");
	if (mach_name) {
		tlmu_append_arg(&q, mach_name);
	} else {
		tlmu_append_arg(&q, "tlm-mach");
	}

	if (elf_filename) {
		tlmu_append_arg(&q, "-kernel");
		tlmu_append_arg(&q, elf_filename);
	}

	/* Insn count driven time.  */
	if (1) {
		tlmu_append_arg(&q, "-icount");
		tlmu_append_arg(&q, "1");
	}

	/* Debug.  */
       	if (tracing) {
		tlmu_append_arg(&q, "-d");
		tlmu_append_arg(&q, "in_asm,exec,cpu");
	}

	/* Gdb stub.  */
	if (gdb_conn) {
		tlmu_append_arg(&q, "-gdb");
		tlmu_append_arg(&q, gdb_conn);
	}

	tlmu_run(&q);
}
