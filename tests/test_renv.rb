require 'open3'
require 'tempfile'

# Test Utilities

@tests = 0
@test_description = 0

# Setup
@systmp = Dir.mktmpdir
def try(descr)
  start = Time.now
  @tests += 1
  @test_description = descr
  yield
  delta = format('%.3f', (Time.now - start))
  delta = "\e[37m#{delta}\e[39m"
  puts "#{delta}: #{descr}"
end
at_exit do
  FileUtils.remove_dir @systmp
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
    X="width"   Y="height" Z="depth"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    X="width"
    Y="height"
    Z="depth"
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

try 'Filter malformed lines' do
  cmd = '../renv'
  input = <<~'IN'
    SD=""$PWD""  
    DS=""
    X=""width"" Y=height Z="height"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    DS=""
    X="width"
  OUT
  eq status.success?, true
end

try 'Escape literals' do
  cmd = '../renv'
  input = <<~'IN'
    SD="$$PWD"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~'OUT'
    SD="\$PWD"
  OUT
  eq status.success?, true
end

try 'Save environment variable' do
  dst = "#{@systmp}/local.env"
  cmd = "../renv xyz=123 #{dst}"
  out, err, status = Open3.capture3({ 'SD' => '..' }, cmd)
  eq err, ''
  eq out, ''
  eq status.success?, true
  cmd = "../renv HTDOCS='/var/www/htdocs' #{dst}"
  out, err, status = Open3.capture3({ 'SD' => '..' }, cmd)
  eq err, ''
  eq out, ''
  eq status.success?, true
  eq File.read(dst), <<~LOCALENV
    xyz="123"
    HTDOCS="/var/www/htdocs"
  LOCALENV
end
