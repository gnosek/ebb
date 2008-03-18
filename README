# A Web Server Called *Ebb*

Ebb aims to be a small and fast web server specifically for hosting 
dynamic web applications. It is not meant to be a full featured web server
like Lighttpd, Apache, or Nginx. Rather it should be used in multiplicity
behind a load balancer and a front-end server. It is not meant to serve static files in production.

At one level Ebb is a minimalist C library that ties together the 
[Mongrel state machine](http://mongrel.rubyforge.org/browser/tags/rel_1-0-1/ext/http11/http11_parser.rl) 
and [libev](http://software.schmorp.de/pkg/libev.html) event loop. One can use
this library to drive a web application written in C. (Perhaps for embedded 
devices?) However, most people will be interested in the binding of this
library to the Ruby programming language. The binding provides a
[Rack](http://rack.rubyforge.org/) server interface that allows it to host
Rails, Merb, or other frameworks.

A Python-WSGI binding is under development.

## Install

The Ruby binding is available as a Ruby Gem. It can be install by executing

    gem install ebb

Ebb depends on having glib2 headers and libraries installed. For example, in
Macintosh if one is using Darwin ports then the following should do the trick
  
    port install glib2
  
Downloads are available at
the [RubyForge project page](http://rubyforge.org/frs/?group_id=5640).

## Running

Using the executable `ebb_rails` one can start Ebb with a Rails project. Use
`ebb_rails -h` to see all of the options but to start one can try

    cd my_rails_project/
    ebb_rails start

When using `ebb_rails` from monit, the monitrc entry might look like this:

    check process myApp4000
      with pidfile /home/webuser/myApp/current/tmp/ebb.4000.pid
      start program = "/usr/bin/ruby /usr/bin/ebb_rails start -d -e production -p 4000 -P /home/webuser/myApp/current/tmp/ebb.4000.pid -c /home/webuser/myApp/current" as uid webuser and gid webuser
      stop program = "/usr/bin/ruby /usr/bin/ebb_rails stop -P /home/webuser/myApp/current/tmp/ebb.4000.pid" as uid webuser and gid webuser
      if totalmem > 120.0 MB for 2 cycles then restart
      if loadavg(5min) greater than 10 for 8 cycles then restart
      group myApp

To use Ebb with a different framework you will have to do a small amount of
hacking at the moment! :)

## Speed

Because Ebb-Ruby handles most of the processing in C, it is able to do work
often times more efficiently than other Ruby language web servers.

![Benchmark](http://s3.amazonaws.com/four.livejournal/20080311/ebb.png)

Ebb-Ruby can handle threaded processing better than the other 'evented' 
servers. This won't be of any benefit to Rails applications because Rails
places a lock around each request that wouldn't allow concurrent processing
anyway. In Merb, for example, Ebb's thread handling will allow Ebb instances
to handle larger loads. [More](http://four.livejournal.com/848525.html)

## Contributions

Contributions (patches, criticism, advice) are very welcome! 
Please send all to to 
[the mailing list](http://groups.google.com/group/ebbebb).

The source code
is hosted [github](http://github.com/ry/ebb/tree/master). It can be retrieved 
by executing

    git clone git://github.com/ry/ebb.git

Here are some features that I would like to add:
* HTTP 1.1 Expect/Continue (RFC 2616, sections 8.2.3 and 10.1.1)
* A parser for multipart/form-data
* Optimize and clean up upload handling
* Option to listen on unix sockets instead of TCP
* Python binding

## (The MIT) License

Copyright © 2008 [Ry Dahl](http://tinyclouds.org) (ry at tiny clouds dot org)

<div id="license">
Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
</div>
