require 'open3'
require 'tempfile'
require 'socket'
require 'securerandom'

# Test Utilities
@tests = 0
@test_description = 0

# Setup
@systmp = Dir.mktmpdir
@renv = "#{Dir.pwd}/stubs/renv"
File.write @renv, <<~STUB
  #!/bin/sh
  echo renv $*
STUB
FileUtils.chmod 0o755, @renv

at_exit do
  FileUtils.remove_dir @systmp
  File.unlink @renv
end

ENV['PATH'] = "#{Dir.pwd}/../:#{ENV['PATH']}"

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

# Install or update utilities

try 'Install a missing file' do
  dst = "#{@systmp}/_rutils/whereami"
  cmd = "./copyfile input/whereami.sh #{dst}"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, "rset: initialized directory '#{@systmp}/_rutils'\n"
  eq status.success?, true
  eq File.stat(dst).mode.to_s(8), '100755'
  eq File.stat(File.dirname(dst)).mode.to_s(8), '40750'
end

try 'Update an existing file' do
  dst = "#{@systmp}/_rutils/whereami"
  cmd = "./copyfile input/whereami.sh #{dst}"
  FileUtils.touch dst, mtime: 0
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, "rset: updating '#{dst}'\n"
  eq status.success?, true
  eq File.stat(dst).mode.to_s(8), '100755'
  eq File.stat(File.dirname(dst)).mode.to_s(8), '40750'
end

# Execution functions

try 'Pipe input to a command' do
  cmd = './cmd_pipe_stdin input/whereami.sh'
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq status.success?, true
  eq out, File.read('input/whereami.sh')
end

try 'Capture output of a command' do
  cmd = "./cmd_pipe_stdout head -n1 #{__FILE__}"
  out, err, status = Open3.capture3(cmd)
  eq err, "output_size: 16\nstrlen: 16\n"
  eq status.success?, true
  eq out, "require 'open3'\n"
end

try 'Capture multi-line output from a command' do
  cmd = "./cmd_pipe_stdout tail -c 4096 #{__FILE__}"
  out, err, status = Open3.capture3(cmd)
  eq err, "output_size: 4096\nstrlen: 4096\n"
  eq status.success?, true
  eq out.length, 4096
end

try 'Capture a large chunk of text from a command' do
  tin = "#{@systmp}/random_text_in"
  tout = "#{@systmp}/random_text_out"
  random_s = SecureRandom.alphanumeric(32_768)
  File.open(tin, 'w') { |f| f.write(random_s) }
  cmd = "./cmd_pipe_stdout /bin/cat #{tin} > #{tout}"
  _, err, status = Open3.capture3(cmd)
  eq err, "output_size: 32768\nstrlen: 32768\n"
  eq status.success?, true
  cmd = "diff #{tin} #{tout}"
  _, err, status = Open3.capture3(cmd)
  eq err, ''
  eq status.success?, true
end

try 'Locate an executable in the current path' do
  cmd = './which sh'
  out, err, status = Open3.capture3({ 'PATH' => '/bin:/usr/bin' }, cmd)
  eq err, ''
  eq status.success?, true
  eq out, "/bin/sh\n"
end

try 'Start an ssh session' do
  cmd = './ssh_command S 10.0.0.99'
  out, err, status = Open3.capture3({ 'PATH' => "#{Dir.pwd}/stubs" }, cmd)
  eq err, ''
  eq status.success?, true
  eq out, <<~RESULT
    ssh -fN -R 6000:localhost:6000 -S /tmp/test_rset_socket -M 10.0.0.99
    ssh -S /tmp/test_rset_socket 10.0.0.99 'mkdir /tmp/rset_staging_6000'
    tar -cf - -C _rutils ./
    ssh -q -S /tmp/test_rset_socket 10.0.0.99 tar -xf - -C /tmp/rset_staging_6000
  RESULT
end

try 'Execute commands over ssh using a pipe' do
  cmd = './ssh_command P 10.0.0.98'
  out, err, status = Open3.capture3({ 'PATH' => "#{Dir.pwd}/stubs:#{Dir.pwd}/.." }, cmd)
  eq err, ''
  eq status.success?, true
  eq out, <<~RESULT
    renv /dev/null /tmp/rset_env_XXXXXX
    ssh -q -S /tmp/test_rset_socket 10.0.0.98 'cat > /tmp/rset_staging_6000/final.env; touch /tmp/rset_staging_6000/local.env'
    ssh -T -S /tmp/test_rset_socket 10.0.0.98 ' sh -a -c "cd /tmp/rset_staging_6000; . ./final.env; . ./local.env; SD='/tmp/rset_staging_6000' INSTALL_URL='http://127.0.0.1:6000'; exec /bin/sh"'
  RESULT
end

try 'Execute commands over ssh using a tty' do
  cmd = './ssh_command T 10.0.0.99'
  out, err, status = Open3.capture3({ 'PATH' => "#{Dir.pwd}/stubs:#{Dir.pwd}/.." }, cmd)
  eq err, ''
  eq status.success?, true
  eq out, <<~RESULT
    renv /dev/null /tmp/rset_env_XXXXXX
    ssh -q -S /tmp/test_rset_socket 10.0.0.99 'cat > /tmp/rset_staging_6000/final.env; touch /tmp/rset_staging_6000/local.env'
    ssh -T -S /tmp/test_rset_socket 10.0.0.99 'cat > /tmp/rset_staging_6000/_script'
    ssh -t -S /tmp/test_rset_socket 10.0.0.99 ' sh -a -c "cd /tmp/rset_staging_6000; . ./final.env; . ./local.env; SD='/tmp/rset_staging_6000' INSTALL_URL='http://127.0.0.1:6000'; exec /bin/sh /tmp/rset_staging_6000/_script"'
  RESULT
end

try 'End an ssh session' do
  FileUtils.touch '/tmp/test_rset_socket'
  cmd = './ssh_command E 10.0.0.99'
  out, err, status = Open3.capture3({ 'PATH' => "#{Dir.pwd}/stubs" }, cmd)
  eq err, ''
  eq status.success?, true
  eq out, <<~RESULT
    ssh -S /tmp/test_rset_socket 10.0.0.99 rm -rf /tmp/rset_staging_6000
    ssh -q -S /tmp/test_rset_socket -O exit 10.0.0.99
  RESULT
  File.unlink '/tmp/test_rset_socket'
end

# Smoke test

try 'Run rset with no arguments' do
  cmd = '../rset'
  _, err, status = Open3.capture3(cmd)
  eq err.gsub(/release: (\d\.\d)/, 'release: 0.0'), <<~USAGE
    release: 0.0
    usage: rset [-entv] [-E environment] [-F sshconfig_file] [-f routes_file] [-l option_name] [-x label_pattern] hostname ...
  USAGE
  eq status.success?, false
end

# Parse Progressive Label Notation (pass)

try 'Parse a label file' do
  cmd = './parser H input/t460s.pln'
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, File.read('expected/t460s.out')
  eq status.success?, true
end

try 'Parse a routes file' do
  cmd = './parser H input/routes.pln'
  out, err, status = Open3.capture3(cmd)
  eq out, File.read('expected/routes.out')
  eq err, ''
  eq status.success?, true
end

try 'Recursively parse routes and hosts' do
  cmd = './parser R input/routes.pln'
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq status.success?, true
  eq out, File.read('expected/recursive.out')
end

try 'Format and run a command line' do
  cmd = './args'
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq status.success?, true
  eq out, File.read('expected/args.out')
end

# Parse Progressive Label Notation (fail)

try 'Report an unknown syntax' do
  fn = "#{@systmp}/routes.pln"
  FileUtils.mkdir_p("#{@systmp}/_sources")
  File.open(fn, 'w') { |f| f.write("php\n") }
  cmd = "#{Dir.pwd}/../rset -l interpreter -n localhost"
  out, err, status = Open3.capture3(cmd, chdir: @systmp)
  eq err, "routes.pln: unknown symbol at line 1: 'php'\n"
  eq status.success?, false
  eq out, ''
end

try 'Detect local execution that does not emit a newline' do
  pln = 'input/local_exec_out_01.pln'
  cmd = "./parser H #{pln}"
  out, err, status = Open3.capture3(cmd)
  eq err, "#{pln}: output of local execution for the label 'two' must end with a newline\n"
  eq out, ''
  eq status.exitstatus, 1
end

try 'Report a bad regex' do
  fn = "#{@systmp}/routes.pln"
  File.open(fn, 'w') { |f| f.write('') }
  cmd = "#{Dir.pwd}/../rset -n -x 't[42' localhost"
  out, err, status = Open3.capture3(cmd, chdir: @systmp)
  eq err[0..20], 'rset: bad expression:'
  eq status.success?, false
  eq out, ''
end

try 'Report an unknown option' do
  fn = "#{@systmp}/routes.pln"
  File.open(fn, 'w') { |f| f.write("username=radman\n") }
  cmd = "#{Dir.pwd}/../rset -n -l interpreter 't[42'"
  out, err, status = Open3.capture3(cmd, chdir: @systmp)
  eq err, "routes.pln: unknown option 'username=radman'\n"
  eq status.success?, false
  eq out, ''
end

# Custom Logging

try 'Log start message' do
  cmd = "./log_msg '== Starting on %h at %T =='"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out.gsub(/[0-9]/, '0').sub(/[+-]{1}0{4} /, '+0100 '), <<~RESULT
    == Starting on localhost at 0000-00-00 00:00:00+0100 ==
  RESULT
  eq status.success?, true
end

try 'Log exit code and other characters' do
  cmd = "./log_msg '== Running %l %% (%e) =='"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    == Running network % (2) ==
  RESULT
  eq status.success?, true
end

# Environment

try 'Format environment option on separate lines' do
  cmd = %(./format_env N 'first="one" second="two"')
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    first="one"
    second="two"
  RESULT
  eq status.success?, true
end

try 'No closing quote' do
  cmd = %(./format_env N 'first="one" second="two')
  out, err, status = Open3.capture3(cmd)
  eq err, %(format_env: no closing quote: first="one" second="two\n)
  eq out, ''
  eq status.exitstatus, 1
end

try 'Format environment and verify with renv' do
  cmd = %(./format_env V 'first="one" second="two"')
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    first="one"
    second="two"
  RESULT
  eq status.success?, true
end

try 'Format environment and fail verifications with renv' do
  cmd = %(./format_env V 'HOSTNAME=')
  out, err, status = Open3.capture3(cmd)
  eq err, "renv: unknown pattern: HOSTNAME=\n"
  eq out, ''
  eq status.success?, false
end

# Hostlists

try 'Simple hostlist' do
  cmd = "./hostlist 'web12.dev'"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    (1)
    web12.dev
  RESULT
  eq status.success?, true
end

try 'Expand hostlist' do
  cmd = "./hostlist 'web{8..11}.dev'"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    (4)
    web8.dev
    web9.dev
    web10.dev
    web11.dev
  RESULT
  eq status.success?, true
end

try 'Expand hostlist of IP addresses' do
  cmd = "./hostlist '172.16.{1..2}.2'"
  out, err, status = Open3.capture3(cmd)
  eq err, ''
  eq out, <<~RESULT
    (2)
    172.16.1.2
    172.16.2.2
  RESULT
  eq status.success?, true
end

try 'Multiple hostlist ranges' do
  cmd = "./hostlist 'web{1..2}-{11..8}.dev'"
  out, err, status = Open3.capture3(cmd)
  eq err, "hostlist: maximum of 1 groups\n"
  eq out, ''
  eq status.success?, false
end

try 'Invalid hostlist range' do
  cmd = "./hostlist 'web{9..9}.dev'"
  out, err, status = Open3.capture3(cmd)
  eq err, "hostlist: non-ascending range: 9..9\n"
  eq out, ''
  eq status.success?, false

  cmd = "./hostlist 'web{1..9999}.dev'"
  out, err, status = Open3.capture3(cmd)
  eq err, "hostlist: maximum range exceeds 50\n"
  eq out, ''
  eq status.success?, false
end

# Dry Run

try 'Show matching routes and hosts' do
  out, err, status = nil
  cmd = "#{Dir.pwd}/../rset -l interpreter -n t460s"
  Dir.chdir('input') do
    FileUtils.mkdir_p('_sources')
    out, err, status = Open3.capture3(cmd)
  end
  eq err, ''
  eq status.success?, true
  eq out, File.read('expected/dry_run.out')
end

try 'Raise error if no route match is found' do
  FileUtils.mkdir_p("#{@systmp}/_sources")
  out, err, status = nil
  cmd = "#{Dir.pwd}/../rset -n localhost.xyz"
  Dir.chdir('input') do
    out, err, status = Open3.capture3(cmd)
  end
  eq status.success?, false
  eq err, "rset: No match for 'localhost.xyz' in routes.pln\n"
end
