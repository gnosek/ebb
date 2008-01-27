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

colors = %w{F74343 444130 7DA478 E4AC3D}.sort_by { rand }

results = ServerTestResults.open(ARGV[0])
all_m = []
chart = GoogleChart::LineChart.new('400x300', [Time.now.strftime('%Y.%m.%d'), "requests per second", "#{results.length} data points"].join(','), true)
results.servers.each do |server|

  chart.data(server, results.data(server, :size).sort, colors.shift)
end
#chart.axis(:x, :range => [0,100])
chart.axis(:x, :labels => ['kb'], :positions => [50])
puts chart.to_url
