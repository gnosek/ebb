DIR = File.dirname(__FILE__)

def fib(n)
  return 1 if n <= 1
  fib(n-1) + fib(n-2)
end

def wait(seconds)
  n = (seconds / 0.01).to_i
  n.times do 
    sleep(0.01)
    #File.read(DIR + '/yahoo.html') 
  end
end

class SimpleApp
  @@responses = {}
  
  def initialize
    @count = 0
  end
  
  def call(env)
    commands = env['PATH_INFO'].split('/')
    
    @count += 1
    if commands.include?('periodical_activity') and @count % 10 != 1
      return [200, {'Content-Type'=>'text/plain'}, "quick response!\r\n"]
    end
    
    if commands.include?('fibonacci')
      n = commands.last.to_i
      raise "fibonacci called with n <= 0" if n <= 0
      body = (1..n).to_a.map { |i| fib(i).to_s }.join(' ')
      status = 200
    
    elsif commands.include?('wait')
      n = commands.last.to_i
      raise "wait called with n <= 0" if n <= 0
      wait(n)
      body = "waited about #{n} seconds"
      status = 200
    
    elsif commands.include?('bytes')
      n = commands.last.to_i
      raise "bytes called with n <= 0" if n <= 0
      body = @@responses[n] || "C"*n
      status = 200
      
    elsif commands.include?('test_post_length')
      input_body = ""
      while chunk = env['rack.input'].read(512)
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
    
    else
      status = 404
      body = "Undefined url"
    end
    
    [status, {'Content-Type' => 'text/plain'}, body + "\r\n\r\n"]
  end
end


if $0 == __FILE__
  require DIR + '/../ruby_lib/ebb'
  require 'rubygems'
  require 'ruby-debug'
  Debugger.start
  
  server = Ebb::Server.new(SimpleApp.new, :port => 4001)
  puts "Ebb started on http://0.0.0.0:4001/"
  server.start
end