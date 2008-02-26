# A ruby binding to the ebb web server
# Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
# This software is released under the "MIT License". See README file for details.
module Ebb
  LIBDIR = File.expand_path(File.dirname(__FILE__))
  VERSION = File.read(LIBDIR + "/../VERSION").gsub(/\s/,'')
end

$: << Ebb::LIBDIR
require Ebb::LIBDIR + '/../src/ebb_ext'
require 'daemonizable'

module Ebb
  class Client
    BASE_ENV = {
      'SERVER_SOFTWARE' => "Ebb #{Ebb::VERSION}",
      'SERVER_PROTOCOL' => 'HTTP/1.1',
      'GATEWAY_INTERFACE' => 'CGI/1.2',
      'rack.version' => [0, 1],
      'rack.errors' => STDERR,
      'rack.multithread'  => false,
      'rack.multiprocess' => false,
      'rack.run_once' => false
    }.freeze
    
    def env
      @env ||= begin
        env = FFI::client_env(self).update(BASE_ENV)
        env['rack.input'] = RequestBody.new(self)
        env
      end
    end
    
    def finished
      FFI::client_finished(self)
    end
    
    def write(data)
      FFI::client_write(self, data)
    end
  end
  
  class RequestBody
    def initialize(client)
      @client = client
    end
    
    def read(len)
      FFI::client_read_input(@client, len)
    end
    
    def gets
      raise NotImplementedError
    end
    
    def each
      raise NotImplementedError
    end
  end
  
  class Server
    include Daemonizable
    def self.run(app, options={})
      # port must be an integer
      server = self.new(app, options)
      yield server if block_given?
      server.start
    end
    
    def initialize(app, options={})
      @socket = options[:socket]
      @port = (options[:port] || 4001).to_i
      pid_file = options[:pid_file]
      log_file = options[:log_file]
      @timeout =  options[:timeout]
      
      if options[:daemonize]
        change_privilege options[:user], options[:group] if options[:user] && options[:group]
        daemonize
      end
      @app = app
      FFI::server_initialize(self)
    end
    
    def process_client(client)
      begin
        status, headers, body = @app.call(client.env)
      rescue
        raise if $DEBUG
        status = 500
        headers = {'Content-Type' => 'text/plain'}
        body = "Internal Server Error\n"
      end
      
      client.write "HTTP/1.1 %d %s\r\n" % [status, HTTP_STATUS_CODES[status]]
      
      if body.respond_to? :length and status != 304
        client.write "Connection: close\r\n"
        headers['Content-Length'] = body.length
      end
      
      headers.each { |k, v| client.write "#{k}: #{v}\r\n" }
      client.write "\r\n"
      
      # Not many apps use streaming yet so i'll hold off on that feature
      # until the rest of ebb is more developed
      # Yes, I know streaming responses are very cool.
      if body.kind_of?(String)
        client.write body
      else
        body.each { |p| client.write p }
      end
      client.finished
    end
    
    def start
      trap('INT')  { @running = false }
      
      if @socket
        FFI::server_listen_on_socket(self, @socket) or raise "Problem listening on socket #{@socket}"
      else
        FFI::server_listen_on_port(self, @port) or raise "Problem listening on port #{@port}"
      end
      @waiting_clients = []
      
      @running = true
      while FFI::server_process_connections(self) and @running
        unless @waiting_clients.empty?
          if $DEBUG and  @waiting_clients.length > 1
            puts "#{@waiting_clients.length} waiting clients"
          end
          client = @waiting_clients.shift
          process_client(client)
        end
      end
      FFI::server_unlisten(self)
    end
  end
  
  HTTP_STATUS_CODES = {  
    100  => 'Continue', 
    101  => 'Switching Protocols', 
    200  => 'OK', 
    201  => 'Created', 
    202  => 'Accepted', 
    203  => 'Non-Authoritative Information', 
    204  => 'No Content', 
    205  => 'Reset Content', 
    206  => 'Partial Content', 
    300  => 'Multiple Choices', 
    301  => 'Moved Permanently', 
    302  => 'Moved Temporarily', 
    303  => 'See Other', 
    304  => 'Not Modified', 
    305  => 'Use Proxy', 
    400  => 'Bad Request', 
    401  => 'Unauthorized', 
    402  => 'Payment Required', 
    403  => 'Forbidden', 
    404  => 'Not Found', 
    405  => 'Method Not Allowed', 
    406  => 'Not Acceptable', 
    407  => 'Proxy Authentication Required', 
    408  => 'Request Time-out', 
    409  => 'Conflict', 
    410  => 'Gone', 
    411  => 'Length Required', 
    412  => 'Precondition Failed', 
    413  => 'Request Entity Too Large', 
    414  => 'Request-URI Too Large', 
    415  => 'Unsupported Media Type', 
    500  => 'Internal Server Error', 
    501  => 'Not Implemented', 
    502  => 'Bad Gateway', 
    503  => 'Service Unavailable', 
    504  => 'Gateway Time-out', 
    505  => 'HTTP Version not supported'
  }.freeze
end

module Rack
  module Adapter
    autoload :Rails, 'rails_adapter'
  end
end
