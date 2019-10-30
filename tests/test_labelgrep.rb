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

try "Scan files for a pattern" do
    cmd = "../labelgrep 'pkg_add.' input/t420s.pln input/common/openbsd.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    expected = <<~EOF
    input/t420s.pln ([36mcommon packages[0m)
    [33m6[0m	[4mpkg_add [0mrsync-- vpnc noto-fonts hermit-font anonymous-pro vim--gtk2 ruby%2.4 heimdal
    EOF
    eq out, expected
    eq status.success?, true
end
