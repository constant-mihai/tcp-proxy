CC=gcc
CFLAGS=-I. -lpthread -Wall -Wextra -fno-diagnostics-show-caret -Wcast-align -Werror=return-type -g -O0 -Ddebug #-fPIC
DEPS = ../c_exercises/data-structures-and-algorithms/list/list.h \
	   workerpool.h \
	   tcp_server.h \
	   tun.h \
	   route.h \
	   tcp_server_test.h \
	   tun_test.h

OBJ = workerpool.o \
	  tcp_server.o \
	  tun.o \
	  route.o \
	  tcp_server_test.o \
	  tun_test.o \
	  main.o

# this is needed if modules are imported by relative path
LDFLAGS += -Wl,-rpath=../c_exercises/data-structures-and-algorithms

LIBS = -L../c_exercises/data-structures-and-algorithms/ -ldsaa

INCS = -I../c_exercises/data-structures-and-algorithms/

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(INCS)

# This took a few hours to debug, so I will leave a comment here for the next time:
# the linked libraries need to be at the end
# https://stackoverflow.com/questions/45135/why-does-the-order-in-which-libraries-are-linked-sometimes-cause-errors-in-gcc
# https://www.cprogramming.com/tutorial/shared-libraries-linux-gcc.html
tcp-proxy: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: shared 
shared: $(OBJ)
	$(CC) -shared -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o
