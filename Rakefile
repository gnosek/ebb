require 'rake'
require 'rake/gempackagetask'
require File.dirname(__FILE__) + '/ruby_binding/lib/ebb'

spec = Gem::Specification.new do |s|
  s.platform = Gem::Platform::RUBY
  s.summary = "Web server"
  s.name = 'ebb'
  s.author = 'ry dahl'
  s.email = 'ry@tinyclouds.org'
  s.homepage = 'http://repo.or.cz/w/ebb.git'
  s.version = Ebb::VERSION
  s.requirements << 'none'
  
  s.require_path = 'ruby_binding/lib'
  s.extensions = 'ruby_binding/ext/extconf.rb'
  s.executables = %w(ebb_rails)
  s.bindir = "ruby_binding/bin"
  
  s.autorequire = 'rake'
  s.files = FileList.new('ruby_binding/**/*', 'src/**/*', 'README').to_a
  s.description = ''
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
end

task :clean do 
  sh 'cd src && make clean'
  sh 'cd ruby_binding && rake clobber'
  sh 'rm -rf pkg'
end