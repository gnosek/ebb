require 'rake'
require 'rake/gempackagetask'
require 'rake/clean'

task(:package => 'src/parser.c')
file('src/parser.c' => 'src/parser.rl') do
  sh 'ragel src/parser.rl | rlgen-cd -G2 -o src/parser.c'
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
  s.version = File.read(File.dirname(__FILE__) + "/VERSION").gsub(/\s/,'')
  s.requirements << 'none'
  
  s.require_path = 'ruby_lib'
  s.extensions = 'src/extconf.rb'
  s.bindir = 'bin'
  s.executables = %w(ebb_rails)
  
  s.files = FileList.new ['{src,libev,benchmark,ruby_lib}/*.(rb|c|h)', 'bin/ebb_rails','README']
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
end


CLEAN.add ["**/*.{o,bundle,so,obj,pdb,lib,def,exp}", "benchmark/*.dump"]
CLOBBER.add ['src/Makefile', 'src/parser.c', 'src/mkmf.log','doc', 'coverage']
