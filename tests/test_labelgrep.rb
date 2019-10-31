#!/usr/bin/env ruby

require 'open3'

# Test Utilities
$tests = 0
$test_description = 0

# Setup

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

puts "\e[32m---\e[39m"

# Smoke test

try "Run labelgrep without enough arguments" do
    cmd = "../labelgrep install"
    out, err, status = Open3.capture3(cmd)
    eq err.gsub(/release: (\d\.\d)/, "release: 0.0"),
        "release: 0.0\n" +
        "usage: labelgrep pattern file.pln [...]\n"
    eq out, ""
    eq status.success?, false
end

# Functional tests

try "Scan multiple files with more than one label" do
    cmd = "../labelgrep 'pkg_add.' input/t430s.pln input/common/openbsd.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    expected = <<~EOF
    input/t430s.pln ([36mcommon packages[0m)
    [33m6[0m	[4mpkg_add [0mrsync-- ruby%2.6
    input/t430s.pln ([36mdesktop[0m)
    [33m16[0m	[4mpkg_add [0mhermit-font vim--gtk2
    EOF
    eq out, expected
    eq status.success?, true
end

try "Find more than one match per label" do
    cmd = "../labelgrep '/etc/hostname' input/t430s.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    expected = <<~EOF
    input/t430s.pln ([36mcommon packages[0m)
    [33m7[0m	echo "inet 172.16.0.1/16" > [4m/etc/hostname[0m.vether0
    [33m8[0m	echo "add vether0" > [4m/etc/hostname[0m.bridge0
    EOF
    eq out, expected
    eq status.success?, true
end
