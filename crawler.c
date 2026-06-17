#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crawler.h"
#include "actionc.h"

#define MEM_SIZE 65536


uint8_t vmem[MEM_SIZE];
uint8_t visited[MEM_SIZE];

extern unsigned char action_bin_nozap[];

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
	int i=0;
	enqueue(entry);

	while (dequeue(&entry)) {

		uint16_t pc = entry;

		switch (entry){ // Applying patches - for external data segments
			case 0xa654: //Graphics
				visited[0xb0b0]=1;
				for (i=0xa684; i<0xa688; i++) visited[i]=1; // E:
			case 0xa68c: //DrawTo
				for (i=0xa688; i<0xa68c; i++) visited[i]=1; // S:
				break;
			case 0xa737:// PTrig
				for (i=0xa74a; i<0xa74e; i++) visited[i]=1; // button masks
				break; 
			case 0xa3cc: // PrintF - %h issue
				     // a439 word update to mainbnk.prth
				     // copy mainbnk.prth to location
				     //    356 AF13                     printh  .proc                   ; PrintH(num)
				/*
				   357 AF13 85 A0                       sta     arg0
				   358 AF15 86 A1                       stx     arg1
				   359 AF17 A9 04                       lda     #4
				   360 AF19 85 A2                       sta     arg2
				   361 AF1B A0 24                       ldy     #'$'
				   362 AF1D 20 00 B2                    jsr     mainio.putchar
				   363 AF20 A9 00               ??ph1   lda     #0
				   364 AF22 A2 04                       ldx     #4
				   365 AF24 06 A0               ??ph2   asl     arg0
				   366 AF26 26 A1                       rol     arg1
				   367 AF28 2A                          rol
				   368 AF29 CA                          dex
				   369 AF2A D0 F8                       bne     ??ph2
				   370                          ; CLC
				   371 AF2C 69 30                       adc     #'0'
				   372 AF2E C9 3A                       cmp     #':'
				   373 AF30 30 02                       bmi     ??ph3
				   374 AF32 69 06                       adc     #6
				   375 AF34 A8                  ??ph3   tay
				   376 AF35 20 00 B2                    jsr     mainio.putchar
				   377 AF38 C6 A2                       dec     arg2
				   378 AF3A D0 E4                       bne     ??ph1
				   379 AF3C 60                          rts
				   380                                  .endp
				   */

				
				{
					uint16_t len=0xAF3D-0xAF13;
					uint16_t dproc=0x2e0-len;
					printf("Patching lib PrintF: %04x-%04x for %H hex output\n",0xA000+dproc,0xA000+dproc+len-1);
					for (int i=0; i<len; i++)
					{
						action_lib[0x1000+dproc+i]=action_bin_nozap[0x3000+0xf13+i];
						visited[0xA000+dproc+i]=1;
					}
					// substitute jsr addr
					action_lib[0x1000+0x439]=dproc&0xff;
					action_lib[0x1000+0x43a]=0xA0+(dproc>>8);
				}
				break;
		}


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
