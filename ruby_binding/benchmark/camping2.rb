#!/usr/bin/env ruby
require 'rubygems'
require '../ebb'
require 'camping'
require 'rack'
require 'mongrel'
require 'thin'

require 'server_test'


begin
  results = ServerTestResults.open('./results.dump')
  $servers.each { |s| s.start }
  sleep 3
  [1,10,20,30,50].map { |i| i.kilobytes }.rand_each do |size|
    $servers.rand_each do |server| 
      if r = server.trial(:size => size)
        results << r
      else
        puts "error! restarting server"
        server.kill
        server.start
      end
      sleep 0.2 # give the other process some time to cool down?
    end
    puts "---"
  end
ensure
  puts "\n\nkilling servers"
  $servers.each { |server| server.kill }  
  results.write('./results.dump')
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
