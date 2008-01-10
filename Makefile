EV_FLAGS = -I/opt/libev-2.01/include -L/opt/libev-2.01/lib -lev

compile: test

test_server: ev_tcp_socket.c ev_tcp_socket.h
	gcc ev_tcp_socket.c -g $(EV_FLAGS) `pkg-config --cflags --libs glib-2.0` -o test_server

#% : %.c
#  $(GHC) $(HFLAGS) $< $(LIBS) -o $@

test: clean test_server
	./test_server

clean:
	rm -f *.o ev_tcp_socket_test
