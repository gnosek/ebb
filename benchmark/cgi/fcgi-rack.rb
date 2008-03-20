#!/usr/bin/env ruby
require 'rubygems'
require 'rack'
require File.dirname(__FILE__) + '/../application'

trap ('INT') { exit }
puts ("Starting FastCGI server at 0.0.0.0:9001")
Rack::Handler::FastCGI.run(SimpleApp.new, :Port => 9001)
