require 'rake'
require 'rake/testtask'
require 'rake/gempackagetask'
require 'rake/clean'

COMMON_DISTFILES = FileList.new('src/ebb.{c,h}', 'src/parser.{c,h}', 
  'libev/*', 'README')

RUBY_DISTFILES = COMMON_DISTFILES + FileList.new('src/ebb_ruby.c', 
  'src/extconf.rb', 'ruby_lib/**/*', 'benchmark/*.rb', 'bin/ebb_rails', 
  'test/*')

PYTHON_DISTFILES = COMMON_DISTFILES + FileList.new('setup.py', 
  'src/ebb_python.c')

CLEAN.add ["**/*.{o,bundle,so,obj,pdb,lib,def,exp}", "benchmark/*.dump", 
  'site/index.html', 'MANIFEST']

CLOBBER.add ['src/Makefile', 'src/parser.c', 'src/mkmf.log', 'build']

Rake::TestTask.new do |t|
  t.test_files = FileList.new("test/*.rb")
  t.verbose = true
end

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

task(:test => RUBY_DISTFILES)
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
  s.email = 'ry at tiny clouds dot org'
  s.homepage = 'http://ebb.rubyforge.org'
  s.version = `grep EBB_VERSION #{dir('src/ebb.h')}`.match(/\d.\d.\d/)[0]
  s.rubyforge_project = 'ebb'
  
  s.add_dependency('rack')
  s.required_ruby_version = '>= 1.8.4'
  
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
  sh 'ragel -s -G2 src/parser.rl'
end
