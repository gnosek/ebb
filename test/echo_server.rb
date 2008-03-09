require 'rubygems'
require 'json'
require  File.dirname(__FILE__) + '/../ruby_lib/ebb'


class EchoApp
  def call(env)
    env['rack.input'] = env['rack.input'].read(1000000)
    env.delete('rack.errors')
    [200, {'Content-Type' => 'text/json'}, env.to_json]
  end
end


server = Ebb::start_server(EchoApp.new, :port => 4037)
