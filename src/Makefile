CC=gcc
CFLAGS=-I. -lpthread -Wall -Wextra -fno-diagnostics-show-caret -Wcast-align -Werror=return-type -g -O0 -Ddebug #-fPIC
DEPS = ../c_exercises/data-structures-and-algorithms/list/list.h \
	   workerpool.h \
	   tcp_server.h
OBJ = workerpool.o tcp_server.o main.o

#LDFLAGS += -Wl,-rpath=../c_exercises/data-structures-and-algorithms/list# this is needed if modules are imported by relative path

LIBS = -L../c_exercises/data-structures-and-algorithms/ -ldsaa

INCS = -I../c_exercises/data-structures-and-algorithms/

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCS)

tcp-proxy: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: shared 
shared: $(OBJ)
	$(CC) -shared -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
