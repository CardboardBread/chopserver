PORT=15342
CFLAGS = -DPORT=$(PORT) -g -Wall -Werror

all: chopserver chopclient timeserver

chopserver: chopserver.o socket.o chophelper.o
	gcc ${CFLAGS} -o $@ $^

chopclient: chopclient.o socket.o chophelper.o
	gcc ${CFLAGS} -o $@ $^

timeserver: timeserver.o socket.o chophelper.o
	gcc ${CFLAGS} -o $@ $^

%.o: %.c
	gcc ${CFLAGS} -c $<

clean:
	rm -f *.o chopserver chopclient timeserver
