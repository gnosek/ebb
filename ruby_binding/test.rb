require 'ebb'
require 'rubygems'
require 'ruby-debug'
require 'camping'
require 'rack'
Debugger.start


Camping.goes :CampApp
module CampApp
  module Controllers
    class Index < R '/','/(\d+)'
      def get(size=1)
        @headers["Content-Type"] = 'text/plain'
        size = size.to_i
        raise "size is #{size}" if size <= 0
        "C" * size + "\r\n\r\n"
      end
    end
  end
end

class SimpleApp
  def call(env)
    [200, {:content_type => 'text/plain'}, env.inspect + "\r\n\r\n"]
  end
end

app = Rack::Adapter::Camping.new(CampApp)
#app = SimpleApp.new

server = Ebb::Server.new(app)
server.start
