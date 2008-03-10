# Ruby Binding to the Ebb Web Server
# Copyright (c) 2008 Ry Dahl. This software is released under the MIT License.
# See README file for details.
module Ebb
  LIBDIR = File.dirname(__FILE__)
  VERSION = File.read(LIBDIR + "/../VERSION").gsub(/\s/,'')
  require Ebb::LIBDIR + '/../src/ebb_ext'
  autoload :Runner, LIBDIR + '/ebb/runner'
  
  def self.start_server(app, options={})
    port = (options[:port] || 4001).to_i
    nworkers = options[:workers] || 1
    # socket = options[:socket]
    # timeout =  options[:timeout]
    
    FFI::server_listen_on_port(port)
    
    puts "Ebb listening at http://0.0.0.0:#{port}/ (#{nworkers} worker#{'s' if nworkers > 1})"
    
    trap('INT')  { @running = false }
    @running = true
    
    workers = ThreadGroup.new
    nworkers.times do
      thread = Thread.new do
        while @running
          FFI::server_process_connections()
          if client = FFI::waiting_clients.shift
            process_client(app, client)
          end
        end
      end
      workers.add(thread)
    end
    workers.list.each { |thread| thread.join }
    
    puts "Ebb unlistening"
    FFI::server_unlisten()
  end
  
  def self.process_client(app, client)
    #puts "Request: #{client.env.inspect}\n"
    begin
      status, headers, body = app.call(client.env)
    rescue
      raise if $DEBUG
      status = 500
      headers = {'Content-Type' => 'text/plain'}
      body = "Internal Server Error\n"
    end
    
    client.write_status(status)
    
    if body.respond_to? :length and status != 304
      headers['Connection'] = 'close'
      headers['Content-Length'] = body.length
    end
    
    headers.each { |k, v| client.write_header(k,v) }
    
    client.write "\r\n"
    
    # Not many apps use streaming yet so i'll hold off on that feature
    # until the rest of ebb is more developed.
    if body.kind_of?(String)
      client.write body
    else
      body.each { |p| client.write p }
    end
    client.finished
  end
  
  def FFI.waiting_clients
    @waiting_clients
  end
  
  class Client
    BASE_ENV = {
      'SCRIPT_NAME' => '',
      'SERVER_SOFTWARE' => "Ebb #{Ebb::VERSION}",
      'SERVER_PROTOCOL' => 'HTTP/1.1',
      'rack.version' => [0, 1],
      'rack.errors' => STDERR,
      'rack.url_scheme' => 'http',
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
    
    def write_status(status)
      FFI::client_write_status(self, status.to_i, HTTP_STATUS_CODES[status])
    end
    
    def write_header(field, value)
      FFI::client_write_header(self, field.to_s, value.to_s)
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