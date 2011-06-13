BASEDIR=../..
-include $(BASEDIR)/config-host.mak
VPATH=$(SRC_PATH)/tests/tlmu

TLMU=$(CURDIR)

CPPFLAGS += -I $(SRC_PATH)/tests/tlmu
CPPFLAGS += -I $(TLMU)/include/tlmu
CFLAGS   += -Wall -O2 -g
LDFLAGS  += -L $(TLMU)/lib
LDLIBS   += -ltlmu -pthread -ldl -lrt

install-tlmu:
	$(MAKE) -C $(BASEDIR) install-tlmu DESTDIR=$(CURDIR)

C_EXAMPLE_OBJS += c_example.o

all: c_example

sc-all: c_example sc_example

c_example: $(C_EXAMPLE_OBJS)

.PHONY: sc_example
sc_example:
	$(MAKE) -C sc_example

run:
	LD_LIBRARY_PATH=./lib ./c_example

run-sc-all: run
	LD_LIBRARY_PATH=./lib ./sc_example/sc_example

clean:
	$(MAKE) -C sc_example clean
	$(RM) $(C_EXAMPLE_OBJS) c_example

