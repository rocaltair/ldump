CFLAGS = -O3
LFLAGS = --shared -fPIC -L/usr/local/lib -llua

LIB = ldump.so

all : $(LIB)


$(LIB) : ldump.o
	$(CC) -o $@ $(LFLAGS) $^

ldump.o : ldump.c
	$(CC) -c $(CFLAGS) $<

clean : 
	rm -f ldump.o $(LIB)

.PHONY : clean

