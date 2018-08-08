CFLAGS = -I..
LDFLAGS =
OBJS = getsocket parser pipe sshcmd copyfile

all: test_rinstall test_rset

getsocket: getsocket.c ../execute.c
	cc ${CFLAGS} $> ${LDFLAGS} -o getsocket

parser: parser.c ../input.c
	cc ${CFLAGS} $> ${LDFLAGS} -ll -o parser

copyfile: copyfile.c ../input.c
	cc ${CFLAGS} $> ${LDFLAGS} -ll -o copyfile

pipe: pipe.c ../execute.c
	cc ${CFLAGS} $> ${LDFLAGS} -o pipe

sshcmd: sshcmd.c ../execute.c
	cc ${CFLAGS} $> ${LDFLAGS} -o sshcmd

test_rinstall: ../rinstall getsocket
	@ruby test_rinstall.rb

test_rset: ${OBJS}
	@ruby test_rset.rb

lint:
	@mandoc -Tlint ../rset.1

clean:
	rm -f *.core ${OBJS}

.PHONY: all clean test regen
