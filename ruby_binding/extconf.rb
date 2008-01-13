require 'mkmf'

dir_config("ebb_ext")
have_library("c", "main")
# need to test for having the ebb library and header?

create_makefile("ebb_ext")
