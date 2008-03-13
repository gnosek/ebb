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
    if options.has_key?(:threaded_processing)
      threaded_processing = options[:threaded_processing] ? true : false
    else
      threaded_processing = true
    end
    
    Client::BASE_ENV['rack.multithread'] = threaded_processing
    
    FFI::server_listen_on_port(port)
    
    puts "Ebb listening at http://0.0.0.0:#{port}/ (#{threaded_processing ? 'threaded' : 'sequential'} processing)"
    trap('INT')  { @running = false }
    @running = true
    
    while @running
      FFI::server_process_connections()
      while client = FFI::waiting_clients.shift
        if threaded_processing
          Thread.new(client) { |c| c.process(app) }
        else
          client.process(app)
        end
      end
    end
    
    puts "Ebb unlistening"
    FFI::server_unlisten()
  end
  
  # This array is created and manipulated in the C extension.
  def FFI.waiting_clients
    @waiting_clients
  end
  
  class Client
    BASE_ENV = {
      'SERVER_NAME' => '0.0.0.0',
      'SCRIPT_NAME' => '',
      'SERVER_SOFTWARE' => "Ebb #{Ebb::VERSION}",
      'SERVER_PROTOCOL' => 'HTTP/1.1',
      'rack.version' => [0, 1],
      'rack.errors' => STDERR,
      'rack.url_scheme' => 'http',
      'rack.multiprocess' => false,
      'rack.run_once' => false
    }
    
    def process(app)
      begin
        status, headers, body = app.call(env)
      rescue
        raise if $DEBUG
        status = 500
        headers = {'Content-Type' => 'text/plain'}
        body = "Internal Server Error\n"
      end
      
      status = status.to_i
      FFI::client_write_status(self, status, HTTP_STATUS_CODES[status])
      
      if headers.respond_to?(:[]=) and body.respond_to?(:length) and status != 304
        headers['Connection'] = 'close'
        headers['Content-Length'] = body.length.to_s
      end
      
      headers.each { |field, value| write_header(field, value) }
      write("\r\n")
      
      if body.kind_of?(String)
        write(body)
        body_written()
        begin_transmission()
      else
        begin_transmission()
        body.each { |p| write(p) }
        body_written()
      end
    rescue => e
      puts "Error! #{e.class}  #{e.message}"
    ensure
      FFI::client_release(self)
    end
    
    private
    
    def env
      env = FFI::client_env(self).update(BASE_ENV)
      env['rack.input'] = RequestBody.new(self)
      env
    end
    
    def write(data)
      FFI::client_write(self, data)
    end
    
    def write_header(field, value)
      value.send(value.is_a?(String) ? :each_line : :each) do |v| 
        FFI::client_write_header(self, field, v.chomp)
      end
    end
    
    def body_written
      FFI::client_set_body_written(self, true)
    end
    
    def begin_transmission
      FFI::client_begin_transmission(self)
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
