
test: ev_tcp_socket_test
	./ev_tcp_socket_test

compile: ev_tcp_socket_test

ev_tcp_socket_test: ev_tcp_socket.c
	gcc ev_tcp_socket.c `pkg-config --cflags --libs glib-2.0` -o $@

#% : %.c
#  $(GHC) $(HFLAGS) $< $(LIBS) -o $@

clean:
	rm -f *.o ev_tcp_socket_test