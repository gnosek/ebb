compile: tcp_socket_test

tcp_socket_test: tcp_socket.c
	gcc tcp_socket.c -lsocket `pkg-config --cflags --libs glib-2.0` -o $@

#% : %.c
#  $(GHC) $(HFLAGS) $< $(LIBS) -o $@

clean:
	rm -f *.o tcp_socket_test