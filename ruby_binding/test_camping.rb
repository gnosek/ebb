#!/usr/bin/env ruby
require 'rubygems'
require 'ebb'
require 'camping'
require 'rack'

Camping.goes :CampApp
module CampApp
  module Controllers
    class HW < R('/')
      def get
        @headers["X-Served-By"] = URI("http://rack.rubyforge.org")
        "Camping works!"
      end

      def post
        "Data: #{input.foo}"
      end
    end
  end
end


app = Rack::Adapter::Camping.new(CampApp)

case ARGV[0]
when 'ebb'
  puts "ebb"
  server = Ebb::Server.new(app, :Port => 4001)
  server.start
# when 'm'
#   puts "mongrel"
#   Rack::Handler::Mongrel.run(app, :Port => 4002)
when 'mongrel'
  puts "evented mongrel"
  require 'swiftcore/evented_mongrel' 
  ENV['EVENT'] = "1"
  Rack::Handler::Mongrel.run(app, :Port => 4002)
else
  puts "unknown arg"
end