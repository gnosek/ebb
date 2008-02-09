# A ruby binding to the ebb web server
# Copyright (c) 2007 Ry Dahl <ry.d4hl@gmail.com>
# This software is released under the "MIT License". See README file for details.
$: << File.expand_path(File.dirname(__FILE__))
require 'ebb_ext'

module Ebb
  class Client
    attr_reader :env
    
    def env
      @env.update(
        'rack.input' => Input.new(self)
      )
    end
  end
  
  class Input
    def initialize(client)
      @client = client
    end
    
    def read(len = 1)
      @client.read_input(len)
    end
    
    def gets
      raise NotImplementedError
    end
    
    def each
      raise NotImplementedError
    end
  end
  
  class Server
    def self.run(app, options={})
      # port must be an integer
      server = self.new(app, options)
      yield server if block_given?
      server.start
    end
    
    def initialize(app, options={})
      @host = options[:Host] || '0.0.0.0'
      @port = (options[:Port] || 4001).to_i
      @app = app
      init(@host, @port)
    end
    
    # Called by the C library on each request.
    # env is a hash containing all the variables of the request
    # client is a TCPSocket
    def process_client(client)
      status, headers, body = @app.call(client.env)
      
      client.write "HTTP/1.1 %d %s\r\n" % [status, HTTP_STATUS_CODES[status]]
      
      if body.respond_to? :length and status != 304
        client.write "Connection: close\r\n"
        headers['Content-Length'] = body.length
      end
      
      headers.each { |k, v| client.write "#{k}: #{v}\r\n" }
      client.write "\r\n"
      if body.kind_of?(String)
        client.write body
      elsif body.kind_of?(StringIO)
        client.write body.string
      else
        # Not many apps use this yet so i'll hold off on streaming
        # responses until the rest of ebb is more developed
        # Yes, I know streaming responses are very cool.
        raise NotImplementedError, "Unsupported body of type #{body.class}"
      end
    end
    
    def start
      trap('INT')  { @running = false }
      trap('TERM') { @running = false }
      really_start
      @running = true
      while process_connections and @running
        unless @waiting_clients.empty?
          if $debug
            puts "#{@waiting_clients.length} waiting clients" if @waiting_clients.length > 1
          end
          client = @waiting_clients.shift
          process_client(client)
          client.start_writing
        end
      end
    ensure
      stop
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
