require  File.dirname(__FILE__) + '/helper'

APP_DIR = File.dirname(__FILE__) + "/rails_app"
EBB_RAILS = "#{Ebb::LIBDIR}/../bin/ebb_rails"
class EbbRailsTest < Test::Unit::TestCase
  # just to make sure there isn't some load error
  def test_version
    out = %x{ruby #{EBB_RAILS} -v}
    assert_match %r{Ebb #{Ebb::VERSION}}, out
  end
  
  def test_parser
    runner = Ebb::Runner::Rails.new
    runner.parse_options("start -c #{APP_DIR} -p #{TEST_PORT}".split)
    assert_equal TEST_PORT, runner.options[:port].to_i
    assert_equal APP_DIR, runner.options[:root]
  end

  
  def test_start_app
    Thread.new do
      runner = Ebb::Runner::Rails.new
      runner.run("start -c #{APP_DIR} -p #{TEST_PORT}".split)
    end
    sleep 0.1 until Ebb.running?
    
    response = get '/'
    assert_equal 200, response.code.to_i
    
  ensure
    Ebb.stop_server
    sleep 0.1 while Ebb.running?
  end
end