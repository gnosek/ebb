# supply the benchmark dump file as an argumetn to this program
require 'rubygems'
require 'google_chart'

class Array
  def avg
    sum.to_f / length
  end
  def sum
    inject(0) { |i, s| s += i }
  end
end

colors = [
  '99dddd',
  'dd99dd',
  'dddd99'
].sort_by { rand }

results = Marshal.load(File.read(ARGV[0]))

chart = GoogleChart::LineChart.new('500x300', [Time.now.strftime('%Y.%m.%d'), "requests per second", "#{results.length} data points"].join(','), true)
servers = results.map {|r| r[:server] }.uniq
servers.each do |server|
  server_data = results.find_all { |r| r[:server] == server }
  concurrencies = server_data.map { |d| d[:concurrency] }.uniq
  data = []
  concurrencies.each do |c|
    measurements = server_data.find_all { |d| d[:concurrency] == c }.map { |d| d[:rps] }
    data << [c, measurements.avg]
  end
  chart.data(server, data.sort, colors.shift)
end
chart.axis(:x, :range => [0,100])
chart.axis(:x, :labels => ['concurrency'], :positions => [50])
puts chart.to_url