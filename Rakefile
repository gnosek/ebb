require 'rake'
require 'rake/testtask'
require 'rake/gempackagetask'
require 'rake/clean'

COMMON_DISTFILES = FileList.new('src/ebb.{c,h}', 'src/parser.{c,h}', 
  'libev/*', 'VERSION', 'README')

RUBY_DISTFILES = COMMON_DISTFILES + FileList.new('src/ebb_ruby.c', 
  'src/extconf.rb', 'ruby_lib/**/*', 'benchmark/*.rb', 'bin/ebb_rails', 
  'test/*')

PYTHON_DISTFILES = COMMON_DISTFILES + FileList.new('setup.py', 
  'src/ebb_python.c')

CLEAN.add ["**/*.{o,bundle,so,obj,pdb,lib,def,exp}", "benchmark/*.dump", 
  'site/index.html', 'MANIFEST']

CLOBBER.add ['src/Makefile', 'src/parser.c', 'src/mkmf.log', 'build']


def dir(path)
  File.expand_path File.join(File.dirname(__FILE__), path)
end

task(:default => :test)

task(:package => 'src/parser.c')

task(:compile => 'src/parser.c') do 
  sh "cd #{dir('src')} && ruby extconf.rb && make"
end

file('MANIFEST') do
  File.open(dir('MANIFEST'), "w+") do |manifest|
    PYTHON_DISTFILES.each { |file| manifest.puts(file) }
  end
end



task(:wc) { sh "wc -l ruby_lib/*.rb src/ebb*.{c,h}" }

task(:test => :compile)
Rake::TestTask.new do |t|
  t.test_files = 'test/basic_test.rb'
  t.verbose = true
end

task(:site_upload => :site) do
  sh 'scp -r site/* rydahl@rubyforge.org:/var/www/gforge-projects/ebb/'
end
task(:site => 'site/index.html')
file('site/index.html' => %w{README site/style.css}) do
  require 'BlueCloth'
  doc = BlueCloth.new(File.read(dir('README')))
  template = <<-HEREDOC
<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
    <title>Ebb</title>
    <link rel="alternate" href="http://max.kanat.us/tag-syndicate/?user=four&tag=ebb" title="RSS Feed" type="application/rss+xml" />
    <link type="text/css" rel="stylesheet" href="style.css" media="screen"/>
  </head>
  <body>  
    <div id="content">CONTENT</div>
  </body>
</html>
HEREDOC
  
  File.open(dir('site/index.html'), "w+") do |f|
    f.write template.sub('CONTENT', doc.to_html)
  end
end

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
  s.rubyforge_project = 'ebb'
  
  s.require_path = 'ruby_lib'
  s.extensions = 'src/extconf.rb'
  s.bindir = 'bin'
  s.executables = %w(ebb_rails)
  
  s.files = RUBY_DISTFILES
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
end

## Compile 
file('src/parser.c' => 'src/parser.rl') do
  #sh "ragel src/parser.rl | rlgen-cd -G2 -o src/parser.c"  # ragel 5
  sh 'ragel -G2 src/parser.rl' # ragel 6
end

file('test/parser_test' => ['src/parser.c', 'src/parser.h']) do
  sh "gcc -g -Wall -I#{dir('src')} -DPARSER_TEST #{dir('src/parser.c')} -o #{dir('test/parser_test')}"
end
