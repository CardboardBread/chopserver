FLAGS = -Wall -std=gnu99 -ggdb
.PHONY: all clean

all: chopserver chopclient

chopserver: chopserver.o chopconn.o chopconst.o chopdata.o chopdebug.o choppacket.o chopsocket.o chopsend.o hashtable.o
	gcc ${FLAGS} -o $@ $^

chopclient: chopserver.o chopconn.o chopconst.o chopdata.o chopdebug.o choppacket.o chopsocket.o chopsend.o hashtable.o
	gcc ${FLAGS} -o $@ $^

%.o : src/%.c
	gcc ${FLAGS} -c $<

%.o : include/%.c
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o *.gch chopserver chopclient