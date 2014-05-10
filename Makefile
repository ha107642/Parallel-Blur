CC=gcc -O3 -Wall
CLIBS= -pthread
	
all: blurpt
	
time.o: time.c timer.h
	$(CC) -c time.c
	
qdbmp.o: qdbmp.c qdbmp.h
	$(CC) -c qdbmp.c

%:  %.c time.o qdbmp.o
	$(CC) -o $@ $@.c time.o qdbmp.o
	
%pt: %pt.c time.o qdbmp.o
	$(CC) $(CLIBS) -o $@ $@.c time.o qdbmp.o
	
clean:
	rm -f *.o

distclean: clean
	rm -f *~
