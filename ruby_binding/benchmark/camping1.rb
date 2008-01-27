#!/usr/bin/env ruby
require 'rubygems'
require '../ebb'
require 'camping'
require 'rack'
require 'mongrel'

Camping.goes :CampApp
module CampApp
  module Controllers
    class HW < R('/')
      def get
        @headers["X-Served-By"] = URI("http://rack.rubyforge.org")
        @headers["Content-Type"] = 'text/plain'
        "Camping works! " * 500
      end
      def post
        "Data: #{input.foo}"
      end
    end
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
    require 'swiftcore/evented_mongrel'
    ENV['EVENT'] = "1"
    Rack::Handler::Mongrel.run(app, :Port => 4001)
  end
  
  servers << ServerTest.new('ebb', 4002) do
    require '../ebb'
    server = Ebb::Server.new(app, :Port => 4002)
    server.start
  end
  
  servers << ServerTest.new('mongrel', 4003) do
    require 'mongrel'
    Rack::Handler::Mongrel.run(app, :Port => 4003)
  end
  
  servers << ServerTest.new('thin', 4004) do
    require 'thin'
    Rack::Handler::Thin.run(app, :Port => 4004)
  end
  
  
  ([3, 6, 9, 12, 15]*3).rand_each do |concurrency|
    servers.rand_each do |server| 
      $results << server.run_trial(concurrency)
      sleep 0.5 # give the other process some time to cool down?
    end
    puts "---"
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
