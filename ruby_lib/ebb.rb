# Ruby Binding to the Ebb Web Server
# Copyright (c) 2008 Ry Dahl. This software is released under the MIT License.
# See README file for details.
require 'stringio'
module Ebb
  LIBDIR = File.dirname(__FILE__)
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

    if options.has_key?(:fileno)
      FFI::server_listen_on_fd(options[:fileno].to_i)
    else
      FFI::server_listen_on_port(port)
    end
    @running = true
    trap('INT')  { stop_server }
    
    log.puts "Ebb listening at http://0.0.0.0:#{port}/ (#{threaded_processing ? 'threaded' : 'sequential'} processing, PID #{Process.pid})"
    
    while @running
      FFI::server_process_connections()
      while client = FFI::waiting_clients.shift
        if threaded_processing
          Thread.new(client) { |c| process(app, c) }
        else
          process(app, client)
        end
      end
    end
    FFI::server_unlisten()
  end
  
  def self.running?
    FFI::server_open?
  end
  
  def self.stop_server()
    @running = false
  end
  
  def self.process(app, client)
    #p client.env
    status, headers, body = app.call(client.env)
    
    # Write the status
    client.write_status(status)
    
    # Add Content-Length to the headers.
    if headers['Content-Length'].nil? and
       headers.respond_to?(:[]=) and 
       body.respond_to?(:length) and 
       status != 304
    then
      headers['Content-Length'] = body.length.to_s
    end
    
    # Decide if we should keep the connection alive or not
    if headers['Connection'].nil?
      if headers['Content-Length'] and client.should_keep_alive?
        headers['Connection'] = 'Keep-Alive'
      else
        headers['Connection'] = 'close'
      end
    end
    
    # Write the headers
    headers.each { |field, value| client.write_header(field, value) }
    
    # Write the body
    if body.kind_of?(String)
      client.write_body(body)
    else
      body.each { |p| client.write_body(p) }
    end
    
  rescue => e
    log.puts "Ebb Error! #{e.class}  #{e.message}"
    log.puts e.backtrace.join("\n")
  ensure
    client.release
  end
  
  @@log = STDOUT
  def self.log=(output)
    @@log = output
  end
  def self.log
    @@log
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
    
    def env
      @env ||= begin
        env = FFI::client_env(self).update(BASE_ENV)
        env['rack.input'] = RequestBody.new(self)
        env
      end
    end
    
    def write_status(status)
      s = status.to_i
      FFI::client_write_status(self, s, HTTP_STATUS_CODES[s])
    end
    
    def write_body(data)
      FFI::client_write_body(self, data)
    end
    
    def write_header(field, value)
      value.send(value.is_a?(String) ? :each_line : :each) do |v| 
        FFI::client_write_header(self, field, v.chomp)
      end
    end
    
    def release
      FFI::client_release(self)
    end
    
    def set_keep_alive
      FFI::client_set_keep_alive(self)
    end
    
    def should_keep_alive?
      if env['HTTP_VERSION'] == 'HTTP/1.0' 
        return true if env['HTTP_CONNECTION'] =~ /Keep-Alive/i
      else
        return true unless env['HTTP_CONNECTION'] =~ /close/i
      end
      false
    end
  end
  
  class RequestBody
    def initialize(client)
      @client = client
    end
    
    def read(len = nil)
      if @io
        @io.read(len)
      else
        if len.nil?
          s = ''
          while(chunk = read(10*1024)) do
            s << chunk
          end
          s
        else
          FFI::client_read_input(@client, len)
        end
      end
    end
    
    def gets
      io.gets
    end
    
    def each(&block)
      io.each(&block)
    end
    
    def io
      @io ||= StringIO.new(read)
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
