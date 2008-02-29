require 'mkmf'


unless pkg_config('glib-2.0')
  abort "Ebb requires glib-2.0 and pkg_config"
end

flags = []

if have_header('sys/select.h')
  flags << '-DEV_USE_SELECT'
end

if have_header('poll.h')
  flags << '-DEV_USE_POLL'
end

if have_header('sys/epoll.h')
  flags << '-DEV_USE_EPOLL'
end

if have_header('sys/event.h') and have_header('sys/queue.h')
  flags << '-DEV_USE_KQUEUE'
end

if have_header('port.h')
  flags << '-DEV_USE_PORT'
end

if have_header('sys/inotify.h')
  flags << '-DEV_USE_INOTIFY'
end

dir = File.dirname(__FILE__)
libev_dir = File.expand_path(dir + '/../libev')

$LDFLAGS << " -lpthread "
$CFLAGS << " -std=c99 -I#{libev_dir} " << flags.join(' ')
$defs << "-DRUBY_VERSION_CODE=#{RUBY_VERSION.gsub(/\D/, '')}"


dir_config('ebb_ext')
create_makefile('ebb_ext')

