require 'mkmf'

dir_config("ebb")
have_library("c", "main")

create_makefile("ebb")
