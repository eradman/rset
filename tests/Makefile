RUBY = ruby
CFLAGS = -I..
OBJS += args
OBJS += cmd_pipe_stdin
OBJS += cmd_pipe_stdout
OBJS += copyfile
OBJS += format_env
OBJS += getsocket
OBJS += hostlist
OBJS += log_msg
OBJS += parser
OBJS += ssh_command
OBJS += which
RSET_LIBS = ../compat.o ../rutils.o ../input.o ../execute.o

all: rset.o test

rset.o: ${RSET_LIBS}
	${LD} -r ${RSET_LIBS} -o $@ ${LDFLAGS}

.c: rset.o ${RSET_LIBS}
	${CC} ${CFLAGS} $< rset.o -o $@ ${LDFLAGS}

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
