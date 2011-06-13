BASEDIR=../../..
-include $(BASEDIR)/config-host.mak
VPATH=$(SRC_PATH)/tests/tlmu/sc_example/

TLMU=$(CURDIR)/..
# Edit these to match your install paths
SYSTEMC = /opt/systemc
TLM2 = /opt/systemc/TLM-2009-07-15

CXXFLAGS += -O2 -g
CPPFLAGS += -I $(SRC_PATH)/tests/tlmu
CPPFLAGS += -I $(SYSTEMC)/include/
CPPFLAGS += -I $(TLM2)/include/tlm
CPPFLAGS += -I $(TLMU)/include/tlmu

LDFLAGS = -L $(SYSTEMC)/lib-linux64
LDFLAGS += -L $(TLMU)/lib

LDLIBS   += -ltlmu -lsystemc -pthread -lrt -ldl

SC_EXAMPLE_OBJS += sc_example.o
SC_EXAMPLE_OBJS += tlmu_sc.o
SC_EXAMPLE_OBJS += memory.o
SC_EXAMPLE_OBJS += magicdev.o

all: sc_example

sc_example: $(SC_EXAMPLE_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	$(RM) $(SC_EXAMPLE_OBJS) sc_example

