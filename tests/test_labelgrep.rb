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

try 'Scan multiple files with more than one label' do
  cmd = "../labelgrep 'pkg_add.' input/t460s.pln input/common/openbsd.pln"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  expected = <<~OUTPUT
    input/t460s.pln ([36mcommon packages[0m)
    [33m10[0m	[4mpkg_add [0mrsync-- ruby%3.2
    input/t460s.pln ([36mdesktop[0m)
    [33m22[0m	[4mpkg_add [0mhermit-font vim--gtk2
  OUTPUT
  eq out, expected
  eq status.success?, true
end

try 'Find more than one match per label' do
  cmd = "../labelgrep '/etc/hostname' input/t460s.pln"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  expected = <<~OUTPUT
    input/t460s.pln ([36mcommon packages[0m)
    [33m11[0m	echo "inet 172.16.0.1/16" > [4m/etc/hostname[0m.vether0
    [33m12[0m	echo "add vether0" > [4m/etc/hostname[0m.bridge0
  OUTPUT
  eq out, expected
  eq status.success?, true
end
