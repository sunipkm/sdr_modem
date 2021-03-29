CC=gcc
RM=rm -vrf
CFLAGS=-std=gnu11 -DTXDEBUG -DTX_UNIT_TEST -DRXDEBUG -DRX_UNIT_TEST -DADIDMA_DEBUG -DADIDMA_NOIRQ -pedantic -Wall -O3 -pthread -D_POSIX_SOURCE
OBJS=src/adidma.o\
src/libuio.o\
drivers/gpiodev/gpiodev.o
TXOBJS=src/txmodem.o
RXOBJS=src/rxmodem.o
UIOOBJS=src/uiotest.o

tx: $(OBJS) $(TXOBJS)
	$(CC) $(CFLAGS) -o tx.out $(OBJS) $(TXOBJS) -lpthread -lm

rx: $(OBJS) $(RXOBJS)
	$(CC) $(CFLAGS) -o rx.out $(OBJS) $(RXOBJS) -lpthread -lm

uio: $(OBJS) $(UIOOBJS)
	$(CC) $(CFLAGS) -o uio.out $(OBJS) $(UIOOBJS) -lpthread -lm

%.o: %.c
	$(CC) $(CFLAGS) -Iinclude/ -Idrivers/ -c -o $@ $<

clean:
	$(RM) $(OBJS) $(RXOBJS) $(TXOBJS) $(UIOOBJS)
	$(RM) tx.out rx.out uio.out
