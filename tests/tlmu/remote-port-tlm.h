/*
 * TLM remoteport glue
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

class remoteport_packet {
public:
	union {
		struct rp_pkt *pkt;
		uint8_t *u8;
	};
	size_t data_offset;
	size_t size;

	remoteport_packet(void);
	void alloc(size_t size);
};

class remoteport_tlm
: public sc_core::sc_module
{
public:
	tlm_utils::simple_initiator_socket_tagged<remoteport_tlm> *from_rp_sk[32];
	tlm_utils::simple_target_socket<remoteport_tlm> to_rp_sk;
	sc_out<bool> *from_rp_irqs;
	unsigned int from_rp_nr_irqs;

	sc_in<bool> *to_rp_irqs;
	unsigned int to_rp_nr_irqs;

	const char *sk_descr;

	sc_in<bool> rst;

	SC_HAS_PROCESS(remoteport_tlm);
        remoteport_tlm(sc_core::sc_module_name name,
			int fd,
			const char *sk_descr,
			unsigned int nr_init_sockets,
			unsigned int to_rp_nr_irqs,
			unsigned int from_rp_nr_irqs);

private:
	uint32_t rp_pkt_id;
	unsigned char *pktbuf_data;
	remoteport_packet pkt_rx;
	remoteport_packet pkt_tx;
	/* Socket.  */
	int fd;

	tlm_utils::tlm_quantumkeeper m_qk;

	int64_t rp_map_time(sc_time t);
	void account_time(int64_t rp_time_ns);

	ssize_t rp_read(void *rbuf, size_t count);
	ssize_t rp_write(const void *wbuf, size_t count);

	sc_time rp_bus_access(struct rp_pkt &pkt,
			   bool can_sync,
			   tlm::tlm_command cmd,
			   unsigned char *data, size_t size);

	void rp_cmd_read(struct rp_pkt &pkt, bool can_sync);
	void rp_cmd_write(struct rp_pkt &pkt, bool can_sync,
			unsigned char *data, size_t size);
	void rp_say_hello(void);
	void rp_cmd_hello(struct rp_pkt &pkt);
	void rp_cmd_sync(struct rp_pkt &pkt, bool can_sync);
	void rp_irq(void);
	bool rp_process(bool sync);
	void process(void);

	/* Part of the master interface into remote-port.  */
	virtual void to_rp_b_transport(tlm::tlm_generic_payload& trans,
					sc_time& delay);
};
