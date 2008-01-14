SHELL = /bin/sh

CC = gcc
CFLAGS = `pkg-config --cflags glib-2.0` -I/opt/libev-2.01/include -L/opt/libev-2.01/lib

ALL_CFLAGS = -g -Wall $(CFLAGS)

OBJS = tcp.o ebb.o
LIBS = -lev `pkg-config --libs glib-2.0` -lpthread
#EXE = test_server ebb_test
TESTS = tcp_test ebb_test

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(TESTS) $(OBJS) mongrel_parser ruby_binding

%_test : $(OBJS) Makefile parser.o
	$(CC) $(ALL_CFLAGS) $(OBJS) mongrel/parser.o $@.c -o $@ $(LIBS)

parser.o: mongrel_parser

mongrel_parser:
	$(MAKE) -C ./mongrel

ruby_binding:
	rake -f ruby_binding/Rakefile

test: test_server test.rb
	ruby test.rb

.PHONY : clean
clean:
	rm -f $(OBJS) $(TESTS)
	$(MAKE) -C ./mongrel clean
