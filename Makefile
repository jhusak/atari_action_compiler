
all: ac

ac: actionc.c fake6502.c fake6502.h devices.c devices.h crawler.c save_exec_lib.c
	$(CC) -flto -march=native -O3 -W -Wall -Wextra -o actionc actionc.c fake6502.c devices.c crawler.c save_exec_lib.c

clean:
	rm -f *~ */*~ actionc

