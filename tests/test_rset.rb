#!/usr/bin/env ruby

require "open3"
require "tempfile"

# Test Utilities
$tests = 0
$test_description = 0

# Setup
$systmp = `mktemp -d /tmp/XXXXXX`.chomp
at_exit { `rm -r #{$systmp}` }

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

$usage_text = \
        "release: 0.3\n" +
        "usage: rset [-ln] [-f routes_file] host_pattern [label]\n"

# Install or update utilities

try "Install a missing file" do
    dst = $systmp + "/_rutils/whereami"
    cmd = "./copyfile input/whereami.sh #{dst}"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq status.success?, true
    #eq out, File.read('input/whereami.sh')
    eq File.stat(dst).mode.to_s(8), '100755'
    eq File.stat(File.dirname(dst)).mode.to_s(8), '40750'
end

# Execute and pipe to STDIN

try "Try a simple pipe" do
    cmd = "./pipe input/whereami.sh"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq status.success?, true
    eq out, File.read('input/whereami.sh')
end

# Smoke test

try "Run rset with no arguments" do
    cmd = "../rset"
    out, err, status = Open3.capture3(cmd)
    eq err, $usage_text
    eq status.success?, false
end

# Parsing Progressive Label Notation (.pln)

try "Try parsing a label file" do
    cmd = "./parser H input/t420s.pln"
    out, err, status = Open3.capture3(cmd)
    eq err, ""
    eq out, File.read('expected/t420s.out')
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

# Dry Run

try "Show matching routes and hosts" do
    out, err, status = nil
    Dir.chdir("input") do
        cmd = "../../rset -ln t420"
        out, err, status = Open3.capture3(cmd)
    end
    eq err, ""
    eq status.success?, true
    eq out, File.read('expected/dry_run.out')
end

# Error Handling

try "Show matching routes and hosts" do
    out, err, status = nil
    Dir.chdir("input") do
        cmd = "../../rset -ln 't[42'"
        out, err, status = Open3.capture3(cmd)
    end
    eq err, "rset: bad expression: brackets ([ ]) not balanced\n"
    eq status.success?, false
    eq out, ""
end

puts "\e[32m---\e[0m"
