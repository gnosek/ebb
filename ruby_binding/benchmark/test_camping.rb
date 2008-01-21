#!/usr/bin/env ruby
require 'rubygems'
require '../ebb'
require 'camping'
require 'rack'
require 'mongrel'
require 'swiftcore/evented_mongrel' 
require 'thin'

Camping.goes :CampApp
module CampApp
  module Controllers
    class HW < R('/')
      def get
        @headers["X-Served-By"] = URI("http://rack.rubyforge.org")
        @headers["Content-Type"] = 'text/plain'
        "Camping works! " * 5000
      end
      def post
        "Data: #{input.foo}"
      end
    end
  end
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


class ServerTest
  attr_reader :name, :port, :app
  def initialize(name, port, &start_block)
    @name = name
    @port = port
    puts "Starting #{name}"
    @pid = fork { start_block.call }
    sleep 3
  end
  
  def <=>(a)
    @name <=> a.name
  end
  
  def kill
    Process.kill('KILL', @pid)
  end
  
  def run_trial(concurrency)
    print "#{@name} with concurrency #{concurrency}..."
    $stdout.flush
    r = %x{ab -q -c #{concurrency} -n 1000 http://0.0.0.0:#{@port}/ | grep "Requests per second" }
    raise "couldn't match rps in #{r.inspect}" unless r =~ /Requests per second:\s*(\d+\.\d\d)/
    rps = $1.to_f
    puts rps
    {
      :test => 'camping1',
      :server=> @name, 
      :concurrency => concurrency, 
      :rps => rps,
      :time => Time.now
    }
  end
end

def all_tests(dump_file = "./benchmarks.ruby_dump")
  
  if File.readable?(dump_file)
    $results = Marshal.load(File.read(dump_file))
  else
    $results = []
  end
  
  app = Rack::Adapter::Camping.new(CampApp)
  servers = []
  
  servers << ServerTest.new('evented mongrel', 4001) do
    ENV['EVENT'] = "1"
    Rack::Handler::Mongrel.run(app, :Port => 4001)
  end
  
  servers << ServerTest.new('ebb', 4002) do
    server = Ebb::Server.new(app, :Port => 4002)
    server.start
  end
  
  servers << ServerTest.new('thin', 4003) do
    Rack::Handler::Thin.run(app, :Port => 4003)
  end
  
  
  ([1, 10, 19, 28, 37, 46, 55, 64, 73, 82, 91, 100]*3).rand_each do |concurrency|
    servers.rand_each do |server| 
      $results << server.run_trial(concurrency)
      sleep 0.5 # give the other process some time to cool down?
    end
  end
  
ensure
  puts "killing servers"
  servers.each { |server| server.kill }  
  unless $results.empty?
    puts "writing to dump file #{dump_file}"
    File.open(dump_file, 'w+') do |f|
      f.write Marshal.dump($results)
    end
  end
end

all_tests

# when 'ebb'
#   puts "ebb"
#   server = Ebb::Server.new(app, :Port => 4001)
#   server.start
# # when 'm'
# #   puts "mongrel"
# #   Rack::Handler::Mongrel.run(app, :Port => 4002)
# when 'mongrel'
# 
# else
#   puts "unknown arg"
# end