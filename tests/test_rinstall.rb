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

def try(descr)
    start = Time.now
    $tests += 1
    $test_description = descr
    yield
    delta = "%.3f" % (Time.now - start)
    # highlight slow tests
    delta = "\e[7m#{delta}\e[27m" if (Time.now - start) > 0.1
    puts "#{delta}: #{descr}"
end

def eq(a, b)
    _a = "#{a}".gsub /^/, "\e[33m> "
    _b = "#{b}".gsub /^/, "\e[36m< "
    raise "\"#{$test_description}\"\n#{_a}\e[39m#{_b}\e[39m" unless b === a
end

def write_file(fn, contents)
    f = File.new("#{$systmp}/" + fn, "w")
    f.write "123"
    f.close
end

$usage_text = \
        "release: 0.1\n" +
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

try "install a file from a remote URL" do
    dst = $systmp + "/dst/test2.txt"
    Dir.mkdir "#{$systmp}/dst/"
    write_file("test.txt", "123")
    cmd = "/usr/bin/env INSTALL_URL=#{$install_url} ../rinstall test.txt #{dst}"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, ""
    eq status.success?, true
    eq "123", File.read(dst)
    eq File.stat(dst).mode.to_s(8), '100644'
end

puts "\e[32m---\e[0m"
