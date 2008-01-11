CC = gcc
CFLAGS = 
ALL_CFLAGS = -g -Wall -O6 $(CFLAGS)
OBJS = parser.o
LIBS = 

%.o : %.c Makefile
	$(CC) $(ALL_CFLAGS) -c $< -o $@

all: $(OBJS)

parser.xml: parser.rl
	ragel parser.rl -o parser.xml

parser.c: parser.xml
	rlgen-cd -G2 -o parser.c parser.xml

.PHONY : clean clobber
clean:
	rm -f $(OBJS) parser.xml
clobber: clean
	rm -f parser.c
