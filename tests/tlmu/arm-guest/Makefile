BASEDIR=../../..
-include $(BASEDIR)/config-host.mak
VPATH=$(SRC_PATH)/tests/tlmu/arm-guest

CROSS  = arm-linux-

CC      = $(CROSS)gcc
CXX     = $(CROSS)g++
AS      = $(CROSS)gcc -x assembler-with-cpp -c
LD      = $(CC)
SIZE    = $(CROSS)size

CFLAGS  = -Wall -g -O2

LDFLAGS  = -Wl,-Ttext,0x18008000
LDFLAGS += -Wl,-Tdata,0x19008000
LDFLAGS += -nostartfiles
LDLIBS  += -nostdlib

OBJS = entry.o guest.o
TARGET = guest

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(CRT) $(LDLIBS)

clean:
	$(RM) $(TARGET) $(OBJS)

