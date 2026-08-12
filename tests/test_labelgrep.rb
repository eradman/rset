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

# Functional tests

try 'Scan multiple files with more than one label' do
  cmd = "../labelgrep 'pkg_add.' input/t460s.pln input/common/openbsd.pln"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  expected = <<~OUTPUT
    input/t460s.pln (\e[36mcommon packages\e[0m)
    \e[33m12\e[0m\t\e[4mpkg_add \e[0mrsync-- ruby%3.2
    input/t460s.pln (\e[36mdesktop\e[0m)
    \e[33m24\e[0m\t\e[4mpkg_add \e[0mhermit-font vim--gtk2
  OUTPUT
  eq out, expected
  eq status.success?, true
end

try 'Find more than one match per label' do
  cmd = "../labelgrep '/etc/hostname' input/t460s.pln"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  expected = <<~OUTPUT
    input/t460s.pln (\e[36mcommon packages\e[0m)
    \e[33m13\e[0m\techo "inet 172.16.0.1/16" > \e[4m/etc/hostname\e[0m.vether0
    \e[33m14\e[0m\techo "add vether0" > \e[4m/etc/hostname\e[0m.bridge0
  OUTPUT
  eq out, expected
  eq status.success?, true
end
