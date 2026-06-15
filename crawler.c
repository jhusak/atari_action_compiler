#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crawler.h"
#include "actionc.h"

#define MEM_SIZE 65536


uint8_t vmem[MEM_SIZE];
uint8_t visited[MEM_SIZE];

typedef struct {
	uint16_t addr;
} QueueItem;

QueueItem queue[MEM_SIZE];
int qhead = 0;
int qtail = 0;

static void enqueue(uint16_t addr)
{
	if (!visited[addr])
		queue[qtail++].addr = addr;
}

static int dequeue(uint16_t *addr)
{
	if (qhead == qtail)
		return 0;

	*addr = queue[qhead++].addr;
	return 1;
}

static int is_branch(uint8_t op)
{
	switch (op) {
		case 0x10: case 0x30:
		case 0x50: case 0x70:
		case 0x90: case 0xB0:
		case 0xD0: case 0xF0:
			return 1;
		default:
			return 0;
	}
}

void crawl6502(uint16_t entry)
{
	enqueue(entry);

	while (dequeue(&entry)) {

		uint16_t pc = entry;

		while (1) {

			if (visited[pc])
				break;

			uint8_t op = read6502(pc);
			uint8_t len = lentable[op];
			//if (pc>=0xa000 && pc<0xc000)
			//fprintf(stderr, "addr: %04x val: %02x len: %d\n",pc, op, len);

			visited[pc] = 1;
			if (len>=2) visited[pc+1]=1;
			if (len==3) visited[pc+2]=1;

			if (len == 0)
				break;

			/* RTS, RTI, BRK */
			if (op == 0x60 || op == 0x40 || op == 0x00)
				break;

			/* JSR */
			if (op == 0x20) {
				uint16_t target = read6502word(pc + 1);

				if (target>=0xa000 && target<0xc000)
					enqueue(target);
				//fprintf(stderr, "jsr addr: %04x target: %04x\n",pc,target);

				pc += 3;
				continue;
			}

			/* JMP abs */
			if (op == 0x4C) {
				uint16_t target = read6502word(pc + 1);

				if (target>=0xa000 && target<0xc000)
				enqueue(target);
				//fprintf(stderr, "jmp addr: %04x target: %04x\n",pc,target);
				break;
			}

			/* JMP indirect */
			if (op == 0x6C) {
				//fprintf(stderr, "jmp (xxxx) encountered. break crawl;\n");
				uint16_t itarget = read6502word(pc + 1);
				uint16_t target=read6502word(itarget);
				if (target>=0xA000 && target<0xc000) {
				//	fprintf(stderr, "enqueued addr %04x from (%04x)\n",target,itarget);
					enqueue(target);
				}
				break;
			}

			/* branch */
			if (is_branch(op)) {
				int8_t disp = (int8_t)read6502(pc + 1);

				uint16_t taken = (uint16_t)(pc + 2 + disp);

				enqueue(taken);
				//fprintf(stderr, "enq addr: %04x taken: %04x\n",pc,taken);

				pc += 2;
				continue;
			}

			pc += len;
		}
	}
	//fprintf(stderr, "ended\n");
}

void save_crawl_mem()
{
    FILE *f;

    FOPEN(f,"memcrawl.sav","wb")

    for (int i=0xa000; i<0xc000; i++)
    {
	    uint8_t c=read6502(i);
	    fwrite(&c, 1, 1, f);
    }

    fclose(f);

    FOPEN(f,"memvisited.sav", "wb");

    for (int i=0xa000; i<0xc000; i++)
    {
	    uint16_t c=visited[i];
	    fwrite(&c, 1, 1, f);
    }

    fclose(f);
}
