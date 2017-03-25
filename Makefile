CFLAGS := -O3 -Wall -fPIC  -Wno-unused-function
LFLAGS := --shared

LIB = ldump.so

PLAT= $(shell uname)
ifeq ($(PLAT), Darwin)
 	LFLAGS += -undefined dynamic_lookup
endif

all : $(LIB)


$(LIB) : ldump.o
	$(CC) -o $@ $(LFLAGS) $^
	@rm -f ldump.o

ldump.o : ldump.c
	$(CC) -c $(CFLAGS) $<

clean : 
	rm -f ldump.o $(LIB)

.PHONY : clean

