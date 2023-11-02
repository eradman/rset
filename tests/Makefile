RUBY = ruby
CFLAGS = -I..
OBJS = args cmd_pipe_stdin cmd_pipe_stdout copyfile getsocket parser ssh_command which hostlist
RSET_LIBS = ../compat.o ../rutils.o ../input.o ../execute.o

all: rset.o test

rset.o: ${RSET_LIBS}
	${LD} -r ${RSET_LIBS} -o $@

.c: rset.o
	${CC} ${CFLAGS} $< rset.o ${LDFLAGS} -o $@

test: ${OBJS}
	@${RUBY} test_renv.rb
	@${RUBY} test_rsub.rb
	@${RUBY} test_miniquark.rb
	@${RUBY} test_rinstall.rb
	@${RUBY} test_labelgrep.rb
	@${RUBY} test_rset.rb

clean:
	rm -rf .gem
	rm -f *.core ${OBJS} *.o

.PHONY: all clean test
