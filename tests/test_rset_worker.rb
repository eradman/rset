require 'open3'
require 'tempfile'

# Test Utilities
@tests = 0
@test_description = 0

# Setup
@systmp = Dir.mktmpdir

at_exit do
  FileUtils.remove_dir @systmp
end

ENV['PATH'] = "#{Dir.pwd}/../:#{ENV.fetch('PATH', nil)}"

def try(descr)
  start = Time.now
  @tests += 1
  @test_description = descr
  yield
  delta = format('%.3f', (Time.now - start))
  delta = "\e[37m#{delta}\e[39m"
  puts "#{delta}: #{descr}"
end

def eq(result, expected)
  a = result.to_s.gsub(/^/, '> ')
  b = expected.to_s.gsub(/^/, '< ')
  raise "\"#{@test_description}\"\n#{a}\n#{b}" unless result == expected
end

puts "\e[32m---\e[39m"

# Usage test

try 'Options not compatible with parallel operation' do
  %w[-n -t].each do |option|
    cmd = "../rset -p 2 -o logs #{option} db1 db2 db3"
    _, err, status = Open3.capture3(cmd)
    eq status.success?, false
    eq err.include?('usage: rset'), true
  end
end

# Background execution

try 'Construct worker arguments' do
  cmd = './worker_argv -x etc -o logs -p 4 -v db1'
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  eq out, <<~ARGS
    (4)
    ./worker_argv
    -x
    etc
    -v
  ARGS
end

try 'Construct worker arguments without spaces' do
  cmd = './worker_argv -xetc -ologs -p4 -v db1'
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  eq out, <<~ARGS
    (3)
    ./worker_argv
    -xetc
    -v
  ARGS
end

try 'Capture and log stdout/stderr for two workers' do
  cmd = "./worker_exec #{@systmp} 2 sh -c 'echo one; echo two >&2; sleep 0.1'"
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  lines = out.split "\n"
  lines.each do |fn|
    eq File.read(fn), <<~OUT
      one
      two
    OUT
    File.unlink fn
  end
  eq lines[0].chomp('1'), lines[1].chomp('2')
end

try 'Worker environment variables' do
  cmd = "./worker_exec #{@systmp} 1 /usr/bin/env"
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  log_fn = out.strip
  lines = File.readlines(log_fn).select { |s| s.match '^RSET_' }
  eq lines.join, <<~ENV
    RSET_HOST_CONNECT=%s|%T|HOST_CONNECT|%h|
    RSET_HOST_CONNECT_ERROR=%s|%T|HOST_CONNECT_ERROR|%h|%e
    RSET_LABEL_EXEC_BEGIN=%s|%T|EXEC_BEGIN|%l|
    RSET_LABEL_EXEC_END=%s|%T|EXEC_END|%l|%e
    RSET_LABEL_EXEC_ERROR=%s|%T|EXEC_ERROR|%l|%e
    RSET_HOST_DISCONNECT=%s|%T|HOST_DISCONNECT|%h|%e
  ENV
  File.unlink log_fn
end

# Log parsing

try 'Summarize worker logs' do
  cmd = '../rexec-summary input/worker.log.1 input/worker.log.2 /dev/null | sort -k2'
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  eq out, <<~ARGS
    21e00595 172.16.0.2  connect fail >> input/worker.log.1
    be5d3572 172.16.0.3  5/6 complete >> input/worker.log.2
    cd4f00ca 172.16.0.4  6/6 complete >> input/worker.log.1
  ARGS
end

try 'Summarize worker logs and overwrite' do
  cmd = '../rexec-summary input/worker.log.1 input/worker.log.2'
  out, err, status = Open3.capture3(cmd)
  eq status.success?, true
  eq err, ''
  eq out.split("\n").count, 4
  eq out.split[-1].dump, '"\e[3A"'
end
