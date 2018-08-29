#!/usr/bin/env ruby

require 'open3'
require 'tempfile'
require 'webrick'

# Test Utilities
$tests = 0
$test_description = 0

# Setup
$systmp = `mktemp -d /tmp/XXXXXX`.chomp
http_port = `./getsocket`.chomp
$install_url = "http://127.0.0.1:#{http_port}"
$wwwserver = fork do
    WEBrick::HTTPServer.new(
        :BindAddress => "localhost",
        :Port => http_port,
        :DocumentRoot => $systmp,
        :Logger => WEBrick::Log.new("/dev/null"),
        :AccessLog => []
    ).start
end
at_exit { `rm -r #{$systmp}`; Process.kill(15, $wwwserver); }
# wait for web server to initialize
sleep 0.1

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
    _a = "#{a}".gsub /^/, "\e[33m> "
    _b = "#{b}".gsub /^/, "\e[36m< "
    raise "\"#{$test_description}\"\n#{_a}\e[39m#{_b}\e[39m" unless b === a
end

def write_file(fn, contents)
    f = File.new("#{$systmp}/" + fn, "w")
    f.write contents
    f.close
end

$usage_text = \
        "release: 0.6\n" +
        "usage: rinstall [-m mode] [-o owner]\n" +
        "                source target\n"

# Smoke test

try "Run rinstall with no arguments" do
    cmd = "../rinstall"
    out, err, status = Open3.capture3(cmd)
    eq err, $usage_text
    eq status.success?, false
end

# Functional tests

try "Install a file from a remote URL" do
    dst = $systmp + "/dst/test2.txt"
    Dir.mkdir "#{$systmp}/dst"
    write_file("test.txt", "123")
    cmd = "INSTALL_URL=#{$install_url} ../rinstall -m 644 test.txt #{dst}"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, ""
    eq status.success?, true
    eq "123", File.read(dst)
    eq File.stat(dst).mode.to_s(8), '100644'
end

try "Install a file from a remote URL to the staging area" do
    dst = $systmp + "/test3.txt"
    Dir.mkdir "#{$systmp}/src"
    write_file("src/test.txt", "678")
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 664 src/test.txt #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out, ""
    eq status.success?, true
    eq File.stat(dst).mode.to_s(8), '100664'
    eq "678", File.read(dst)
end

try "Try to fetch a file that does not exist" do
    dst = $systmp + "/dst/my.txt"
    write_file("test.txt", "123")
    cmd = "INSTALL_URL=#{$install_url} ../rinstall -m 644 bogus.txt #{dst}"
    out, err, status = Open3.capture3(cmd)
    eq status.exitstatus, 3
    eq err.split(/\n/)[-1], "Error fetching #{$install_url}/bogus.txt"
    eq out, ""
    eq File.exists?(dst), false
end
