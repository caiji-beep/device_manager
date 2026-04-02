CROSS_COMPILE ?= arm-linux-gnueabihf-
CC := $(CROSS_COMPILE)gcc

CFLAGS := -Wall -O2
TARGET := serial_test
OBJS := main.o serial_port.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

main.o: main.c serial_port.h
	$(CC) $(CFLAGS) -c main.c -o $@

serial_port.o: serial_port.c serial_port.h
	$(CC) $(CFLAGS) -c serial_port.c -o $@

clean:
	rm -f $(TARGET) $(OBJS)