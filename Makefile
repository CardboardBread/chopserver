PORT=15342
CFLAGS = -DPORT=$(PORT) -g -Wall -Werror -std=gnu99

all: chopserver chopclient

chat_server: chopserver.o socket.o chophelper.o
	gcc ${CFLAGS} -o $@ $^

chat_client: chopclient.o socket.o chophelper.o
	gcc ${CFLAGS} -o $@ $^

%.o: %.c
	gcc ${CFLAGS} -c $<

clean:
	rm -f *.o chopserver chopclient
