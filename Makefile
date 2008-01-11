SHELL = /bin/sh

CC = gcc
CFLAGS = `pkg-config --cflags glib-2.0` -I/opt/libev-2.01/include -L/opt/libev-2.01/lib

ALL_CFLAGS = -g -Wall -O6 $(CFLAGS)

OBJS = tcp_server.o tcp_server_test.o drum.o
LIBS = -lev `pkg-config --libs glib-2.0`
EXE = test_server

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(EXE) mongrel_parser

$(EXE): $(OBJS) Makefile parser.o
	$(CC) $(ALL_CFLAGS) $(OBJS) mongrel/parser.o -o $(EXE) $(LIBS)

parser.o: mongrel_parser

mongrel_parser:
	$(MAKE) -C ./mongrel

test: test_server test.rb
	ruby test.rb

.PHONY : clean
clean:
	rm -f $(OBJS) $(EXE)
	$(MAKE) -C ./mongrel clean
