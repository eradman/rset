#!/usr/bin/env ruby

require 'open3'
require 'tempfile'

# Test Utilities
$tests = 0
$test_description = 0

# Setup
$systmp = Dir.mktmpdir
at_exit { FileUtils.remove_dir $systmp }

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

try "Run rsub with no arguments" do
    cmd = "../rsub"
    out, err, status = Open3.capture3(cmd)
    eq err.gsub(/release: (\d\.\d)/, "release: 0.0"),
        "release: 0.0\n" +
        "usage: rsub [-A] file pattern text\n"
    eq status.success?, false
end

# Functional tests

try "Replace a line, optionally append" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub -A #{dst} 'a=[0-9]' 'a=5'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""),
        "@@ -1,2 +1,2 @@\n" \
        "-a=2\n" \
        "+a=5\n" \
        " b=3\n"
    eq status.success?, true
end

try "Append a line" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub -A #{dst} 'c=[0-9]' 'c=5'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""),
        "@@ -1,2 +1,3 @@\n" \
        " a=2\n" \
        " b=3\n" \
        "+c=5\n"
    eq status.success?, true
end

try "No change" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub #{dst} 'a=[0-9]' 'a=2'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out, ""
    eq status.exitstatus, 1
end

try "Unable to open file" do
    dst = "/bogus/test.txt"
    cmd = "#{Dir.pwd}/../rsub -A #{dst} 'c=[0-9]' 'c=5'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "rsub: file not found: /bogus/test.txt\n"
    eq out, ""
    eq status.exitstatus, 3
end
