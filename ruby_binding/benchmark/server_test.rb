$: << File.expand_path(File.dirname(__FILE__) + '/..')

require 'rubygems'
require 'camping'
require 'rack'

Camping.goes :CampApp
module CampApp
  module Controllers
    class Index < R '/','/(\d+)'
      @@responses = {}
      def get(size=1)
        @headers["Content-Type"] = 'text/plain'
        size = size.to_i
        raise "size is #{size}" if size <= 0
        @@responses[size] ||= "C" * size
      end
    end
  end
end


module Bytes
  def bytes
    self
  end
  alias :byte :bytes

  def kilobytes
    self * 1024
  end
  alias :kilobyte :kilobytes

  def megabytes
    self * 1024.kilobytes
  end
  alias :megabyte :megabytes

  def gigabytes
    self * 1024.megabytes 
  end
  alias :gigabyte :gigabytes

  def terabytes
    self * 1024.gigabytes
  end
  alias :terabyte :terabytes
  
  def petabytes
    self * 1024.terabytes
  end
  alias :petabyte :petabytes
  
  def exabytes
    self * 1024.petabytes
  end
  alias :exabyte :exabytes
  
end
class Fixnum
  include Bytes
end

def number_to_human_size(size, precision=1)
  size = Kernel.Float(size)
  case
    when size.to_i == 1;    "1 Byte"
    when size < 1.kilobyte; "%d Bytes" % size
    when size < 1.megabyte; "%.#{precision}f KB"  % (size / 1.0.kilobyte)
    when size < 1.gigabyte; "%.#{precision}f MB"  % (size / 1.0.megabyte)
    when size < 1.terabyte; "%.#{precision}f GB"  % (size / 1.0.gigabyte)
    else                    "%.#{precision}f TB"  % (size / 1.0.terabyte)
  end.sub(/([0-9])\.?0+ /, '\1 ' )
rescue
  nil
end

class Array
  def avg
    sum.to_f / length
  end
  
  def sum
    inject(0) { |i, s| s += i }
  end
  
  def rand_each(&block)
    sort_by{ rand }.each &block
  end
end

class ServerTestResults
  def self.open(filename)
    if File.readable?(filename)
      new(Marshal.load(File.read(filename))) 
    else
      new
    end
  end

  def initialize(results = [])
    @results = results
  end
  
  def write(filename='results.dump')
    puts "writing dump file to #{filename}"
    File.open(filename, 'w+') do |f|
      f.write Marshal.dump(@results)
    end
  end
  
  def <<(r)
    @results << r
  end
  
  def length
    @results.length
  end

  def servers
    @results.map {|r| r[:server] }.uniq.sort
  end

  def data(server, what=:size)
    server_data = @results.find_all { |r| r[:server] == server }
    ticks = server_data.map { |d| d[what] }.uniq
    datas = []
    ticks.each do |c|
      measurements = server_data.find_all { |d| d[what] == c }.map { |d| d[:rps] }
      datas << [c, measurements.avg]
    end
    datas
  end

end

class ServerTest
  attr_reader :name, :port, :app, :pid
  def initialize(name, port, &start_block)
    @name = name
    @port = port
    @start_block = start_block
  end
  
  def <=>(a)
    @name <=> a.name
  end
  
  def kill
    Process.kill('KILL', @pid)
  end
  
  def running?
    !@pid.nil?
  end
  
  def start
    puts "Starting #{name}"
    @pid = fork { @start_block.call }
  end
  
  def trial(options = {})
    concurrency = options[:concurrency] || 50
    size = options[:size] || 500
    requests = options[:requests] || 500
    
    print "#{@name} (c=#{concurrency},s=#{number_to_human_size(size)})  "
    $stdout.flush
    r = %x{ab -q -c #{concurrency} -n #{requests} http://0.0.0.0:#{@port}/#{size}}
    # Complete requests:      1000

    return nil unless r =~ /Requests per second:\s*(\d+\.\d\d)/
    rps = $1.to_f
    if r =~ /Complete requests:\s*(\d+)/
      completed_requests = $1.to_i
    end
    puts "#{rps} req/sec (#{completed_requests} completed)"
    {
      :test => 'camping1',
      :server=> @name, 
      :concurrency => concurrency, 
      :size => size,
      :rps => rps,
      :requests => requests,
      :requests_completed => completed_requests,
      :time => Time.now
    }
  end
  
end

$servers = []
app = Rack::Adapter::Camping.new(CampApp)
$servers << ServerTest.new('evented mongrel', 4001) do
  require 'mongrel'
  require 'swiftcore/evented_mongrel'
  ENV['EVENT'] = "1"
  Rack::Handler::Mongrel.run(app, :Port => 4001)
end

$servers << ServerTest.new('ebb', 4002) do
  require 'ebb'
  server = Ebb::Server.new(app, :Port => 4002)
  server.start
end

$servers << ServerTest.new('mongrel', 4003) do
 require 'mongrel'
 Rack::Handler::Mongrel.run(app, :Port => 4003)
end
 
$servers << ServerTest.new('thin', 4004) do
  require 'thin'
  Rack::Handler::Thin.run(app, :Port => 4004)
end
