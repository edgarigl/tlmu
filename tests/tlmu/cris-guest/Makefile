BASEDIR=../../..
-include $(BASEDIR)/config-host.mak
VPATH=$(SRC_PATH)/tests/tlmu/cris-guest

CROSS  = cris-axis-elf-

CC      = $(CROSS)gcc
CXX     = $(CROSS)g++
AS      = $(CROSS)gcc -x assembler-with-cpp -c
LD      = $(CC)
SIZE    = $(CROSS)size

CFLAGS  = -Wall -g -O2

LDFLAGS = -Wl,-Ttext,0x18000000
LDFLAGS += -Wl,-Tdata,0x19000000
LDFLAGS += -nostartfiles
LDLIBS  += -nostdlib

OBJS = entry.o guest.o
TARGET = guest

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(CRT) $(LDLIBS)

clean:
	$(RM) $(TARGET) $(OBJS)

