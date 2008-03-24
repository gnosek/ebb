require File.dirname(__FILE__) + '/helper'

class BasicTestFD < ServerTestFD
  def test_get_bytes
    [1,10,1000].each do |i|
      response = get("/bytes/#{i}")
      assert_equal "#{'C'*i.to_i}", response['output']
    end
  end
  
  def test_get_unknown
    response = get('/blah')
    assert_equal "Undefined url", response['output']
  end
  
  def test_small_posts
    [1,10,321,123,1000].each do |i|
      response = post("/test_post_length", 'C'*i)
      assert_equal 200, response['status']
    end
  end
  
  def test_large_post
    [50,60,100].each do |i|
      response = post("/test_post_length", 'C'*1024*i)
      assert_equal 200, response['status']
    end
  end
end
