CFLAGS = -I..
LDFLAGS =
OBJS = getsocket parser pipe copyfile args

all: test_rinstall test_rset

getsocket: getsocket.c ../execute.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o $@

parser: parser.c ../input.c
	cc ${CFLAGS} $< ../input.c ${LDFLAGS} -ll -o $@

copyfile: copyfile.c ../rutils.c
	cc ${CFLAGS} $< ../rutils.c ${LDFLAGS} -ll -o $@

pipe: pipe.c ../execute.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o $@

args: args.c ../execute.c
	cc ${CFLAGS} $< ../execute.c ${LDFLAGS} -o $@

test_rinstall: ../rinstall getsocket
	@ruby test_rinstall.rb

test_rset: ${OBJS}
	@ruby test_rset.rb

lint:
	@mandoc -Tlint ../pln.5
	@mandoc -Tlint ../rinstall.1
	@mandoc -Tlint ../rset.1

clean:
	rm -f *.core ${OBJS}

.PHONY: all clean test regen
