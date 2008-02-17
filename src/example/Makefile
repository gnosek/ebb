# Stand alone Makefile
# Not used when compiling from ruby binding

GLIB_CFLAGS = `pkg-config --cflags glib-2.0`
GLIB_LIBS   = `pkg-config --libs glib-2.0`

LIBEV_PREFIX = /opt/libev-2.01
LIBEV_CFLAGS = -I$(LIBEV_PREFIX)/include 
LIBEV_LIBS   = -L$(LIBEV_PREFIX)/lib -lev 

CC = gcc
CFLAGS = -g -Wall $(GLIB_CFLAGS) $(LIBEV_CFLAGS) # -DDEBUG
LIBS = $(LIBEV_LIBS) $(GLIB_LIBS) -lpthread
OBJS = ebb.o parser.o
EXE = ebb_test

%.o : %.c Makefile
	$(CC) $(CFLAGS) -c $< -o $@

all: $(OBJS) $(EXE)

$(EXE): $(OBJS) Makefile
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) $@.c -o $(EXE) 

parser.c: parser.rl
	ragel parser.rl | rlgen-cd -G2 -o parser.c

.PHONY : clean
clean:
	rm -f $(OBJS) $(EXE) parser.c .ruby_generated_makefile