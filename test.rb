require 'test/unit'
require 'rubygems'
require 'ruby-debug'
Debugger.start

class ServerTest < Test::Unit::TestCase
  def test_server
    system %q{./tcp_test > /dev/null &}
    sleep 0.5
    TCPSocket.open('localhost', 1337) do |socket|
      ["Hello", "World", "foo bar"].each do |w|
        socket.write w
        assert_equal w.reverse, socket.read(w.length)
      end
    end
  ensure
    system %q{killall -9 tcp_test}
  end
end
