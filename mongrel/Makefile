CC = gcc
CFLAGS = 
ALL_CFLAGS = -g -Wall -O6 $(CFLAGS)
OBJS = parser.o
LIBS = 

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(OBJS)

parser.c: parser.rl common.rl
	ragel parser.rl | rlgen-cd -G2 -o parser.c 

.PHONY : clean clobber
clean:
	rm -f $(OBJS)
clobber: clean
	rm -f parser.c
