#!/usr/bin/env ruby

require "open3"
require "tempfile"

# Test Utilities
$tests = 0
$test_description = 0

# Setup
$systmp = Dir.mktmpdir
at_exit { FileUtils.remove_dir $systmp }

ENV['PATH'] = "#{Dir.pwd}/../:#{ENV['PATH']}"

def try(descr)
    start = Time.now
    $tests += 1
    $test_description = descr
    yield
    delta = "%.3f" % (Time.now - start)
    delta = "\e[37m#{delta}\e[39m"
    puts "#{delta}: #{descr}"
end

def eq(a, b)
    _a = "#{a}".gsub /^/, "> "
    _b = "#{b}".gsub /^/, "< "
    raise "\"#{$test_description}\"\n#{_a}\n#{_b}" unless b === a
end

puts "\e[32m---\e[39m"

# Install or update utilities

try "Install a missing file" do
    dst = "#{$systmp}/_rutils/whereami"
    cmd = "./copyfile input/whereami.sh #{dst}"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, "rset: initialized directory '#{$systmp}/_rutils'\n"
    eq status.success?, true
    eq File.stat(dst).mode.to_s(8), '100755'
    eq File.stat(File.dirname(dst)).mode.to_s(8), '40750'
end

try "Update an existing file" do
    dst = "#{$systmp}/_rutils/whereami"
    cmd = "./copyfile input/whereami.sh #{dst}"
    FileUtils.touch dst, :mtime => 0
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, "rset: updating '#{dst}'\n"
    eq status.success?, true
    eq File.stat(dst).mode.to_s(8), '100755'
    eq File.stat(File.dirname(dst)).mode.to_s(8), '40750'
end

# Execution functions

try "Try a simple pipe" do
    cmd = "./pipe input/whereami.sh"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq status.success?, true
    eq out, File.read('input/whereami.sh')
end

try "Locate an executable in the current path" do
    cmd = "./which sh"
    out, err, status = Open3.capture3({"PATH"=>"/bin:/usr/bin"}, cmd)
    eq err, ""
    eq status.success?, true
    eq out, "/bin/sh\n"
end

try "Start an ssh session" do
    cmd = "./ssh_command S 10.0.0.99"
    out, err, status = Open3.capture3({"PATH"=>"#{Dir.pwd}/stubs"}, cmd)
    eq err, ""
    eq status.success?, true
    eq out, <<~RESULT
        ssh -fN -R 6000:localhost:6000 -S /tmp/test_rset_socket -M networking
        ssh -S /tmp/test_rset_socket networking mkdir /tmp/rset_staging_6000
        ssh -q -S /tmp/test_rset_socket networking tar -xf - -C /tmp/rset_staging_6000
    RESULT
end

try "Execute commands over ssh using a pipe" do
    cmd = "./ssh_command P 10.0.0.98"
    out, err, status = Open3.capture3({"PATH"=>"#{Dir.pwd}/stubs"}, cmd)
    eq err, ""
    eq status.success?, true
    eq out, <<~RESULT
        ssh -T -S /tmp/test_rset_socket 10.0.0.98  sh -c "cd /tmp/rset_staging_6000; LABEL='networking' ROUTE_LABEL='10.0.0.98' INSTALL_URL='http://127.0.0.1:6000' exec /bin/sh"
    RESULT
end

try "Execute commands over ssh using a tty" do
    cmd = "./ssh_command T 10.0.0.99"
    out, err, status = Open3.capture3({"PATH"=>"#{Dir.pwd}/stubs"}, cmd)
    eq err, ""
    eq status.success?, true
    eq out, <<~RESULT
        ssh -T -S /tmp/test_rset_socket 10.0.0.99 cat > /tmp/rset_staging_6000/_script
        ssh -t -S /tmp/test_rset_socket 10.0.0.99  sh -c "cd /tmp/rset_staging_6000; LABEL='networking' ROUTE_LABEL='10.0.0.99' INSTALL_URL='http://127.0.0.1:6000' exec /bin/sh /tmp/rset_staging_6000/_script"
    RESULT
end

try "End an ssh session" do
    FileUtils.touch "/tmp/test_rset_socket"
    cmd = "./ssh_command E 10.0.0.99"
    out, err, status = Open3.capture3({"PATH"=>"#{Dir.pwd}/stubs"}, cmd)
    eq err, ""
    eq status.success?, true
    eq out, <<~RESULT
         ssh -S /tmp/test_rset_socket 10.0.0.99 rm -rf /tmp/rset_staging_6000
         ssh -q -S /tmp/test_rset_socket -O exit 10.0.0.99
    RESULT
    File.unlink "/tmp/test_rset_socket"
end

# Smoke test

try "Run rset with no arguments" do
    cmd = "../rset"
    out, err, status = Open3.capture3(cmd)
    eq err.gsub(/release: (\d\.\d)/, "release: 0.0"),
        "release: 0.0\n" +
        "usage: rset [-lntv] [-F sshconfig_file] [-f routes_file] [-x label_pattern] hostname ...\n"
    eq status.success?, false
end

# Parsing Progressive Label Notation (.pln)

try "Try parsing a label file" do
    cmd = "./parser H input/t430s.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, File.read('expected/t430s.out')
    eq status.success?, true
end

try "Try parsing routes as if they were a host file" do
    cmd = "./parser H input/routes.pln"
    out, err, status = Open3.capture3(cmd)
    eq out, File.read('expected/routes.out')
    eq err, ""
    eq status.success?, true
end

try "Recursively parse routes and hosts" do
    cmd = "./parser R input/routes.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq status.success?, true
    eq out, File.read('expected/recursive.out')
end

try "Format and run a command line" do
    cmd = "./args"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq status.success?, true
    eq out, File.read('expected/args.out')
end

# Error Handling

try "Report an unknown syntax" do
    fn = "#{$systmp}/routes.pln"
    FileUtils.mkdir_p("#{$systmp}/_sources")
    File.open(fn, 'w') { |f| f.write("php\n") }
    cmd = "#{Dir.pwd}/../rset -ln localhost"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "routes.pln: unknown symbol at line 1: 'php'\n"
    eq status.success?, false
    eq out, ""
end

try "Report a bad regex" do
    fn = "#{$systmp}/routes.pln"
    File.open(fn, 'w') { |f| f.write("") }
    cmd = "#{Dir.pwd}/../rset -ln -x 't[42' localhost"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err[0..20], "rset: bad expression:"
    eq status.success?, false
    eq out, ""
end

try "Report an unknown option" do
    fn = "#{$systmp}/routes.pln"
    File.open(fn, 'w') { |f| f.write("username=radman\n") }
    cmd = "#{Dir.pwd}/../rset -ln 't[42'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "routes.pln: unknown option 'username=radman'\n"
    eq status.success?, false
    eq out, ""
end

# Dry Run

try "Show matching routes and hosts" do
    fn = "#{$systmp}/routes.pln"
    FileUtils.mkdir_p("#{$systmp}/_sources")
    out, err, status = nil
    cmd = "#{Dir.pwd}/../rset -ln t430s"
    Dir.chdir("input") do
        out, err, status = Open3.capture3(cmd)
    end
    eq err, ""
    eq status.success?, true
    eq out, File.read('expected/dry_run.out')
end

try "Raise error if no route match is found" do
    fn = "#{$systmp}/routes.pln"
    FileUtils.mkdir_p("#{$systmp}/_sources")
    out, err, status = nil
    cmd = "#{Dir.pwd}/../rset -n localhost.xyz"
    Dir.chdir("input") do
        out, err, status = Open3.capture3(cmd)
    end
    eq status.success?, false
    eq err, "No match for 'localhost.xyz' in routes.pln\n"
end
