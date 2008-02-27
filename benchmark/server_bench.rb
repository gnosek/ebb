require File.dirname(__FILE__) + "/server_test
"
# supported servers: mongrel, emongrel, ebb, thin
# use another name an a already open port for anything else
usage = "e.g. server_bench response_size ebb:4001 mongrel:4002 other:4003"

bench_name = ARGV.shift

servers = []
ARGV.each do |server|
  name, port = server.split(':')
  servers << ServerTest.new(name, port)
end

