require 'socket'
require 'rubygems'
require 'json'
require 'test/unit'

PORT = 4037

# This test depends on echo_server running at port 4037. I do this so that
# I can run a Python server at that port with a similar application and reuse
# these tests.

def send_request(request_string)
  socket = TCPSocket.new("0.0.0.0", PORT)
  socket.write(request_string)
  lines = []
  out = socket.read(5000000)
  raise "Connection Closed on #{request_string.inspect}" if out.nil?
  out.each_line { |l| lines << l }
  env = JSON.parse(lines.last)
rescue Errno::ECONNREFUSED, Errno::ECONNRESET, Errno::EPIPE
  return :fail
rescue RuntimeError => e
  if e.message =~ /Connection Closed/
    return :fail
  else
    raise e
  end
rescue => e
  puts "unknown exception: #{e.class}"
  raise e
ensure
  socket.close unless socket.nil?
end

def drops_request?(request_string)
  :fail == send_request(request_string)
end

class HttpParserTest < Test::Unit::TestCase
  
  def test_parse_simple
    env = send_request("GET / HTTP/1.1\r\n\r\n")
    
    assert_equal 'HTTP/1.1', env['SERVER_PROTOCOL']
    assert_equal '/', env['REQUEST_PATH']
    assert_equal 'HTTP/1.1', env['HTTP_VERSION']
    assert_equal '/', env['REQUEST_URI']
    assert_equal 'GET', env['REQUEST_METHOD']    
    assert_nil env['FRAGMENT']
    assert_nil env['QUERY_STRING']
    assert_nil env['rack.input']
  end
  
  def test_parse_dumbfuck_headers
    should_be_good = "GET / HTTP/1.1\r\naaaaaaaaaaaaa:++++++++++\r\n\r\n"
    env = send_request(should_be_good)
    assert_equal "++++++++++", env["HTTP_AAAAAAAAAAAAA"]
    assert_nil env['rack.input']
    
    nasty_pound_header = "GET / HTTP/1.1\r\nX-SSL-Bullshit:   -----BEGIN CERTIFICATE-----\r\n\tMIIFbTCCBFWgAwIBAgICH4cwDQYJKoZIhvcNAQEFBQAwcDELMAkGA1UEBhMCVUsx\r\n\tETAPBgNVBAoTCGVTY2llbmNlMRIwEAYDVQQLEwlBdXRob3JpdHkxCzAJBgNVBAMT\r\n\tAkNBMS0wKwYJKoZIhvcNAQkBFh5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMu\r\n\tdWswHhcNMDYwNzI3MTQxMzI4WhcNMDcwNzI3MTQxMzI4WjBbMQswCQYDVQQGEwJV\r\n\tSzERMA8GA1UEChMIZVNjaWVuY2UxEzARBgNVBAsTCk1hbmNoZXN0ZXIxCzAJBgNV\r\n\tBAcTmrsogriqMWLAk1DMRcwFQYDVQQDEw5taWNoYWVsIHBhcmQYJKoZIhvcNAQEB\r\n\tBQADggEPADCCAQoCggEBANPEQBgl1IaKdSS1TbhF3hEXSl72G9J+WC/1R64fAcEF\r\n\tW51rEyFYiIeZGx/BVzwXbeBoNUK41OK65sxGuflMo5gLflbwJtHBRIEKAfVVp3YR\r\n\tgW7cMA/s/XKgL1GEC7rQw8lIZT8RApukCGqOVHSi/F1SiFlPDxuDfmdiNzL31+sL\r\n\t0iwHDdNkGjy5pyBSB8Y79dsSJtCW/iaLB0/n8Sj7HgvvZJ7x0fr+RQjYOUUfrePP\r\n\tu2MSpFyf+9BbC/aXgaZuiCvSR+8Snv3xApQY+fULK/xY8h8Ua51iXoQ5jrgu2SqR\r\n\twgA7BUi3G8LFzMBl8FRCDYGUDy7M6QaHXx1ZWIPWNKsCAwEAAaOCAiQwggIgMAwG\r\n\tA1UdEwEB/wQCMAAwEQYJYIZIAYb4QgEBBAQDAgWgMA4GA1UdDwEB/wQEAwID6DAs\r\n\tBglghkgBhvhCAQ0EHxYdVUsgZS1TY2llbmNlIFVzZXIgQ2VydGlmaWNhdGUwHQYD\r\n\tVR0OBBYEFDTt/sf9PeMaZDHkUIldrDYMNTBZMIGaBgNVHSMEgZIwgY+AFAI4qxGj\r\n\tloCLDdMVKwiljjDastqooXSkcjBwMQswCQYDVQQGEwJVSzERMA8GA1UEChMIZVNj\r\n\taWVuY2UxEjAQBgNVBAsTCUF1dGhvcml0eTELMAkGA1UEAxMCQ0ExLTArBgkqhkiG\r\n\t9w0BCQEWHmNhLW9wZXJhdG9yQGdyaWQtc3VwcG9ydC5hYy51a4IBADApBgNVHRIE\r\n\tIjAggR5jYS1vcGVyYXRvckBncmlkLXN1cHBvcnQuYWMudWswGQYDVR0gBBIwEDAO\r\n\tBgwrBgEEAdkvAQEBAQYwPQYJYIZIAYb4QgEEBDAWLmh0dHA6Ly9jYS5ncmlkLXN1\r\n\tcHBvcnQuYWMudmT4sopwqlBWsvcHViL2NybC9jYWNybC5jcmwwPQYJYIZIAYb4QgEDBDAWLmh0\r\n\tdHA6Ly9jYS5ncmlkLXN1cHBvcnQuYWMudWsvcHViL2NybC9jYWNybC5jcmwwPwYD\r\n\tVR0fBDgwNjA0oDKgMIYuaHR0cDovL2NhLmdyaWQt5hYy51ay9wdWIv\r\n\tY3JsL2NhY3JsLmNybDANBgkqhkiG9w0BAQUFAAOCAQEAS/U4iiooBENGW/Hwmmd3\r\n\tXCy6Zrt08YjKCzGNjorT98g8uGsqYjSxv/hmi0qlnlHs+k/3Iobc3LjS5AMYr5L8\r\n\tUO7OSkgFFlLHQyC9JzPfmLCAugvzEbyv4Olnsr8hbxF1MbKZoQxUZtMVu29wjfXk\r\n\thTeApBv7eaKCWpSp7MCbvgzm74izKhu3vlDk9w6qVrxePfGgpKPqfHiOoGhFnbTK\r\n\twTC6o2xq5y0qZ03JonF7OJspEd3I5zKY3E+ov7/ZhW6DqT8UFvsAdjvQbXyhV8Eu\r\n\tYhixw1aKEPzNjNowuIseVogKOLXxWI5vAi5HgXdS0/ES5gDGsABo4fqovUKlgop3\r\n\tRA==\r\n\t-----END CERTIFICATE-----\r\n\r\n"
    assert drops_request?(nasty_pound_header) # Correct?
  end
  
  def test_parse_error
    assert drops_request?("GET / SsUTF/1.1")
  end

  def test_fragment_in_uri
    env = send_request("GET /forums/1/topics/2375?page=1#posts-17408 HTTP/1.1\r\n\r\n")
    assert_equal '/forums/1/topics/2375?page=1', env['REQUEST_URI']
    assert_equal 'posts-17408', env['FRAGMENT']
    assert_nil env['rack.input']
  end
  
  # lame random garbage maker
  def rand_data(min, max, readable=true)
    count = min + ((rand(max)+1) *10).to_i
    res = count.to_s + "/"
    
    if readable
      res << Digest::SHA1.hexdigest(rand(count * 100).to_s) * (count / 40)
    else
      res << Digest::SHA1.digest(rand(count * 100).to_s) * (count / 20)
    end
    
    return res
  end
  
  def test_horrible_queries
    10.times do |c|
      req = "GET /#{rand_data(10,120)} HTTP/1.1\r\nX-#{rand_data(1024, 1024+(c*1024))}: Test\r\n\r\n"
      assert drops_request?(req), "large header names are caught"
    end
    
    # then that large mangled field values are caught
    10.times do |c|
      req = "GET /#{rand_data(10,120)} HTTP/1.1\r\nX-Test: #{rand_data(1024, 1024+(c*1024), false)}\r\n\r\n"
      assert drops_request?(req), "large mangled field values are caught"
      ### XXX this is broken! fix me. this test should drop the request.
    end
    
    # then large headers are rejected too
    req = "GET /#{rand_data(10,120)} HTTP/1.1\r\n"
    req << "X-Test: test\r\n" * (80 * 1024)
    assert drops_request?(req), "large headers are rejected"
    
    # finally just that random garbage gets blocked all the time
    10.times do |c|
      req = "GET #{rand_data(1024, 1024+(c*1024), false)} #{rand_data(1024, 1024+(c*1024), false)}\r\n\r\n"
      assert drops_request?(req), "random garbage gets blocked all the time"
    end
  end
end
