CC=gcc
RM=rm -vrf
CFLAGS=-std=gnu11 -DIIO_UNIT_TEST -DTX_UNIT_TEST -DRX_UNIT_TEST -DADIDMA_NOIRQ -pedantic -Wall -O3 -pthread -D_POSIX_SOURCE
OBJS=src/adidma.o\
src/libuio.o\
drivers/gpiodev/gpiodev.o
TXOBJS=src/txmodem.o
RXOBJS=src/rxmodem.o
UIOOBJS=src/uiotest.o
IIOOBJS=src/libiio.o

tx: $(OBJS) $(TXOBJS)
	$(CC) $(CFLAGS) -o tx.out $(OBJS) $(TXOBJS) -lpthread -lm

rx: $(OBJS) $(RXOBJS)
	$(CC) $(CFLAGS) -o rx.out $(OBJS) $(RXOBJS) -lpthread -lm

uio: $(OBJS) $(UIOOBJS)
	$(CC) $(CFLAGS) -o uio.out $(OBJS) $(UIOOBJS) -lpthread -lm

iio: $(IIOOBJS)
	$(CC) $(CFLAGS) -o iio.out $(IIOOBJS) -lpthread -lm -liio

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude/ -Idrivers/ -c -o $@ $<

clean:
	$(RM) $(OBJS) $(RXOBJS) $(TXOBJS) $(UIOOBJS) $(IIOOBJS)
	$(RM) tx.out rx.out uio.out iio.out
