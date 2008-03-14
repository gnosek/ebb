require 'rubygems'
require File.dirname(__FILE__) + '/../ruby_lib/ebb'
require 'test/unit'
require 'net/http'
require 'socket'
require 'rubygems'
require 'json'

include Ebb

TEST_PORT = 4044

def get(path)
  Net::HTTP.get_response(URI.parse("http://0.0.0.0:#{TEST_PORT}#{path}"))
end

def post(path, data)
  Net::HTTP.post_form(URI.parse("http://0.0.0.0:#{TEST_PORT}#{path}"), data)
end

class HelperApp
  def call(env)
    commands = env['PATH_INFO'].split('/')
    
    if commands.include?('bytes')
      n = commands.last.to_i
      raise "bytes called with n <= 0" if n <= 0
      body = "C"*n
      status = 200
      
    elsif commands.include?('test_post_length')
      input_body = env['rack.input'].read
      
      content_length_header = env['HTTP_CONTENT_LENGTH'].to_i
      
      if content_length_header == input_body.length
        body = "Content-Length matches input length"
        status = 200
      else
        body = "Content-Length header is #{content_length_header} but body length is #{input_body.length}"
        status = 500
      end
    
    else
      status = 404
      body = "Undefined url"
    end
    
    [status, {'Content-Type' => 'text/plain'}, body]
  end
end

class ServerTest < Test::Unit::TestCase
  def setup
    Thread.new { Ebb.start_server(HelperApp.new, :port => TEST_PORT) }
    sleep 0.1 until Ebb.running?
  end
  
  def teardown
    Ebb.stop_server
    sleep 0.1 while Ebb.running?
  end
end
