require 'ebb_ext'

module Ebb
  class Server
    def self.run(app, options={})
      # port must be an integer
      server = self.new(app, options)
      yield server if block_given?
      server.start
    end
    
    def initialize(app, options={})
      @host = options[:Host] || '0.0.0.0'
      @port = options[:Port].to_i || 4001
      @app = app
    end
    
    # Called by the C library on each request.
    # env is a hash containing all the variables of the request
    # client is a TCPSocket
    # XXX: push this code to C?
    def process(client)
      status, header, body = @app.call(client.env)
      
      client.write("HTTP/1.1 %d %s\r\n" % [status, HTTP_STATUS_CODES[status]])
      
      if body.respond_to? :length and status != 304
        client.write("Connection: close\r\n")
        header['Content-Length'] = body.length
      end
      
      headers.each { |k, v| client.write("#{k}: #{v}\r\n") }
      body.each { |part| client.write(part) }
    ensure
      body.close if body.respond_to? :close
      client.close
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