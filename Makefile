CFLAGS = -O3
LFLAGS = --shared -fPIC -llua

LIB = ldump.so

all : $(LIB)


$(LIB) : ldump.o rbuf.o
	$(CC) -o $@ $(LFLAGS) $^

ldump.o : ldump.c
	$(CC) -c $(CFLAGS) $<

rbuf.o : rbuf.c rbuf.h
	$(CC) -c $(CFLAGS) $<

clean : 
	rm -f ldump.o rbuf.o $(LIB)

.PHONY : clean

