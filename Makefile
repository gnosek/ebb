####### Edit me ########################################

GLIB_CFLAGS = `pkg-config --cflags glib-2.0`
GLIB_LIBS   = `pkg-config --libs glib-2.0`

LIBEV_PREFIX = /opt/libev-2.01
LIBEV_CFLAGS = -I$(LIBEV_PREFIX)/include 
LIBEV_LIBS   = -L$(LIBEV_PREFIX)/lib -lev 

########################################################

CC = gcc
CFLAGS = $(GLIB_CFLAGS) $(LIBEV_CFLAGS)
LIBS = $(LIBEV_LIBS) $(GLIB_LIBS)

ALL_CFLAGS = -g -Wall $(CFLAGS) # -DDEBUG

all: ebb.o mongrel_parser ebb_test ruby_binding

ebb.o : ebb.c Makefile parser_callbacks.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

ebb_test : ebb.o Makefile parser.o
	$(CC) $(ALL_CFLAGS) ebb.o mongrel/parser.o $@.c -o $@ $(LIBS)

parser.o: mongrel_parser

mongrel_parser:
	make -C ./mongrel

ruby_binding: ebb.o parser.o
	make -C ./ruby_binding clean
	make -C ./ruby_binding

.PHONY : clean
clean:
	rm -f ebb.o ebb_test
	make -C ./mongrel clean
	make -C ./ruby_binding clean