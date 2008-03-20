#!/bin/sh
/opt/lighttpd-1.4.19/sbin/lighttpd -v
/opt/lighttpd-1.4.19/sbin/lighttpd -D -f ./lighttpd.conf &> /dev/null
