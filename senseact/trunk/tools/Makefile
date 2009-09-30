CC := gcc
CFLAGS := -c -Wall -I../kernel/include
LDFLAGS :=

all: senseact

senseact: senseact.o
	$(CC) $(LDFLAGS) senseact.o -o senseact

senseact.o: senseact.c
	$(CC) $(CFLAGS) senseact.c

clean:
	@rm -f *.o senseact
