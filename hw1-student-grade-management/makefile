CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99

gtu: gtuStudentGrades.c
	$(CC) $(CFLAGS) gtuStudentGrades.c -o gtu

run: gtu
	./gtu
clean:
	rm -f gtu
	rm -f system.log

makeclean: clean
