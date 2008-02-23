# A ruby binding to the ebb web server
# Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
# This software is released under the "MIT License". See README file for details.
module Ebb
  LIBDIR = File.expand_path(File.dirname(__FILE__))
end

$: << Ebb::LIBDIR
require Ebb::LIBDIR + '/../src/ebb_ext'
require 'daemonizable'

module Ebb
  class Client
    BASE_ENV = {
      'SERVER_SOFTWARE' => 'Ebb 0.0.1',
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
        @env = @ebb_env.update(BASE_ENV)
        @env['rack.input'] = Input.new(self)
        @env
      end
    end
  end
  
  class Input
    CHUNKSIZE = 4*1024
    def initialize(client)
      @client = client
    end
    
    def read(len = 1)
      @client.read_input(len)
    end
    
    def gets
      raise NotImplementedError, "Fix me, please. Yes, you!"
    end
    
    def each
      raise NotImplementedError, "Fix me, please Yes, you!"
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
      @host = options[:host] || '0.0.0.0'
      @port = (options[:port] || 4001).to_i
      pid_file = options[:pid_file]
      log_file = options[:log_file]
      @timeout =  options[:timeout]

      if options[:daemonize]
        change_privilege options[:user], options[:group] if options[:user] && options[:group]
        daemonize
      end
      @app = app
      init(@host, @port)
    end
    
    def process_client(client)
      status, headers, body = @app.call(client.env)
      
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
      listen
      @running = true
      while process_connections and @running
        unless @waiting_clients.empty?
          if $debug and  @waiting_clients.length > 1
            puts "#{@waiting_clients.length} waiting clients"
          end
          client = @waiting_clients.shift
          process_client(client)
        end
      end
      unlisten
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
  }
end

module Rack
  module Adapter
    autoload :Rails, 'rails_adapter'
  end
end
