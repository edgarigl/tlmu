/*
 * System-C TLM-2.0 remoteport glue
 *
 * Copyright (c) 2013 Xilinx Inc
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
#include <sys/utsname.h>

#include "systemc.h"
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/tlm_quantumkeeper.h"
#include <iostream>

extern "C" {
#include "safeio.h"
#include "remote-port-proto.h"
#include "remote-port-sk.h"
};
#include "remote-port-tlm.h"

using namespace sc_core;
using namespace std;

remoteport_packet::remoteport_packet(void)
{
	u8 = NULL;
	size = 0;
	alloc(sizeof *pkt);
}

void remoteport_packet::alloc(size_t new_size)
{
	if (size < new_size) {
                uint8_t *old = u8;
		u8 = (uint8_t *) realloc(u8, new_size);
		if (u8 == NULL) {
			cerr << "out of mem" << endl;
			exit(EXIT_FAILURE);
		}
		size = new_size;
	}
}

remoteport_tlm::remoteport_tlm(sc_module_name name,
			int fd,
			const char *sk_descr,
			unsigned int nr_init_sockets,
			unsigned int to_rp_nr_irqs,
			unsigned int from_rp_nr_irqs)
	: sc_module(name),
	  to_rp_sk("toRemotePortSocket"),
	  rst("rst")
{
	int i;
	this->fd = fd;
	this->sk_descr = sk_descr;
	this->rp_pkt_id = 0;

	for (i = 0; i < nr_init_sockets; i++) {
		char txt[64];
		sprintf(txt, "init_socket_%d", i);
		from_rp_sk[i] = new tlm_utils::simple_initiator_socket_tagged<remoteport_tlm>(txt);
	}

	to_rp_sk.register_b_transport(this, &remoteport_tlm::to_rp_b_transport);

	SC_THREAD(process);

	if (to_rp_nr_irqs > 0) {
		to_rp_irqs = new sc_in<bool>[to_rp_nr_irqs];
		this->to_rp_nr_irqs = to_rp_nr_irqs;
		SC_METHOD(rp_irq);
		dont_initialize();
		for (i = 0; i < to_rp_nr_irqs; i++) {
			sensitive << to_rp_irqs[i];
		}
	}
}

/* Convert an sc_time into int64 nanoseconds trying to avoid rounding errors.
   Why make this easy when you don't have to?  */
int64_t remoteport_tlm::rp_map_time(sc_time t)
{
	sc_time tr, tmp;
	double dtr;

	tr = sc_get_time_resolution();
	dtr = tr.to_seconds() * 1000 * 1000 * 1000;

	tmp = t * dtr;
	return tmp.value();
}

ssize_t remoteport_tlm::rp_read(void *rbuf, size_t count)
{
	ssize_t r;

	r = safe_read(fd, rbuf, count);
	if (r < count) {
		if (r < 0)
			perror(__func__);
		exit(EXIT_FAILURE);
	}
}

ssize_t remoteport_tlm::rp_write(const void *wbuf, size_t count)
{
	ssize_t r;

	r = safe_write(fd, wbuf, count);
	if (r < count) {
		if (r < 0)
			perror(__func__);
		exit(EXIT_FAILURE);
	}
}

void remoteport_tlm::rp_cmd_hello(struct rp_pkt &pkt)
{
	if (pkt.hello.version.major != RP_VERSION_MAJOR) {
		cerr << "RP Version missmatch"
			<< " remote=" << pkt.hello.version.major
			<< "." << pkt.hello.version.minor
			<< " local=" << RP_VERSION_MAJOR
			<< "." << RP_VERSION_MINOR
			<< endl;
		exit(EXIT_FAILURE);
	}
}

void remoteport_tlm::account_time(int64_t rclk)
{
	int64_t lclk;
	int64_t delta_ns;
	sc_time delta;

	lclk = rp_map_time(m_qk.get_current_time());
	if (lclk >= rclk)
		return;

	delta_ns = rclk - lclk;

	assert(delta_ns >= 0);
	delta = sc_time((double) delta_ns, SC_NS);
	assert(delta >= SC_ZERO_TIME);

#if 0
	cout << "account rclk=" << rclk << " current=" << m_qk.get_current_time() << " delta=" << delta << endl;
#endif
	m_qk.inc(delta);
	return;
}

sc_time remoteport_tlm::rp_bus_access(struct rp_pkt &pkt,
				   bool can_sync,
				   tlm::tlm_command cmd,
				   unsigned char *data, size_t len)
{
	tlm::tlm_generic_payload tr;
	sc_time delay;

	account_time(pkt.sync.timestamp);
	if (can_sync && m_qk.need_sync()) {
		m_qk.sync();
//		cout << "bus sync done " << m_qk.get_current_time() << endl;
	}

	delay = m_qk.get_local_time();
	assert(pkt.busaccess.width == 0);

	tr.set_command(cmd);
	tr.set_address(pkt.busaccess.addr);
	tr.set_data_ptr(data);
	tr.set_data_length(len);
	tr.set_streaming_width(pkt.busaccess.stream_width);
	tr.set_dmi_allowed(false);
	tr.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);

	from_rp_sk[pkt.hdr.dev - 1][0]->b_transport(tr, delay);
	if (tr.get_response_status() != tlm::TLM_OK_RESPONSE) {
		/* Handle errors.  */
		printf("bus error\n");
	}

	if (pkt.busaccess.attributes & RP_BUS_ATTR_EOP) {
		tr.set_data_length(0);
		from_rp_sk[pkt.hdr.dev - 1][0]->b_transport(tr, delay);
	}

	m_qk.set(delay);
	return delay;
}

void remoteport_tlm::rp_cmd_read(struct rp_pkt &pkt, bool can_sync)
{
	size_t plen;
	int64_t clk;
	sc_time delay;
	struct rp_pkt lpkt = pkt;

	/* FIXME: We the callee is allowed to yield, and may call
		us back out again (loop). So we should be reentrant
		in respect to pkt_tx.  */
	pkt_tx.alloc(sizeof lpkt.busaccess + lpkt.busaccess.len);
	delay = rp_bus_access(lpkt, can_sync, tlm::TLM_READ_COMMAND,
		(unsigned char *) (&pkt_tx.pkt->busaccess + 1),
		lpkt.busaccess.len);

	clk = rp_map_time(delay);
	clk += lpkt.busaccess.timestamp;
	assert(clk >= lpkt.busaccess.timestamp);

	plen = rp_encode_read_resp(lpkt.hdr.id, lpkt.hdr.dev,
				  &pkt_tx.pkt->busaccess,
				  clk,
				  lpkt.busaccess.addr,
				  lpkt.busaccess.attributes,
				  lpkt.busaccess.len,
				  lpkt.busaccess.width,
				  lpkt.busaccess.stream_width);
	rp_write(pkt_tx.pkt, plen);
}

void remoteport_tlm::rp_cmd_write(struct rp_pkt &pkt, bool can_sync,
				  unsigned char *data, size_t len)
{
	size_t plen;
	int64_t clk;
	sc_time delay;
	struct rp_pkt lpkt = pkt;

	delay = rp_bus_access(lpkt, can_sync,
				tlm::TLM_WRITE_COMMAND, data, len);

	clk = rp_map_time(delay);
	clk += lpkt.busaccess.timestamp;
	assert(clk >= lpkt.busaccess.timestamp);
	plen = rp_encode_write_resp(lpkt.hdr.id, lpkt.hdr.dev,
				    &pkt_tx.pkt->busaccess,
				    clk,
				    lpkt.busaccess.addr,
				    lpkt.busaccess.attributes,
				    lpkt.busaccess.len,
				    lpkt.busaccess.width,
				    lpkt.busaccess.stream_width);
	rp_write(pkt_tx.pkt, plen);
}

void remoteport_tlm::rp_say_hello(void)
{
	struct rp_pkt_hello pkt;
	size_t len;

	len = rp_encode_hello(rp_pkt_id++, 0,
				&pkt, RP_VERSION_MAJOR, RP_VERSION_MINOR);
	rp_write(&pkt, len);
}

void remoteport_tlm::rp_cmd_sync(struct rp_pkt &pkt, bool can_sync)
{
	size_t plen;
        int64_t clk;

	account_time(pkt.sync.timestamp);

	clk = rp_map_time(m_qk.get_current_time());
        assert(clk >= pkt.sync.timestamp);
	plen = rp_encode_sync_resp(pkt.hdr.id,
				   pkt.hdr.dev, &pkt_tx.pkt->sync,
				   clk);
	rp_write(pkt_tx.pkt, plen);

	/* Relaxing this sync to run in parallel with the remote
	   speeds up simulation significantly but allows us to skew off
	   time (in theory). The added inaccuracy is not really observable
	   to any side of the simulation though.  */
	if (can_sync && m_qk.need_sync()) {
		m_qk.sync();
	}
//	cout << "sync done " << m_qk.get_current_time() << endl;

}

void remoteport_tlm::to_rp_b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay)
{
	size_t plen;
	int64_t clk;
	bool resp_ready;

	tlm::tlm_command cmd = trans.get_command();
	sc_dt::uint64 addr = trans.get_address();
	unsigned char *data = trans.get_data_ptr();
	unsigned int len = trans.get_data_length();
	unsigned char *be = trans.get_byte_enable_ptr();
	unsigned int wid = trans.get_streaming_width();

	pkt_tx.alloc(sizeof pkt_tx.pkt->busaccess + len);
	clk = rp_map_time(m_qk.get_current_time());
	if (cmd == tlm::TLM_READ_COMMAND) {
		plen = rp_encode_read(rp_pkt_id++, 0,
					&pkt_tx.pkt->busaccess,
					clk,
					addr, 0,
					len,
					0,
					wid);
		rp_write(pkt_tx.pkt, plen);
	} else {
		plen = rp_encode_write(rp_pkt_id++, 0,
					&pkt_tx.pkt->busaccess,
					clk,
					addr, 0,
					len,
					0,
					wid);
		rp_write(pkt_tx.pkt, plen);
		rp_write(data, len);
	}
	do {
		resp_ready = rp_process(false);
	} while (!resp_ready);

	if (cmd == tlm::TLM_READ_COMMAND) {
		memcpy(data, pkt_rx.u8 + pkt_rx.data_offset, len);
	}
	trans.set_response_status(tlm::TLM_OK_RESPONSE);
}

void remoteport_tlm::rp_irq(void)
{
	size_t plen;
	int64_t clk;
	int i;

	clk = rp_map_time(m_qk.get_current_time());
	for (i = 0; i < to_rp_nr_irqs; i++) {
		if (to_rp_irqs[i].event()) {
			bool val = to_rp_irqs[i].read();
			plen = rp_encode_interrupt(rp_pkt_id++, 2,
						   &pkt_tx.pkt->interrupt,
						   clk, i, 0, val);
			rp_write(pkt_tx.pkt, plen);
		}
	}
}

bool remoteport_tlm::rp_process(bool can_sync)
{
	ssize_t r;

	while (1) {
		unsigned char *data;
		uint32_t dlen;
		size_t datalen;

		r = rp_read(&pkt_rx.pkt->hdr, sizeof pkt_rx.pkt->hdr);

		rp_decode_hdr(pkt_rx.pkt);

		pkt_rx.alloc(sizeof pkt_rx.pkt->hdr + pkt_rx.pkt->hdr.len);
		r = rp_read(&pkt_rx.pkt->hdr + 1, pkt_rx.pkt->hdr.len);

		dlen = rp_decode_payload(pkt_rx.pkt);
		data = pkt_rx.u8 + sizeof pkt_rx.pkt->hdr + dlen;
		datalen = pkt_rx.pkt->hdr.len - dlen;
		if (pkt_rx.pkt->hdr.flags & RP_PKT_FLAGS_response) {
			pkt_rx.data_offset = sizeof pkt_rx.pkt->hdr + dlen;
			return true;
		}

//		printf("%s: cmd=%d dev=%d\n", __func__, pkt_rx.pkt->hdr.cmd, pkt_rx.pkt->hdr.dev);
		switch (pkt_rx.pkt->hdr.cmd) {
		case RP_CMD_hello:
			rp_cmd_hello(*pkt_rx.pkt);
			break;
		case RP_CMD_write:
			rp_cmd_write(*pkt_rx.pkt, can_sync, data, datalen);
			break;
		case RP_CMD_read:
			rp_cmd_read(*pkt_rx.pkt, can_sync);
			break;
		case RP_CMD_interrupt:
			break;
		case RP_CMD_sync:
                        rp_cmd_sync(*pkt_rx.pkt, can_sync);
			break;
		defaul:
			assert(0);
			break;
		}
		/* We've just processed peer packets and it is
		   likely running freely. Good spot for a local sync.  */
		if (can_sync) {
			m_qk.sync();
		}
	}
	return false;
}

void remoteport_tlm::process(void)
{
	if (fd == -1) {
		fd = sk_open(sk_descr);
		if (fd == -1) {
			if (sk_descr)
				perror(sk_descr);
			return;
		}
	}

	m_qk.reset();
	wait(rst.negedge_event());

	rp_say_hello();

	while (1) {
		rp_process(true);
	}
	return;
}
