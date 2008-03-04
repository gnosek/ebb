#!/usr/bin/env python
from distutils.core import setup
from distutils.extension import Extension
import commands

def pkgconfig(*packages, **kw):
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}
    for token in commands.getoutput("pkg-config --libs --cflags %s" % ' '.join(packages)).split():
        kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
    return kw

ebb = Extension( "ebb"
               , ["src/ebb_python.c", "src/ebb.c", "src/parser.c"]
               , **pkgconfig('glib-2.0', include_dirs = ['libev'])
               )

setup( name = "Ebb"
     , description = "a wsgi web server"
     , version = "0.0.3"
     , author = "ry dahl"
     , author_email = "ry at tiny clouds dot org"
     , url = "http://ebb.rubyforge.org/"
     , ext_modules = [ebb]
     )