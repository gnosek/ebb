#!/usr/bin/env ruby
require 'rubygems'
require 'rack'
require 'rack/handler/scgi'
require File.dirname(__FILE__) + '/../application'

trap ('INT') { exit }
puts ("Starting SCGI server at 0.0.0.0:9001")
Rack::Handler::SCGI.run(SimpleApp.new, :Port => 9001)
