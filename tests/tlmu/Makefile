-include .config.mk

SYSTEMC ?= /home/edgari/local/opt/systemc
SYSTEMC_INCLUDE ?=$(SYSTEMC)/include/
SYSTEMC_LIBDIR ?= $(SYSTEMC)/lib-linux64
# In case your TLM-2.0 installation is not bundled with
# with the SystemC one.
# TLM2 ?= /opt/systemc/TLM-2009-07-15

CXXFLAGS += -O2 -g

ifneq "$(SYSTEMC_INCLUDE)" ""
CPPFLAGS += -I $(SYSTEMC_INCLUDE)
endif
ifneq "$(TLM2)" ""
CPPFLAGS += -I $(TLM2)/include/tlm
endif

LDFLAGS = -L $(SYSTEMC_LIBDIR)
LDLIBS   += -lsystemc

SC_DEMO_TOP_C = sc_demo.cc
SC_DEMO_TOP_O = $(SC_DEMO_TOP_C:.cc=.o)

SC_DEMO_OBJS += $(SC_DEMO_TOP_O)

C_OBJS += safeio.o
SC_OBJS += memory.o
SC_OBJS += debugdev.o
SC_OBJS += demo-dma.o
C_OBJS += remote-port-proto.o
C_OBJS += remote-port-sk.o
SC_OBJS += remote-port-tlm.o

OBJS = $(C_OBJS) $(SC_OBJS)

VENV=SYSTEMC_INCLUDE=$(SYSTEMC_INCLUDE) SYSTEMC_LIBDIR=$(SYSTEMC_LIBDIR)
VOBJ_DIR=obj_dir
VFILES=apb_timer.v
VHDFILES=apb_dummy.vhd

SC_DEMO_OBJS += $(OBJS)
ZYNQ_OBJS += $(OBJS)
RONALDO_OBJS += $(OBJS)

TARGET_SC_DEMO = sc_demo

TARGETS = $(TARGET_SC_DEMO)

all: $(TARGETS)

-include $(SC_DEMO_OBJS:.o=.d)
CFLAGS += -MMD
CXXFLAGS += -MMD

$(TARGET_SC_DEMO): $(SC_DEMO_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(OBJS) $(OBJS:.o=.d) $(TARGETS)
	$(RM) -r $(VOBJ_DIR) $(CSRC_DIR) *.daidir
