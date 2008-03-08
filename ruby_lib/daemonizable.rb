# Simplified version of Thin::Daemonizable by Marc-André Cournoyer

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

module Process
  # Returns +true+ the process identied by +pid+ is running.
  def running?(pid)
    Process.getpgid(pid) != -1
  rescue Errno::ESRCH
    false
  end
  module_function :running?
end

# Moadule included in classes that can be turned into a daemon.
# Handle stuff like:
# * storing the PID in a file
# * redirecting output to the log file
# * killing the process gracefully
module Daemonizable
  attr_accessor :pid_file, :log_file, :timeout

  def self.included(base)
    base.extend(ClassMethods)
  end  

  def pid
    File.exist?(pid_file) ? open(pid_file).read : nil
  end
  
  # Turns the current script into a daemon process that detaches from the console.
  def daemonize
    raise ArgumentError, 'You must specify a pid_file to deamonize' unless @pid_file
    
    pwd = Dir.pwd # Current directory is changed during daemonization, so store it
    Kernel.daemonize 
    Dir.chdir pwd
    
    trap('HUP', 'IGNORE') # Don't die upon logout
    
    # Redirect output to the logfile
    [STDOUT, STDERR].each { |f| f.reopen @log_file, 'a' } if @log_file
    
    write_pid_file
    at_exit do
      log ">> Exiting!"
      remove_pid_file
    end
  end
  
  module ClassMethods
    # Kill the process which PID is stored in +pid_file+.
    def kill(pid_file, timeout=60)
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
  end

  private
  
  def remove_pid_file
    File.delete(@pid_file) if @pid_file && File.exists?(@pid_file) && Process.pid == File.read(@pid_file)
  end
  
  def write_pid_file
    log ">> Writing PID to #{@pid_file}"
    open(@pid_file,"w+") { |f| f.write(Process.pid) }
    File.chmod(0644, @pid_file)
  end
end
