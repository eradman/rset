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
  delta = format('%.3f', Time.now - start)
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

try 'Missing command' do
  cmd = '../rset --'
  _, err, status = Open3.capture3(cmd)
  eq err.include?('usage: rset'), true
  eq status.success?, false
end

try 'Invalid command' do
  ['shell', 'wenv wenv'].each do |option|
    cmd = "../rset -- #{option}"
    _, err, status = Open3.capture3(cmd)
    eq err.include?('rset: unknown command:'), true
    eq status.success?, false
  end
end

# Commands

try 'Print shell environment compatible with eval' do
  cmd = '../rset -- wenv'
  out, _, status = Open3.capture3(cmd)
  eq out, <<~SHELL
    unset HTTP_TRACE;
    unset SSH_TRACE;
    export RSET_HOST_CONNECT="%s|%T|HOST_CONNECT|%h|";
    export RSET_HOST_CONNECT_ERROR="%s|%T|HOST_CONNECT_ERROR|%h|%e";
    export RSET_LABEL_EXEC_BEGIN="%s|%T|EXEC_BEGIN|%l|";
    export RSET_LABEL_EXEC_END="%s|%T|EXEC_END|%l|%e";
    export RSET_LABEL_EXEC_ERROR="%s|%T|EXEC_ERROR|%l|%e";
    export RSET_HOST_DISCONNECT="%s|%T|HOST_DISCONNECT|%h|%e";
  SHELL
  eq status.success?, true
end
