require 'optparse'

module Kernel
  unless respond_to? :daemonize # Already part of Ruby 1.9, yeah!
    # Turns the current script into a daemon process that detaches from the console.
    # It can be shut down with a TERM signal. Taken from ActiveSupport.
    def daemonize
      exit if fork                   # Parent exits, child continues.
      Process.setsid                 # Become session leader.
      exit if fork                   # Zap session leader. See [1].
      Dir.chdir "/"                  # Release old working directory.
      File.umask 0000                # Ensure sensible umask. Adjust as needed.
      STDIN.reopen "/dev/null"       # Free file descriptors and
      STDOUT.reopen "/dev/null", "a" # point them somewhere sensible.
      STDERR.reopen STDOUT           # STDOUT/ERR should better go to a logfile.
      trap("TERM") { exit }
    end
  end
end

module Ebb
  class Runner
    DEFAULT_OPTIONS = {
      :port         => 4001,
      :timeout      => 60,
      :workers      => 1
    }
    
    # Kill the process which PID is stored in +pid_file+.
    def self.kill(pid_file, timeout=60)
      raise ArgumentError, 'You must specify a pid_file to stop deamonized server' unless pid_file 
      
      if pid = File.read(pid_file)
        pid = pid.to_i
        
        Process.kill('KILL', pid)
        puts "stopped!"
      else
        puts "Can't stop process, no PID found in #{@pid_file}"
      end
    rescue Errno::ESRCH # No such process
      puts "process not found!"
    ensure
      File.delete(pid_file) rescue nil
    end

    def self.remove_pid_file(file)
      File.delete(file) if file && File.exists?(file) && Process.pid == File.read(file)
    end

    def self.write_pid_file(file)
      puts ">> Writing PID to #{file}"
      open(file,"w+") { |f| f.write(Process.pid) }
      File.chmod(0644, file)
    end
    
    def initialize(name, &definitions)
      @name = name
      instance_eval(&definitions)
      run
    end
    
    def run
      option_parser, options = get_options_from_command_line
      
      
      case ARGV[0]
      when 'start'
        STDOUT.print("Ebb is loading the application...")
        STDOUT.flush()
        @app = app(options)
        STDOUT.puts("loaded")
        
        if options[:daemonize]
          pwd = Dir.pwd # Current directory is changed during daemonization, so store it
          Kernel.daemonize 
          Dir.chdir pwd
          trap('HUP', 'IGNORE') # Don't die upon logout
        end
        
        if options[:log_file]
          [STDOUT, STDERR].each { |f| f.reopen log_file, 'a' } 
        end
        
        if options[:pid_file]
          Runner.write_pid_file(options[:pid_file])
          at_exit do
            puts ">> Exiting!"
            Runner.remove_pid_file(options[:pid_file])
          end
        end
        
        Ebb::start_server(@app, options)
      when 'stop'
        Ebb::Runner.kill options[:pid_file], options[:timeout]
      when nil
        puts "Command required"
        puts option_parser
        exit 1
      else
        abort "Invalid command : #{ARGV[0]}"
      end
    end
    
    
    def get_options_from_command_line
      options = DEFAULT_OPTIONS.dup
      option_parser = OptionParser.new do |parser|
        parser.banner = "Usage: #{@name} [options] start | stop"
        parser.separator ""
        if respond_to?(:add_extra_options)
          add_extra_options(parser, options)
        end
        parser.separator ""
        #  opts.on("-s", "--socket SOCKET", "listen on socket")                 { |socket| options[:socket] = socket }
        parser.on("-p", "--port PORT", "(default: #{options[:port]})") { |options[:port]| }
        parser.on("-d", "--daemonize", "Daemonize") { options[:daemonize] = true }
        parser.on("-l", "--log-file FILE", "File to redirect output") { |options[:port]| }
        parser.on("-P", "--pid-file FILE", "File to store PID") { |options[:pid_file]| }
        parser.on("-t", "--timeout SECONDS", "(default: #{options[:timeout]})") { |options[:timeout]| }
        parser.on("-w", "--workers WORKERS", "Number of worker threads (default: #{options[:workers]})") { |options[:workers]| }
        parser.separator ""
        parser.on_tail("-h", "--help", "Show this message") do
          puts parser
          exit
        end
        parser.on_tail('-v', '--version', "Show version") do
          puts "Ebb #{Ebb::VERSION}"
          exit
        end
      end
      option_parser.parse!(ARGV)
      [option_parser, options]
    end
  end

end
