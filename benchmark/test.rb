require  File.dirname(__FILE__) + '/../ruby_lib/ebb'
require File.dirname(__FILE__) + '/application'



server = Ebb::Server.new(SimpleApp.new, {:Port => 4001})
server.start


# class EbbTest < Test::Unit::TestCase
#   
#   def get(path)
#     Net::HTTP.get_response(URI.parse("http://0.0.0.0:4001#{path}"))
#   end
#   
#   def test_get
#     response = get('/hello')
#     eval response.body
#   end
# end

