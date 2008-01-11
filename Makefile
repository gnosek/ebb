SHELL = /bin/sh

CC = gcc
CFLAGS = `pkg-config --cflags glib-2.0` -I/opt/libev-2.01/include -L/opt/libev-2.01/lib

ALL_CFLAGS = -g -Wall -O6 $(CFLAGS)

OBJS = evtcp_server.o evtcp_server_test.o
LIBS = -lev `pkg-config --libs glib-2.0`
EXE = test_server

# pattern rule to compile object files from C files
# might not work with make programs other than GNU make
%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(EXE)

$(EXE): $(OBJS) Makefile
	$(CC) $(ALL_CFLAGS) $(OBJS) -o $(EXE) $(LIBS)

test: test_server test.rb
	ruby test.rb

.PHONY : clean
clean:
	rm -f $(OBJS) $(EXE)
