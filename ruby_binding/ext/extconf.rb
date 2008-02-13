require 'mkmf'

dir_config('ev')

unless have_library('ev', 'ev_version_major') and
       have_header('ev.h') and
       pkg_config('glib-2.0')
  then
  exit 1
end

## First we should build Ebb
# Avoiding autoconf. hack :(
here = File.expand_path(File.dirname(__FILE__))
ebb_dir = here + '/../../src'
puts "Building Ebb"
ebb_makefile = File.read("#{ebb_dir}/Makefile")
ebb_makefile.gsub!('CFLAGS = ', "CFLAGS = #{$CFLAGS} ")
ebb_makefile.gsub!('LIBS = ', "LIBS = #{$LDFLAGS} ")
makefile_filename = "#{ebb_dir}/.ruby_generated_makefile"
File.open(makefile_filename, "w+") do |f|
  f.write ebb_makefile
end
puts %x{ cd #{ebb_dir} && make -f #{makefile_filename}}



# Help.. what's the proper way to do this?
$LOCAL_LIBS = "#{ebb_dir}/*.o"
$CPPFLAGS += " -I#{ebb_dir}"
$defs << "-DRUBY_VERSION_CODE=#{RUBY_VERSION.gsub(/\D/, '')}"


create_makefile("ebb_ext")
