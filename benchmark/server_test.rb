$: << File.expand_path(File.dirname(__FILE__))

require 'rubygems'
require 'rack'
require 'application'


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
  
  def benchmark
    @results.first[:benchmark]
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

  def data(server)
    server_data = @results.find_all { |r| r[:server] == server }
    ticks = server_data.map { |d| d[:input] }.uniq
    datas = []
    ticks.each do |c|
      measurements = server_data.find_all { |d| d[:input] == c }.map { |d| d[:rps] }
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
    case name
    when 'emongrel'
      @pid = fork { start_emongrel }
    when 'ebb'
      @pid = fork { start_ebb }
    when 'mongrel'
      @pid = fork { start_mongrel }
    when 'thin'
      @pid = fork { start_thin }
    else
      @pid = fork { @start_block.call }
    end
  end
  
  def app
    SimpleApp.new
  end
  
  def start_emongrel
    require 'mongrel'
    require 'swiftcore/evented_mongrel'
    ENV['EVENT'] = "1"
    Rack::Handler::Mongrel.run(app, :Port => @port)
  end

  def start_ebb
    require File.dirname(__FILE__) + '/../ruby_lib/ebb'
    server = Ebb::Server.new(app, :port => @port)
    server.start
  end
  
  def start_mongrel
   require 'mongrel'
   Rack::Handler::Mongrel.run(app, :Port => @port)
  end
  
  def start_thin
    require 'thin'
    Rack::Handler::Thin.run(app, :Port => @port)
  end
  
  def trial(ab_cmd)
    cmd = ab_cmd.sub('PORT', @port)
    
    puts "#{@name} (#{cmd})"
    
    r = %x{#{cmd}}
    
    return nil unless r =~ /Requests per second:\s*(\d+\.\d\d)/
    rps = $1.to_f
    if r =~ /Complete requests:\s*(\d+)/
      requests_completed = $1.to_i
    end
    puts "   #{rps} req/sec (#{requests_completed} completed)"
    
    {
      :server => @name,
      :rps => rps,
      :requests_completed => requests_completed,
      :ab_cmd => cmd
    }
  end
end