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

ALL_CFLAGS = -g -Wall $(CFLAGS)

OBJS = ebb.o
TESTS = ebb_test

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(TESTS) $(OBJS) mongrel_parser

%_test : $(OBJS) Makefile parser.o
	$(CC) $(ALL_CFLAGS) $(OBJS) mongrel/parser.o $@.c -o $@ $(LIBS)

parser.o: mongrel_parser

mongrel_parser:
	make -C ./mongrel

test: test_server test.rb
	ruby test.rb

.PHONY : clean
clean:
	rm -f $(OBJS) $(TESTS)
	$(MAKE) -C ./mongrel clean