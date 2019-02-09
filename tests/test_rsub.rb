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
        "usage: rsub [-A] -r line_regex -l line_text target\n" +
        "usage: rsub target < block_content\n"
    eq status.success?, false
end

# Functional tests - line mode

try "Replace a line, optionally append" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub -A -r 'a=[0-9]' -l 'a=5' #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""),
        "@@ -1,2 +1,2 @@\n" \
        "-a=2\n" \
        "+a=5\n" \
        " b=3\n"
    eq status.success?, true
end

try "Handle input containing special characters" do
    fn = "test ~!@()_+ #{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a = 2\nb = 3\n") }
    cmd = "#{Dir.pwd}/../rsub -r 'a = [0-9]' -l 'a = 5' '#{dst}'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""),
        "@@ -1,2 +1,2 @@\n" \
        "-a = 2\n" \
        "+a = 5\n" \
        " b = 3\n"
    eq status.success?, true
end

try "Append a line" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub -A -r 'c=[0-9]' -l 'c=5' #{dst}"
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
    cmd = "#{Dir.pwd}/../rsub -r 'a=[0-9]' -l 'a=2' #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out, ""
    eq status.exitstatus, 1
end

try "Unable to open file" do
    dst = "/bogus/test.txt"
    cmd = "#{Dir.pwd}/../rsub -A -r xx -l yy #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "rsub: file not found: /bogus/test.txt\n"
    eq out, ""
    eq status.exitstatus, 3
end

# Functional tests - block mode

try "Add a new a block" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("listen_address = '*'\n") }
    cmd = <<~CMD
        #{Dir.pwd}/../rsub #{dst} <<EOF
        log_min_duration_statement = 20
        log_hostname = on
        EOF
    CMD
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""), <<~RESULT
        @@ -1 +1,5 @@
         listen_address = '*'
        +# start managed block
        +log_min_duration_statement = 20
        +log_hostname = on
        +# end managed block
    RESULT
    eq status.success?, true
end

try "Update a block" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f|
        f.write(<<~CONTENTS)
            listen_address = '*'
            # start managed block
            log_hostname = on
            # end managed block
        CONTENTS
    }
    cmd = <<~CMD
        #{Dir.pwd}/../rsub #{dst} <<EOF
        log_min_duration_statement = 20
        log_hostname = on
        EOF
    CMD
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""), <<~RESULT
        @@ -1,4 +1,5 @@
         listen_address = '*'
         # start managed block
        +log_min_duration_statement = 20
         log_hostname = on
         # end managed block
    RESULT
    eq status.success?, true
end

try "Ensure that a relative target cannot be used" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    File.open(dst, 'w') { |f| f.write("a=2\nb=3\n") }
    cmd = "#{Dir.pwd}/../rsub -A -r 'a=[0-9]' -l 'a=5' #{fn}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "rsub: #{fn} is not an absolute path\n"
    eq out, ""
    eq status.exitstatus, 1
end
