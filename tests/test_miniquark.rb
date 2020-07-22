#!/usr/bin/env ruby

require "open3"
require "tempfile"
require "socket"
require "time"

# Test Utilities
$tests = 0
$test_description = 0

parent_pid = Process.pid
$pid = 0

def child_wait
    if $pid > 0 then
        Process.kill(:TERM, $pid) rescue nil
        Process.wait $pid rescue nil
    end
end

# Setup
$systmp = Dir.mktmpdir
at_exit { child_wait; FileUtils.remove_dir $systmp if parent_pid == Process.pid }

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

# Smoke test

try "Run miniquark with no arguments" do
    cmd = "../miniquark"
    out, err, status = Open3.capture3(cmd)
    eq err.gsub(/release: (\d\.\d)/, "release: 0.0"),
        "release: 0.0\n" +
        "usage: miniquark -p port [-h host] [-d dir]\n"
    eq status.success?, false
end

# Populate staging directory
%x{
    cd #{$systmp}
    echo "iohNg]ei8wu" > secrets
    mkdir www www/subdir

    cd www
    case `uname` in
      Darwin)
        dd if=/dev/urandom of=largefile bs=1m count=10 2>&1
        ;;
      *)
        dd if=/dev/urandom of=largefile bs=1M count=10 2>&1
        ;;
    esac
    echo ABCDEFGHIJKLMNOPQRSTUVWXYZ > smallfile
    touch noread.sh
    chmod 110 noread.sh
    touch empty.tar.gz
}

# Temporary server

socket = Socket.new(:INET, :STREAM, 0)
socket.bind(Addrinfo.tcp("127.0.0.1", 0))
port = socket.local_address.ip_port
socket.close

$pid = spawn('../miniquark', '-p', port.to_s, '-d', File.join($systmp, "www"),
             :in=>"/dev/null", :out=>"/dev/null",
             :unsetenv_others=>true)
sleep 0.2
today = 'Sat, 11 Jul 2020 01:25:02 GMT'

# Fetch content

try "HEAD request with target host and user-agent" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~REQUEST
            HEAD /largefile HTTP/1.0\r
            Host: 127.0.0.1\r
            User-Agent: Ruby tcp\r
            \r
        REQUEST
        sock.close_write
        response = sock.read.gsub(/^(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 200 OK\r
            Date: #{today}\r
            Connection: close\r
            Last-Modified: #{today}\r
            Content-Type: application/octet-stream\r
            Content-Length: 10485760\r
            \r
        REPLY
    }
end

try "HEAD request for missing file" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print "HEAD /robots.txt HTTP/1.0\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/^(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 404 Not Found\r
            Date: #{today}\r
            Connection: close\r
            Content-Type: text/plain\r
            \r
            404 Not Found
        REPLY
    }
end

try "GET a file that does have have read permission" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print "GET /noread.sh HTTP/1.1\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/^(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 403 Forbidden\r
            Date: #{today}\r
            Connection: close\r
            Content-Type: text/plain\r
            \r
            403 Forbidden
        REPLY
    }
end

try "GET a small file" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print "GET /smallfile HTTP/1.0\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 200 OK\r
            Date: #{today}\r
            Connection: close\r
            Last-Modified: #{today}\r
            Content-Type: application/octet-stream\r
            Content-Length: 27\r
            \r
            ABCDEFGHIJKLMNOPQRSTUVWXYZ
        REPLY
    }
end

try "GET a file using If-Modified-Since" do
    file = File.stat File.join($systmp, "www/smallfile")
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~REQUEST
            GET /smallfile HTTP/1.1\r
            If-Modified-Since: #{file.mtime.httpdate}\r
            \r
        REQUEST
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 304 Not Modified\r
            Date: #{today}\r
            Connection: close\r
            \r
        REPLY
    }
end

try "GET Range (partial content)" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~REQUEST
            GET /smallfile HTTP/1.1\r
            Range: bytes=0-12\r
            \r
        REQUEST
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY.chomp
            HTTP/1.1 206 Partial Content\r
            Date: #{today}\r
            Connection: close\r
            Last-Modified: #{today}\r
            Content-Type: application/octet-stream\r
            Content-Length: 13\r
            Content-Range: bytes 0-12/27\r
            \r
            ABCDEFGHIJKLM
        REPLY
    }
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~REQUEST
            GET /smallfile HTTP/1.1\r
            Range: bytes=13-27\r
            \r
        REQUEST
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 206 Partial Content\r
            Date: #{today}\r
            Connection: close\r
            Last-Modified: #{today}\r
            Content-Type: application/octet-stream\r
            Content-Length: 14\r
            Content-Range: bytes 13-26/27\r
            \r
            NOPQRSTUVWXYZ
        REPLY
    }
end

try "GET Range (bogus range)" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~REQUEST
            GET /smallfile HTTP/1.1\r
            Range: bytes=12-6\r
            \r
        REQUEST
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 416 Range Not Satisfiable\r
            Date: #{today}\r
            Content-Range: bytes */27\r
            Connection: close\r
            \r
        REPLY
    }
end

try "GET an empty archive" do
    Socket.tcp("127.0.0.1", port) {|sock|
      sock.print "GET /empty.tar.gz HTTP/1.0\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 200 OK\r
            Date: #{today}\r
            Connection: close\r
            Last-Modified: #{today}\r
            Content-Type: application/octet-stream\r
            Content-Length: 0\r
            \r
        REPLY
    }
end

try "GET a file in a parent directory" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print "GET /../secrets HTTP/1.0\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 404 Not Found\r
            Date: #{today}\r
            Connection: close\r
            Content-Type: text/plain\r
            \r
            404 Not Found
        REPLY
    }
end

try "List a directory" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print "GET /subdir HTTP/1.0\r\n\r\n"
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 403 Forbidden\r
            Date: Sat, 11 Jul 2020 01:25:02 GMT\r
            Connection: close\r
            Content-Type: text/plain\r
            \r
            403 Forbidden
        REPLY
    }
end

try "POST json conent" do
    Socket.tcp("127.0.0.1", port) {|sock|
        sock.print <<~DATA
            POST /login HTTP/1.1\r"
            Content-Type: application/json\r
            Content-Length: 134\r
            \r
            {"username": "radman"}
        DATA
        sock.close_write
        response = sock.read.gsub(/(Date: |Last-Modified: ).+(\r)/, '\1'+today+'\2')
        eq response, <<~REPLY
            HTTP/1.1 405 Method Not Allowed\r
            Date: Sat, 11 Jul 2020 01:25:02 GMT\r
            Connection: close\r
            Allow: HEAD, GET\r
            Content-Type: text/plain\r
            \r
            405 Method Not Allowed
        REPLY
    }
end

try "Fetch a 10MB file in parallel with an external utility" do
    pids = []
    src_url = "http://127.0.0.1:#{port}/largefile"
    6.times do |i|
        dst = "#{$systmp}/largefile.#{i}"
        pids << spawn("./fetch.sh", src_url, dst,
                      :out=>["#{$systmp}/fetch_log", "w"],
                      :unsetenv_others=>true)
    end
    pids.each { |pid| Process.wait pid }
    6.times do |i|
        eq File.stat("#{$systmp}/largefile.#{i}").size, 10485760
    end
end
