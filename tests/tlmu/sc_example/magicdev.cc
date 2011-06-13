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

#define SC_INCLUDE_DYNAMIC_PROCESSES

#include <inttypes.h>

#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"

using namespace sc_core;
using namespace std;

#include "magicdev.h"
#include <sys/types.h>

magicdev::magicdev(sc_module_name name)
	: sc_module(name), socket("socket")
{
	socket.register_b_transport(this, &magicdev::b_transport);
	socket.register_transport_dbg(this, &magicdev::transport_dbg);
}

void magicdev::b_transport(tlm::tlm_generic_payload& trans, sc_time& delay)
{
	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64    addr = trans.get_address();
	unsigned char*   data = trans.get_data_ptr();
	unsigned int     len = trans.get_data_length();
	unsigned char*   byt = trans.get_byte_enable_ptr();
	unsigned int     wid = trans.get_streaming_width();

	if (byt != 0) {
		trans.set_response_status(tlm::TLM_BYTE_ENABLE_ERROR_RESPONSE);
		return;
	}

	if (len > 4 || wid < len) {
		trans.set_response_status(tlm::TLM_BURST_ERROR_RESPONSE);
		return;
	}

	if (trans.get_command() == tlm::TLM_READ_COMMAND) {
	} else if (cmd == tlm::TLM_WRITE_COMMAND) {
		switch (addr) {
			case 0:
				cout << "TRACE: " << " "
				     << hex << * (uint32_t *) data
				     << " " << sc_time_stamp() << "\n";
				break;
			case 0x4:
				putchar(* (uint32_t *) data);
				break;
			case 0x8:
				cout << "STOP: " << " "
				     << hex << * (uint32_t *) data
				     << " " << sc_time_stamp() << "\n";
				sc_stop();
				exit(1);
				break;
		}
	}

	// Pretend this is slow!
	delay += sc_time(1, SC_US);
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

unsigned int magicdev::transport_dbg(tlm::tlm_generic_payload& trans)
{
	unsigned int     len = trans.get_data_length();
	return len;
}
