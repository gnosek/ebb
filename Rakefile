require 'rake'
require 'rake/gempackagetask'

spec = Gem::Specification.new do |s|
  s.platform = Gem::Platform::RUBY
  s.summary = "Web server"
  s.name = 'ebb'
  s.version = '0.0.1'
  s.requirements << 'none'
  
  s.require_path = 'ruby_binding'
  s.extensions = 'ruby_binding/extconf.rb'
  
  s.autorequire = 'rake'
  s.files = FileList.new('**/*').to_a
  s.description = ''
end

Rake::GemPackageTask.new(spec) do |pkg|
  pkg.need_zip = true
  pkg.need_tar = true
end