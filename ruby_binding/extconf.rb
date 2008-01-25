require 'mkmf'


# need these extra objects: 
dir_config('ev')

unless  have_library('ev', 'ev_version_major') and have_header('ev.h') and
        pkg_config('glib-2.0')
  then
  exit 1
end
# Help.. what's the proper way to do this?
$LOCAL_LIBS = '../ebb.o ../mongrel/parser.o'
$CPPFLAGS += " -I.. -L.. "

create_makefile("ebb_ext")
