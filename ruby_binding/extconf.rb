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
puts "Building Ebb"
ebb_makefile = File.read("#{here}/../src/Makefile")
ebb_makefile.gsub!('CFLAGS = ', "CFLAGS = #{$CFLAGS} ")
ebb_makefile.gsub!('LIBS = ', "LIBS = #{$LDFLAGS} ")
makefile_filename = "#{here}/../src/.ruby_generated_makefile"
File.open(makefile_filename, "w+") do |f|
  f.write ebb_makefile
end
puts %x{ cd #{here}/../src && make -f #{makefile_filename}}



# Help.. what's the proper way to do this?
$LOCAL_LIBS = '../src/*.o'
$CPPFLAGS += " -I../src"

create_makefile("ebb_ext")
