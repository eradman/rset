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
    SD="$PWD"
    X="width"
    Y="height"

  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    X="width"
    Y="height"
  OUT
  eq status.success?, true
end

try 'Handle unknown variable name' do
  cmd = '../renv'
  input = <<~'IN'
    ~TMP="/tmp"
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, %(renv: unknown pattern: ~TMP="/tmp"\n)
  eq out, ''
  eq status.success?, false
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
  eq err, %(renv: subshells not permitted: SD2="`pwd`"\n)
  eq out, %(SD1="$PWD"\n)
  eq status.success?, false
end

try 'Quote malformed lines' do
  cmd = '../renv'
  # rubocop:disable Layout/TrailingWhitespace
  input = <<~'IN'
    SD=""$PWD""  
    DS=""
    X=""width"" Y=height Z="height"
  IN
  # rubocop:enable Layout/TrailingWhitespace
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, ''
  eq out, <<~OUT
    SD="$PWD"
    DS=""
    X="width Y=height Z=height"
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

try 'Print only environment errors' do
  cmd = '../renv - -q'
  input = <<~'IN'
    SD="$$PWD"
    DS=
  IN
  out, err, status = Open3.capture3(cmd, stdin_data: input)
  eq err, "renv: unknown pattern: DS=\n"
  eq out, ''
  eq status.exitstatus, 1
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
