CC=gcc
CFLAGS=-I. -lpthread -Wall -Wextra -fno-diagnostics-show-caret -Wcast-align -Werror=return-type -g -O0 -Ddebug #-fPIC
DEPS = list.h workerpool.h tcp_server.h
OBJ = list.c workerpool.c tcp_server.o main.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

tcp-proxy: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: shared 
shared: $(OBJ)
	$(CC) -shared -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
	rm udp
