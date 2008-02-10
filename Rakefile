require 'rake'
require 'rake/gempackagetask'

spec = Gem::Specification.new do |s|
  s.platform = Gem::Platform::RUBY
  s.summary = "Web server"
  s.name = 'ebb'
  s.author = 'ry dahl'
  s.email = 'ry@tinyclouds.org'
  s.homepage = 'http://repo.or.cz/w/ebb.git'
  s.version = '0.0.1'
  s.requirements << 'none'
  
  s.require_path = 'ruby_binding'
  s.extensions = 'ruby_binding/extconf.rb'
  
  s.autorequire = 'rake'
  s.files = FileList.new('ruby_binding/**/*', 'src/**/*', 'README').to_a
  s.description = ''
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
  pkg.need_tar = true
end

task :clean do 
  sh 'cd src && make clean'
  sh 'cd ruby_binding && rake clobber'
  sh 'rm -rf pkg'
end