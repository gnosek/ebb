class SimpleApp
  @@responses = {}
  @@count = 0
  def call(env)
    command = env['PATH_INFO'].split('/').last
    if command == "test_post_length"
      input_body = ""
      while chunk = env['rack.input'].read(10)
        input_body << chunk 
      end
      if env['HTTP_CONTENT_LENGTH'].to_i == input_body.length
        body = "Content-Length matches input length"
        status = 200
      else
        body = "Content-Length doesn't matches input length! 
          content_length = #{env['HTTP_CONTENT_LENGTH'].to_i}
          input_body.length = #{input_body.length}"
        status = 500
      end
    elsif command =~ /periodically_slow_(\d+)$/
      if @@count % 10 == 0
        seconds = $1.to_i
        #puts "sleeping #{seconds}"
        sleep seconds
        #puts "done"
      else
        seconds = 0
      end
      @@count += 1
      body = "waited #{seconds} seconds"
    elsif command =~ /slow_(\d+)$/
      seconds = $1.to_i
      #puts "sleeping #{seconds}"
      sleep seconds
      #puts "done"
      status = 200
      body = "waited #{seconds} seconds"
    elsif command.to_i > 0
      size = command.to_i
      @@responses[size] ||= "C" * size
      body = @@responses[size]
      status = 200
    else
      status = 404
      body = "Undefined url"
    end
    [status, {'Content-Type' => 'text/plain'}, body + "\r\n\r\n"]
  end
end
