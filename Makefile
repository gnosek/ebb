SHELL = /bin/sh

CC = gcc
CFLAGS = `pkg-config --cflags glib-2.0` -I/opt/libev-2.01/include -L/opt/libev-2.01/lib

ALL_CFLAGS = -g -Wall -O6 $(CFLAGS)

OBJS = tcp_server.o drum.o
LIBS = -lev `pkg-config --libs glib-2.0`
#EXE = test_server drum_test
TESTS = tcp_server_test drum_test

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(TESTS) $(OBJS) mongrel_parser

%_test : $(OBJS) Makefile parser.o
	$(CC) $(ALL_CFLAGS) $(OBJS) mongrel/parser.o $@.c -o $@ $(LIBS)

parser.o: mongrel_parser

mongrel_parser:
	$(MAKE) -C ./mongrel

test: test_server test.rb
	ruby test.rb

.PHONY : clean
clean:
	rm -f $(OBJS) $(TESTS)
	$(MAKE) -C ./mongrel clean
