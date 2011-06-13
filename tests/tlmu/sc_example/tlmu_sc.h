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

/* To Avoid warnings when declaring the funcion pointers accross C and C++.  */
#define TLMU_NO_DECLARE_CB_FUNC_PTR
extern "C" {
#include "tlmu.h"
};


#define TLMU_MHZ (1000 * 1000)
#define TLMU_GHZ (1000 * 1000 * 1000)

class tlmu_sc
: public sc_core::sc_module
{
public:
	enum {
		TRACING_OFF	= 0,
		TRACING_ON	= 1
	};

	tlm_utils::simple_initiator_socket<tlmu_sc> from_tlmu_sk;
	tlm_utils::simple_target_socket<tlmu_sc> to_tlmu_sk;
	tlm_utils::simple_target_socket<tlmu_sc> to_tlmu_irq_sk;

	SC_HAS_PROCESS(tlmu_sc);
        tlmu_sc(sc_core::sc_module_name name,
		 const char *soname,
		 const char *mach_name,
		 const char *cpu_model,
		 uint64_t freq_hz,
		 const char *elf_filename,
		 int tracing,
		 const char *gdb_conn,
		 int boot_state,
		 uint64_t sync_period_ns);

	void map_ram(const char *name, uint64_t base, uint64_t size, int rw);
	void set_image_load_params(uint64_t base, uint64_t size);
	void append_arg(const char *newarg);

	void wake(void);
	void sleep(void);

private:
	tlm_utils::tlm_quantumkeeper m_qk;
	/* Relative to 1Ghz.  */
	double speed_factor;
	const char *mach_name;
	const char *cpu_model;
	const char *elf_filename;
	int tracing;
	const char *gdb_conn;
	const char *bootsel;
	struct tlmu q;

	virtual void invalidate_direct_mem_ptr(sc_dt::uint64 start_range,
					sc_dt::uint64 end_range);

	virtual bool to_tlmu_get_direct_mem_ptr(tlm::tlm_generic_payload& trans,
					tlm::tlm_dmi& dmi_data);
	virtual void to_tlmu_b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay);
	virtual unsigned int to_tlmu_transport_dbg(tlm::tlm_generic_payload& trans);
	virtual void irq_b_transport(tlm::tlm_generic_payload& trans, sc_time& delay);
	virtual unsigned int irq_transport_dbg(tlm::tlm_generic_payload& trans);
	void process(void);
	void sync_time(int64_t tlmu_time_ns);
	void get_dmi_ptr(uint64_t addr, struct tlmu_dmi *dmi);
	int bus_access(int64_t clk, int rw,
				uint64_t addr, void *data, int len);
	void bus_access_dbg(int64_t clk, int rw,
			uint64_t addr, void *data, int len);
	void sync(int64_t time_ns);
	int64_t last_sync;
};

extern "C" {
#ifdef TLMU_NO_DECLARE_CB_FUNC_PTR

void tlmu_set_sync_cb(struct tlmu *q, void (tlmu_sc::*cb) (int64_t));
void tlmu_set_bus_access_cb(struct tlmu *q,
			int (tlmu_sc::*access)(int64_t clk, int rw,
				uint64_t addr, void *data, int len));
void tlmu_set_bus_access_dbg_cb(struct tlmu *q,
			void (tlmu_sc::*access_debug)(int64_t clk, int rw,
                                uint64_t addr, void *data, int len));
void tlmu_set_bus_get_dmi_ptr_cb(struct tlmu *q,
			void (tlmu_sc::*get_dmi_ptr)(uint64_t addr,
                                struct tlmu_dmi *dmi));
#endif
};


