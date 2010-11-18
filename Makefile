CC=gcc
FUSE_CFLAGS=-D_FILE_OFFSET_BITS=64
CFLAGS=-Wall -pedantic -std=gnu99 -DDEBUG $(FUSE_CFLAGS)
EXEC=teleinfuse
LDFLAGS=-pthread -lfuse

all: $(EXEC)

teleinfuse: teleinfuse.o teleinfo.o
	$(CC) -o $@ $^ $(LDFLAGS)

teleinfuse.o: teleinfo.h

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f *.o

mrproper: clean
	rm -f $(EXEC) *~
