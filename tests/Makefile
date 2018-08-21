CFLAGS = -I..
LDFLAGS =
OBJS = getsocket parser pipe copyfile args

all: test_rinstall test_rset

getsocket: getsocket.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o getsocket

parser: parser.c
	cc ${CFLAGS} $< ../input.c ${LDFLAGS} -ll -o parser

copyfile: copyfile.c
	cc ${CFLAGS} $< ../input.c ${LDFLAGS} -ll -o copyfile

pipe: pipe.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o pipe

args: args.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o args

test_rinstall: ../rinstall getsocket
	@ruby test_rinstall.rb

test_rset: ${OBJS}
	@ruby test_rset.rb

lint:
	@mandoc -Tlint ../rset.1

clean:
	rm -f *.core ${OBJS}

.PHONY: all clean test regen
