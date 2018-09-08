CFLAGS = -I..
OBJS = getsocket copyfile parser pipe args
RSET_LIBS = ../compat.o ../rutils.o ../input.o ../execute.o

all: rset.o test

rset.o: ${RSET_LIBS}
	${LD} -r ${RSET_LIBS} -o $@

.c: rset.o
	${CC} ${CFLAGS} $< rset.o ${LDFLAGS} -o $@

test: ../rinstall ${OBJS}
	@ruby test_rinstall.rb
	@ruby test_rset.rb

lint:
	@mandoc -Tlint ../pln.5
	@mandoc -Tlint ../rinstall.1
	@mandoc -Tlint ../rset.1

clean:
	rm -f *.core ${OBJS} *.o

.PHONY: all clean test
