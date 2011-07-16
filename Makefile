
CC=gcc
CFLAGS=-Wall -g #-O3 #-g 
LDFLAGS=-lumem

rrb: rrb.o mac.o err.o

clean:
	rm -f *.o *.gcno *.gcda rrb

backup:
	tar cjf ${HOME}/backup/rrb-`date +%s`.tbz2 *.[ch] Makefile README && (ls -lrt ~/backup/rrb-* | tail -1 )
