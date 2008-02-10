require 'ebb'
require 'test/unit'
require 'net/http'

class SimpleApp
  @@responses = {}
  @@count = 0
  def call(env)
    command = env['PATH_INFO'].split('/').last
    if command == "test_post_length"
      input_body = ""
      while chunk = env['rack.input'].read(10)
        input_body << chunk 
      end
      if env['HTTP_CONTENT_LENGTH'].to_i == input_body.length
        body = "Content-Length matches input length"
        status = 200
      else
        body = "Content-Length doesn't matches input length! 
          content_length = #{env['HTTP_CONTENT_LENGTH'].to_i}
          input_body.length = #{input_body.length}"
        status = 500
      end
    elsif command =~ /periodically_slow_(\d+)$/
      if @@count % 10 == 0
        seconds = $1.to_i
        sleep seconds
      else
        seconds = 0
      end
      @@count += 1
      body = "waited #{seconds} seconds"
    elsif command =~ /slow_(\d+)$/
      seconds = $1.to_i
      sleep seconds
      status = 200
      body = "waited #{seconds} seconds"
    elsif command.to_i > 0
      size = command.to_i
      @@responses[size] ||= "C" * size
      body = @@responses[size]
      status = 200
    else
      status = 404
      body = env.inspect
    end
    [status, {'Content-Type' => 'text/plain'}, body + "\r\n\r\n"]
  end
end

class App1
  def call(e)
    [200, {'Content-Type' => 'text/plain'},"#{e.inspect}\r\n"]
  end
end

server = Ebb::Server.new(SimpleApp.new, {:Port => 4001})
server.start


# class EbbTest < Test::Unit::TestCase
#   
#   def get(path)
#     Net::HTTP.get_response(URI.parse("http://0.0.0.0:4001#{path}"))
#   end
#   
#   def test_get
#     response = get('/hello')
#     eval response.body
#   end
# end

