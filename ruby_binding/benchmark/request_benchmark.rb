#!/usr/bin/env ruby
$: << File.expand_path(File.dirname(__FILE__))

require 'server_test'

trap('INT')  { exit(1) }
dumpfile = 'request_results.dump'
begin
  results = ServerTestResults.open(dumpfile)
  $servers.each { |s| s.start }
  sleep 3
  [0,5,7,10,15,18,20,23,25,30,40,45,50].map { |i| i.kilobytes }.rand_each do |size|
    $servers.rand_each do |server| 
      if r = server.post_trial(size)
        results << r
      else
        puts "error!"
        server.kill
        sleep 0.5
        server.start
        sleep 2
      end
      sleep 2 # give the other process some time to cool down?
    end
    puts "---"
  end
ensure
  puts "\n\nkilling servers"
  $servers.each { |server| server.kill }  
  results.write(dumpfile)
end
