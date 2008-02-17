# supply the benchmark dump file as an argumetn to this program
require 'rubygems'
require 'google_chart'
require 'server_test'

class Array
  def avg
    sum.to_f / length
  end
  def sum
    inject(0) { |i, s| s += i }
  end
end

colors = %w{F74343 444130 7DA478 E4AC3D}
max_x = 0
max_y = 0
results = ServerTestResults.open(ARGV[0] || 'response_results.dump')
all_m = []
response_chart = GoogleChart::LineChart.new('400x300', Time.now.strftime('%Y.%m.%d, c=50'), true)
results.servers.each do |server|
  data = results.data(server, :size).sort.map { |d| [d[0]/1024.0, d[1]] }
  response_chart.data(server, data, colors.shift)
  x = data.map { |d| d[0] }.max 
  y = data.map { |d| d[1] }.max 
  max_x = x if x > max_x
  max_y = y if y > max_y
end
response_chart.axis(:y, :range => [0,max_y])
response_chart.axis(:y, :labels => ['req/s'], :positions => [50])
response_chart.axis(:x, :range => [0,max_x])
response_chart.axis(:x, :labels => ['kilobytes served'], :positions => [50])
puts response_chart.to_url
