require 'test/unit'
require 'ebb'
require 'rubygems'
require 'ruby-debug'
Debugger.start

class SimpleApp
  def call(env)
    [200, {:Content_Type => 'text/plain'}, "Hello \r\n" * 500000]
  end
end

class EbbTest < Test::Unit::TestCase
  def test_init
    server = Ebb::Server.new(SimpleApp.new)
    server.start
    
    #puts %x{curl -Ni http://localhost:4001/blah}
  end
end
