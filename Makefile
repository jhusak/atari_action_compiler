
all: ac

ac: actionc.c fake6502.c fake6502.h devices.c devices.h
	$(CC) -flto -march=native -O3 -W -Wall -Wextra -o actionc actionc.c fake6502.c devices.c

clean:
	rm -f *~ */*~ actionc

