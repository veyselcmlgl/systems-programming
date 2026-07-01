CC = gcc
CFLAGS = -Wall -Werror -pthread

all: parking_lot

parking_lot: parking_lot.o
	$(CC) $(CFLAGS) -o $@ $^

parking_lot.o: parking_lot.c
	$(CC) $(CFLAGS) -c $<

run: parking_lot
	./parking_lot

clean:
	rm -f parking_lot *.o
