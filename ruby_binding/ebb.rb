module Ebb
  class Server
    def initialize(host, port, app)
      @host       = host
      @port       = port.to_i
      @app        = app
      @timeout    = 60 # sec
    end
    
    def start
      
    end
    
    def stop
    
    end
  end
end