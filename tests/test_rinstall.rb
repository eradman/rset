#!/usr/bin/env ruby

require 'open3'
require 'tempfile'
require 'webrick'
require 'digest/md5'

# Test Utilities
$tests = 0
$test_description = 0

# Setup
$systmp = Dir.mktmpdir
$wwwtmp = Dir.mktmpdir
http_port = %x{ ./getsocket }.chomp
$install_url = "http://localhost:#{http_port}"
$wwwserver = fork do
    WEBrick::HTTPServer.new(
        :BindAddress => "localhost",
        :Port => http_port,
        :DocumentRoot => $wwwtmp,
        :Logger => WEBrick::Log.new("/dev/null"),
        :AccessLog => []
    ).start
end
at_exit { FileUtils.remove_dir $systmp; FileUtils.remove_dir $wwwtmp; Process.kill(15, $wwwserver); }

# wait for web server to initialize
sleep 0.1

def try(descr, skip=false)
    if skip then
        puts "0.000: #{descr} [skipped]"
        return
    end
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

try "Run rinstall with no arguments" do
    cmd = "../rinstall"
    out, err, status = Open3.capture3(cmd)
    eq err.gsub(/release: (\d\.\d)/, "release: 0.0"),
        "release: 0.0\n" +
        "usage: rinstall [-m mode] [-o owner]\n" +
        "                source target\n"
    eq status.success?, false
end

# Functional tests

try "Install a file from a remote URL to the staging area" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    File.open(src, 'w') { |f| f.write("123") }
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 644 #{fn} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq out.chomp, "rinstall: created #{dst}"
    eq err, ""
    eq status.success?, true
    eq "123", File.read(dst)
    eq File.stat(dst).mode.to_s(8), '100644'
end

try "Install a binary file from a remote URL to the staging area" do
    src = "#{$wwwtmp}/sh_#{$tests}"
    dst = "#{$systmp}/ps_#{$tests}"
    FileUtils.cp("/bin/sh", "#{src}"); FileUtils.chmod 0644, src
    FileUtils.cp("/bin/ps", "#{dst}"); FileUtils.chmod 0644, dst
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall sh_#{$tests} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.sub(/(Binary files|Files) /, "").sub(/\..+ differ/, ".XXXXXX"),
        "#{$systmp}/ps_#{$tests} and sh_#{$tests}.XXXXXX\n"
    eq status.exitstatus, 0
    # 'ps' was replaced with 'sh'
    eq Digest::MD5.hexdigest(File.read("/bin/sh")),
       Digest::MD5.hexdigest(File.read(dst))
end

is_busybox = ENV['SHELL']=='/bin/ash'
is_macos = %x{ uname }.chomp == "Darwin"
try "Install a file from a remote URL containing special characters", (is_busybox | is_macos) do
    fn = "test ~!@()_+ #{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    File.open(src, 'w') { |f| f.write("123") }
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 644 '#{fn}' '#{dst}'"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq out.chomp, "rinstall: created #{dst}"
    eq err, ""
    eq status.success?, true
    eq "123", File.read(dst)
    eq File.stat(dst).mode.to_s(8), '100644'
end

try "Try to fetch a file that does not exist" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall bogus.txt #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq status.exitstatus, 3
    eq err.split(/\n/)[-1], "rinstall: unable to fetch #{$install_url}/bogus.txt"
    eq out, ""
    eq File.exists?(dst), false
end

try "Install a file from the local staging directory" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    File.open(src, 'w') { |f| f.write("456") }
    cmd = "INSTALL_URL=http://127.0.0.1/X/ #{Dir.pwd}/../rinstall #{src} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq out.chomp, "rinstall: created #{dst}"
    eq err, ""
    eq status.exitstatus, 0
    eq File.exists?(dst), true
end

try "No need to update a file" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{$tests}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    Dir.mkdir "#{$systmp}/#{$tests}"
    File.open(src, 'w') { |f| f.write("000\n123\n") }
    File.open(dst, 'w') { |f| f.chmod(0642); f.write("000\n123\n") }
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 660 #{fn} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out, ""
    eq File.stat(dst).mode.to_s(8), '100642'
    eq status.exitstatus, 1
end

try "Update a file" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{$tests}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    Dir.mkdir "#{$systmp}/#{$tests}"
    File.open(src, 'w') { |f| f.write("000\n123\n") }
    File.open(dst, 'w') { |f| f.write("000\n111\n") }
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 660 #{fn} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, ""
    eq out.gsub(/[-+]{3}(.*)\n/, ""),
        "@@ -1,2 +1,2 @@\n" \
        " 000\n" \
        "-111\n" \
        "+123\n"
    eq File.stat(dst).mode.to_s(8), '100660'
    eq status.success?, true
end

try "Ensure that a relative target cannot be used" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{fn}"
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall bogus.txt #{fn}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq status.exitstatus, 1
    eq err, "rinstall: #{fn} is not an absolute path\n"
    eq out, ""
    eq File.exists?(dst), false
end

try "Refuse to install an empty file" do
    fn = "test_#{$tests}.txt"
    dst = "#{$systmp}/#{$tests}/#{fn}"
    src = "#{$wwwtmp}/#{fn}"
    Dir.mkdir "#{$systmp}/#{$tests}"
    File.open(src, 'w') { |f| f.write("") }
    File.open(dst, 'w') { |f| f.chmod(0642); f.write("000\n123\n") }
    cmd = "INSTALL_URL=#{$install_url} #{Dir.pwd}/../rinstall -m 660 #{fn} #{dst}"
    out, err, status = Open3.capture3(cmd, :chdir=>$systmp)
    eq err, "rinstall: #{fn} is empty\n"
    eq out, ""
    eq status.exitstatus, 1
end
