require 'rake'
require 'rake/gempackagetask'
require 'rake/clean'

def dir(path)
  File.expand_path File.join(File.dirname(__FILE__), path)
end

task(:default => :compile)

task(:package => 'src/parser.c')

task(:compile => 'src/parser.c') do 
  sh "cd #{dir('src')} && ruby extconf.rb && make"
end

file('src/parser.c' => 'src/parser.rl') do
  sh "ragel src/parser.rl | rlgen-cd -G2 -o src/parser.c"
end

task(:wc) { sh "wc -l ruby_lib/*.rb src/ebb*.{c,h}" }

spec = Gem::Specification.new do |s|
  s.platform = Gem::Platform::RUBY
  s.summary = "A Web Server"
  s.description = ''
  s.name = 'ebb'
  s.author = 'ry dahl'
  s.email = 'ry@tinyclouds.org'
  s.homepage = 'http://repo.or.cz/w/ebb.git'
  s.version = File.read(dir("VERSION")).gsub(/\s/,'')
  s.requirements << 'none'
  
  s.require_path = 'ruby_lib'
  s.extensions = 'src/extconf.rb'
  s.bindir = 'bin'
  s.executables = %w(ebb_rails)
  
  s.files = FileList.new('src/*.{c,h}',
                         'src/extconf.rb',
                         'libev/*',
                         'ruby_lib/*',
                         'benchmark/*.rb',
                         'bin/ebb_rails',
                         'README')
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
end

CLEAN.add ["**/*.{o,bundle,so,obj,pdb,lib,def,exp}", "benchmark/*.dump"]
CLOBBER.add ['src/Makefile', 'src/parser.c', 'src/mkmf.log','doc', 'coverage']
