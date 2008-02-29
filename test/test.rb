require  File.dirname(__FILE__) + '/../ruby_lib/ebb'
require 'test/unit'
require 'net/http'
require 'base64'


class EbbTest < Test::Unit::TestCase
  def setup
    @pid = fork do
      server = Ebb::Server.new(self, :port => 4044)
      STDOUT.reopen "/dev/null", "a"
      server.start
    end
    sleep 0.5
  end
  
  def teardown
    Process.kill('KILL', @pid)
    sleep 0.5
  end
  
  def get(path)
    Net::HTTP.get_response(URI.parse("http://0.0.0.0:4044#{path}"))
  end
  
  def post(path, data)
    Net::HTTP.post_form(URI.parse("http://0.0.0.0:4044#{path}"), data)
  end
  
  @@responses = {}
  def call(env)
    commands = env['PATH_INFO'].split('/')
    
    if commands.include?('bytes')
      n = commands.last.to_i
      raise "bytes called with n <= 0" if n <= 0
      body = @@responses[n] || "C"*n
      status = 200
    
    elsif commands.include?('env')
      env.delete('rack.input') # delete this because it's hard to marshal
      env.delete('rack.errors')
      body = Base64.encode64(Marshal.dump(env))
      status = 200
      
    elsif commands.include?('test_post_length')
      input_body = ""
      while chunk = env['rack.input'].read(512)
        input_body << chunk 
      end
      
      content_length_header = env['HTTP_CONTENT_LENGTH'].to_i
      
      if content_length_header == input_body.length
        body = "Content-Length matches input length"
        status = 200
      else
        body = "Content-Length header is #{content_length_header} but body length is #{input_body.length}"
        #  content_length = #{env['HTTP_CONTENT_LENGTH'].to_i}
        #  input_body.length = #{input_body.length}"
        status = 500
      end
    
    else
      status = 404
      body = "Undefined url"
    end
    
    [status, {'Content-Type' => 'text/plain'}, body]
  end
  
  def test_get_bytes
    [1,10,1000].each do |i|
      response = get("/bytes/#{i}")
      assert_equal "#{'C'*i.to_i}", response.body
    end
  end
  
  def test_get_unknown
    response = get('/blah')
    assert_equal "Undefined url", response.body
  end
  
  def test_small_posts
    [1,10,321,123,1000].each do |i|
      response = post("/test_post_length", 'C'*i)
      assert_equal 200, response.code.to_i, response.body
    end
  end
  
  # this is rough but does detect major problems
  def test_ab
    r = %x{ab -n 1000 -c 50 -q http://0.0.0.0:4044/bytes/123}
    assert r =~ /Requests per second:\s*(\d+)/, r
    assert $1.to_i > 100, r
  end
  
  def test_large_post
    [50,60,100].each do |i|
      response = post("/test_post_length", 'C'*1024*i)
      assert_equal 200, response.code.to_i, response.body
    end
  end
  
  def test_env
    response = get('/env')
    env = Marshal.load(Base64.decode64(response.body))
    assert_equal '/env', env['PATH_INFO']
    assert_equal '/env', env['REQUEST_PATH']
    assert_equal 'HTTP/1.1', env['SERVER_PROTOCOL']
    assert_equal 'CGI/1.2', env['GATEWAY_INTERFACE']
    assert_equal '0.0.0.0', env['SERVER_NAME']
    assert_equal '4044', env['SERVER_PORT']
    assert_equal 'GET', env['REQUEST_METHOD']
  end
end

class EbbRailsTest < Test::Unit::TestCase
  # just to make sure there isn't some load error
  def test_ebb_rails_version
    out = %x{ruby #{Ebb::LIBDIR}/../bin/ebb_rails -v}
    assert_match %r{Ebb #{Ebb::VERSION}}, out
  end

  #   def get(path)
  #     Net::HTTP.get_response(URI.parse("http://0.0.0.0:4043#{path}"))
  #   end
  # 
  #   def test_starting_with_many_options
  #     %x{cd /tmp && rails ebb_test && ruby #{Ebb::LIBDIR}/../bin/ebb_rails start -d -e development -c /tmp/ebb_test -u www -g www -P /tmp/ebb.1.pid -p 4043 &}
  #     response = get('/')
  #     assert_equal 200, response.code
  #   ensure
  #     Process.kill('KILL', %x{cat /tmp/ebb.1.pid})
  #   end
end




# 
# class SocketTest < Test::Unit::TestCase
#   def test_socket_creation
#     filename = '/tmp/ebb.socket'
#     @pid = fork do
#       server = Ebb::Server.new(TestApp.new, {:socket => filename})
#       server.start
#     end
#     sleep(1)
#     assert File.exists?(filename)
#     
#     Process.kill('KILL', @pid)
#     
#     assert !File.exists?(filename)
#   end
# end
