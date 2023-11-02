#!/usr/bin/env ruby

require 'open3'

# Test Utilities

@tests = 0
@test_description = 0

# Setup

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

# Functional tests

try 'Filter environment variables' do
  cmd = '../renv'
  input = <<~IN
    # default.env
    set -e
    SD="$PWD"
    A="B=D"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    A="B=D"
  OUT
  eq status.success?, true
end

try 'Integrate environment variables from argv' do
  cmd = %q{../renv 'XY="A" WZ="B"'}
  input = <<~IN
    SD="$PWD"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    XY="A"
    WZ="B"
  OUT
  eq status.success?, true
end

try 'Disallow shell subsitution' do
  cmd = '../renv'
  input = <<~'IN'
    SD1="$PWD"
    SD2="`pwd`"
    SD3="$(pwd)"
    SD4="\$PWD"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD1="$PWD"
  OUT
  eq status.success?, true
end
