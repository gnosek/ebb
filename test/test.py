import sys
sys.path.append('/Users/ry/projects/ebb/build/lib.macosx-10.3-ppc-2.5')
import ebb
# os.path.abspath(__file__)
print "hello"

def simple_app(environ, start_response):
  """Simplest possible application object"""
  status = '200 OK'
  response_headers = [('Content-type','text/plain')]
  start_response(status, response_headers)
  return ["Hello world!\n", "Something else!", "blllllaaaaaaaah\n"]

ebb.start_server(simple_app, 4001)