CFLAGS = -I..
OBJS = args cmd_pipe_stdin cmd_pipe_stdout copyfile getsocket parser ssh_command which
RSET_LIBS = ../compat.o ../rutils.o ../input.o ../execute.o

all: rset.o test

rset.o: ${RSET_LIBS}
	${LD} -r ${RSET_LIBS} -o $@

.c: rset.o
	${CC} ${CFLAGS} $< rset.o ${LDFLAGS} -o $@

test: ${OBJS}
	@ruby test_rsub.rb
	@ruby test_miniquark.rb
	@ruby test_rinstall.rb
	@ruby test_labelgrep.rb
	@ruby test_rset.rb

clean:
	rm -f *.core ${OBJS} *.o

.PHONY: all clean test
