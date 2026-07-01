CC = gcc
CFLAGS = -pthread -Wall -Wextra
TARGETS = PideShop HungryVeryMuch

all: $(TARGETS)

PideShop: PideShop.c
	$(CC) $(CFLAGS) -o PideShop PideShop.c -lm

HungryVeryMuch: HungryVeryMuch.c
	$(CC) $(CFLAGS) -o HungryVeryMuch HungryVeryMuch.c

clean:
	rm -f $(TARGETS) pideshop.log
