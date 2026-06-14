#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <fcntl.h>
#include <ctype.h>

/*
 * actionc.c
 *
 * Wrapper for fake6502 + ACTION!
 * Author: Jakub Husak, 06.2026
 *
 * build:
 *   cc actionwrap.c fake6502.c -o action6502
 *
 * usage:
 *   ./action6502 input.act output.xex
 *
 *   ./action6502 input.act output.xex \
 *       -m 0x4000 0x12 \
 *       -m 0x4001 0x34
 *
 * TODO: cleanup file mess.
 *
 * The software is "as is". No responsibility taken, use it as you want, but mention the author.
 */

#include "actionc.h"
#include "fake6502.h"
#include "devices.h"
#include "crawler.h"
#include "save_exec_lib.h"

#define GO_TO_COMPILER	1
#define RAM_SIZE        65536
//#define ACTION_LOAD     0x2000

extern uint8_t memory[65536];
extern uint16_t PC;
extern uint8_t A;
extern uint8_t X;
extern uint8_t Y;
extern uint8_t SP;

/* ========================================================= */

uint8_t memory[65536];
uint8_t force_write=0;
uint8_t write_mem=0;
uint8_t show_action_calls=0;
uint8_t show_compilation_time=0;
uint8_t do_save_library=0;
#include "inc/asciitoatari.c"
#include "inc/memory.c"
#include "inc/action_36.c"
typedef uint8_t UBYTE;
#include "inc/altirraos_xl.c"
//#include "inc/atariosxl.c"
#define IS_IDLE() (PC>=0xa2e6 && PC<=0xa2ef)

char * functab[65536]={0};

void init_functab() {
#include "inc/functab.c"
}

uint8_t ascii_to_screen(uint8_t c)
{
	if (c < 32)
		return c + 64;

	if (c < 96)
		return c - 32;

	return c;
}

/*
   Atari SCREEN CODE -> ASCII
   */

uint8_t screen_to_ascii(uint8_t c)
{
	if (c < 64)
		return c + 32;

	if (c < 96)
		return c - 64;

	return c;
}


char * get_error(char * err)
{
	uint8_t errn=atoi(err);
	if (strlen(err)>0)
		switch (errn) {
			case 0: 		return "Out of system memory.";
			case 1: 		return "Missing \" (double quote) \" in a string.";
			case 2: 		return "Nested Defines. You cannot nest the Define directive.";
			case 3: 		return "Global variable symbol table full.";
			case 4: 		return "Local variable symbol table full.";
			case 5: 		return "Set directive syntax error.";
			case 6: 		return "Wrong declaration format.";
			case 7: 		return "Invalid argument list.";
			case 8: 		return "Variable not declared.";
			case 9: 		return "A variable was used where a constant of some kind was required.";
			case 10:		return "Illegal assignment.";
			case 11:		return "Unknown error. System error routines have been impaired so an error can't be defined.";
			case 12:		return "Missing THEN";
			case 13:		return "Missing FI.";
			case 14:		return "Out of code space.";
			case 15:		return "Missing DO.";
			case 16:		return "Missing TO.";
			case 17:		return "Bad expression. Illegal expression format.";
			case 18:		return "Unmatched parenthesis.";
			case 19:		return "Missing OD.";
			case 20:		return "Can't allocate memory.";
			case 21:		return "Illegal array reference.";
			case 22:		return "Input file too large. Break it up.";
			case 23:		return "Illegal conditional statement.";
			case 24:		return "Illegal FOR statement syntax.";
			case 25:		return "Illegal EXIT. No DO - OD loop for the EXIT to EXIT out of.";
			case 26:		return "Nesting too deep (16 levels maximum).";
			case 27:		return "Illegal TYPE syntax.";
			case 28:		return "Illegal RETURN.";
			case 61:		return "Out of Symbol Table space.";
			case 128:		return "BREAK key was used to stop program execution.";
			case 144:		return "Cannot write file.";
			case 165:		return "Bad filename.";
			case 170:		return "File does not exist.";
			default:		return "Error code not recognised";
		}
	return "Error format error";
}


int bankA000_offset=0x1000;

uint8_t read6502(uint16_t address) {
	
	//if (force_write)
	//	fprintf(stderr,"read addr: %04x > %02x\n", address, memory[address]);

	// banking
	if (address>=0xa000 && address<0xb000)
	{
		return action_bin[address-0xa000+bankA000_offset];
	}
	// constant place
	if (address>=0xb000 && address<0xc000)
	{
		return action_bin[address-0xb000];
	}
	if (address<0xa000) return memory[address];

	if (address>=0xc000 && address<0xd000)
		return atarios_bin[address-0xc000];


	if (address>=0xd800)
		return atarios_bin[address-0xc000];

	if (address>=H_DEVICE_BEGIN && address<H_DEVICE_END)
	{
		return memory[address];
	}
	if (address==0xd013) return 1; // cart on
	if (address==0xd01f) return 7; // no consol pressed
	if (address==0xd40b) return memory[address]; // vcount
	if (address==0xd40e) return memory[address]; // vcount
	if (address==0xd40f) return memory[address]; // vcount
	if (address==0xd500) { bankA000_offset=0x1000; return 0xff; }
	if (address==0xd503) { bankA000_offset=0x3000; return 0xff; }
	if (address==0xd509) { bankA000_offset=0x2000; return 0xff; }

    return memory[address];
}

void write6502(uint16_t address, uint8_t value) {
	if (force_write) {memory[address]=value; 
	//	fprintf(stderr,"Write addr: (%d) %04x < %02x\n",force_write, address, value);
		}
	if (address <0xA000) memory[address] = value;
	if (address==0xd500) { bankA000_offset=0x1000; return; }
	if (address==0xd503) { bankA000_offset=0x3000; return; }
	if (address==0xd509) { bankA000_offset=0x2000; return; }
	if (address>0xD000 && address<0xD800) { memory[address]=value ; return; }
}

void write6502word(uint16_t address, uint16_t value)
{
	write6502(address,value&0xff);
	write6502(address+1,(value>>8)&0xff );


}

/* ========================================================= */
static FILE *fin  = NULL;
static FILE *fout = NULL;

static char * filename_in;
static char * filename_out;


static int running = 1;

/* ========================================================= */

void fatal(const char *msg)
{
    fprintf(stderr, "fatal: %s\n", msg);
    exit(1);
}

/* ========================================================= */

static void save_memory_full()
{
    FILE *f;

    f = fopen("memfull.sav", "wb");

    if (!f)
        fatal("cannot open file memfull.sav");

    for (int i=0; i<65536; i++)
    {
	    uint8_t c=read6502(i);
	    fwrite(&c, 1, 1, f);
    }

    fclose(f);
}


/* ========================================================= */

#if 0
static void set_reset_vector(uint16_t addr)
{
    write6502(0xFFFC, addr & 0xFF);
    write6502(0xFFFD, addr >> 8);
}
#endif

/* ========================================================= */

/*
 * bardzo prosty host_open/host_close
 *
 * handle 0 = input.act
 * handle 1 = output
 * handle >=2 = include files
 *
 * ACTION string:
 *   [len][ascii...]
 */

/* ========================================================= */

static void action_string_to_c(
    uint16_t ptr,
    char *out,
    size_t outsz)
{
    uint8_t len;

    len = read6502(ptr);

    if (len >= outsz)
        len = (uint8_t)(outsz - 1);

    ptr++;
    for (uint8_t i=0; i<len; i++) out[i]=read6502(ptr+i);

    //memcpy(out, &memory[ptr + 1], len);

    out[len] = 0;
}

static void c_string_to_action(
		char *in,
		char *in2,
		char *in3,
		uint16_t ptr,
		size_t outsz)
{
	uint8_t len;

	len = strlen(in2);

	if (len >= outsz)
		len = (uint8_t)(outsz - 1);

	// hacky constraints for wrapping strings
	if (strlen(in)>2) in[2]='\0';
	if (strlen(in3)>1) in3[1]='\0';

	uint16_t ptr0=ptr;
	ptr++;
	for (uint8_t i=0; i<strlen(in); i++) write6502(ptr++,in[i]);
	for (uint8_t i=0; i<len; i++) write6502(ptr++,in2[i]);
	for (uint8_t i=0; i<strlen(in3); i++) write6502(ptr++,in3[i]);
	// save len
	write6502(ptr0,ptr-ptr0-1);
}



static void host_exit(void)
{
    //fprintf(stderr, "[HOST_EXIT]\n");

    running = 0;
}

/* ========================================================= */

static int intercept_jsr(void)
{

    switch (PC) {

        case H_PATCH_OPEN:
            Devices_H_Open();
            return 1;

        case H_PATCH_CLOS:
            Devices_H_Close();
            return 1;

        case H_PATCH_READ:
            Devices_H_Read();
            return 1;

        case H_PATCH_WRIT:
            Devices_H_Write();
            return 1;

        case H_PATCH_STAT:
            Devices_H_Status();
            return 1;
    }

    return 0;
}


static void handle_sigint(int sig) {
        (void)sig;
	running=0;
}

/* ========================================================= */

static void run_emulator(void)
{
	long cycles = 0;
	int compile_frames=0;
	init_functab();

	struct timespec now, next_time;

	/* pobierz aktualny czas */
	clock_gettime(CLOCK_MONOTONIC, &now);

	/* dodaj 1/50 sekundy = 20 000 000 ns */
	next_time.tv_nsec += 20000000;



	//write6502(0x8, 0xff);
	//write6502(0x9, 0);
	//write6502(0x244, 0);

	int inited=0;

	while (running) {


		if (PC>=H_DEVICE_BEGIN && PC<H_DEVICE_END && intercept_jsr()) {

			/*
			 * symulacja RTS
			 */

			PC = pull16();
			PC++;
			continue;
		}

		//fprintf(stderr,"%04x\n",PC);
		//
		// CHECK FOR ACTION CALL
		if (PC==0xBFB7)
		{
			if (A==0x20)
			{
				if (Y>=0xa0 && Y<0xbf)
				{
					int ln=Devices_H_GetLastReadLineNumber();
					int a=read6502word(0xe);
					int call=(Y<<8)+X;
					int obank= bankA000_offset;
					bankA000_offset=0x1000;
					crawl6502(call);
					bankA000_offset=obank;

					if (show_action_calls) {
						printf("LIBCALL: %04x: JSR %04x",a,call);
						if (functab[call]!=NULL) printf("   %s",functab[call]);
						printf("\n");
					}
					if (show_action_calls==2) {
						char oute[256];
						action_string_to_c(0x900,oute,sizeof(oute));

						if (strlen(oute)!=0)
							printf("Line %d: %s\n",ln, oute);
					}
				}

			}
		}
		step6502();

		cycles++;

		if ((cycles % 100) == 0) { // _about_ one scanline in real time
			write6502(0x3A, 1);
			write6502(0x39, 1);


			if (++memory[0xd40b]==156) { // simulating vblank
				memory[0xd40b]=0;

				memory[20]++;	// simulating fake nmi
				if (!memory[20]) memory[19]++;
				if (!memory[20] && !memory[19]) memory[18]++;

				if IS_IDLE() {

					if (inited==2) {
						host_exit();
					}

					if (compile_frames>0) {
					//	fprintf(stderr,"compile frames: %d\n",compile_frames);
						compile_frames=0;


						if (!inited) {
							// depends on textmode
							c_string_to_action("C\"",filename_in,"\"",0x590,0x21);
							write6502(764,12);
							inited=1;
						} else if (inited==1) {
							h_textmode=0; // write always binary
							c_string_to_action("W\"",filename_out,"\"",0x590,0x21);
							write6502(764,12);
							inited=2;

						}
					}
				}
				else
					compile_frames++;
			}

		}

	}
	if (show_compilation_time) {
		clock_gettime(CLOCK_MONOTONIC, &next_time);
		next_time.tv_sec-=now.tv_sec;

		if (next_time.tv_nsec < now.tv_nsec)
		{
			next_time.tv_sec--;
			next_time.tv_nsec +=1000000000;
		}

		next_time.tv_nsec -=now.tv_nsec;

		printf("Compiled in %ld.%06ld sec\n",next_time.tv_sec,next_time.tv_nsec/1000);
	}


}

/* ========================================================= */

static void usage(const char *prog)
{
	fprintf(stderr,
			"usage:\n"
			"  %s input.act output.obj <options>\n"
			"	options:\n"
			"	-a	- set atari file format (0x9b = enter), default unix file format\n"
			"	-c	- print action library calls during compilation\n"
			"	-C	- print action library calls during compilation along with code\n"
			"	-w	- write mem.sav (for inspection)\n"
			"	-t	- show compilation time\n"
			"	-l	- link with library functions used\n"
			"	-m addr val - like SET addr=val in Action!; may be used multiple times\n",
			prog);

	exit(1);
}

/* ========================================================= */

int main(int argc, char **argv)
{
	int i;

	if (argc < 3)
		usage(argv[0]);

	filename_in=argv[1];
	if (strlen(filename_in)>0x21)  // 0x21 is max filename length Action! can handle
		fatal("input filename too long");

	filename_out=argv[2];
	if (strlen(filename_out)>0x21) 
		fatal("output filename too long");

        signal(SIGINT, handle_sigint);


#if TERMINAL
	struct termios t,told;

	tcgetattr(0, &t);
	told=t;

	t.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(0, TCSANOW, &t);

	int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

	fcntl(0, F_SETFL, oldf | O_NONBLOCK);
#endif


	memset(memory,0,sizeof(memory));
	memcpy(memory,memdump_dat,sizeof(memdump_dat));
	
	reset6502();

	// PRE-INIT emulator
	
	force_write=1;
	Devices_Frame();

	write6502(0x496, GO_TO_COMPILER);
	write6502(0x4e0, 0x60); // no bell

	force_write=0;

	/*
	 * optional memory pokes
	 */

	for (i = 3; i < argc; i++) {

		if (strcmp(argv[i], "-a") == 0) { // default text mode on, here set binary for texts
			h_textmode=0x00;
		}

		if (strcmp(argv[i], "-c") == 0) { // default no, here set showcalls during compile
			show_action_calls=1;
		}

		if (strcmp(argv[i], "-C") == 0) { // default no, here set showcalls during compile
			show_action_calls=2;
		}

		if (strcmp(argv[i], "-w") == 0) { // default text mode, here set binary for texts
			write_mem=1;
		}

		if (strcmp(argv[i], "-t") == 0) { // default no, here set showcalls during compile
			show_compilation_time=1;
		}

		if (strcmp(argv[i], "-l") == 0) {
			do_save_library=1;
		}


		if (strcmp(argv[i], "-m") == 0) {

			uint16_t addr;
			uint8_t val;

			if ((i + 2) >= argc)
				fatal("missing -m args");

			addr = (uint16_t)strtoul(argv[i + 1], NULL, 0);
			val  = (uint8_t)strtoul(argv[i + 2], NULL, 0);

			write6502(addr, val);

			fprintf(stderr,
					"poke $%04X = $%02X\n",
					addr,
					val);

			i += 2;
		}
	}

	// cart start address. ram is pre-inited with real startup values
	PC=0xb7e7;

	// END PRE-INIT
	
	run_emulator();

	char oute[256]={0};
	char outc[256]={0};

	if (read6502(0x550)) {
		action_string_to_c(0x900,oute,sizeof(oute));

		if (strlen(oute)!=0)
			fprintf(stderr,"%s\n",oute);
		action_string_to_c(0x550,outc,sizeof(outc));
	}
	int error=0;
	if (strlen(outc)!=0)
	{
		fprintf(stderr,"Compile Error: %s - %s\n",outc,get_error(outc));
		error = 1;
	}

	fclose(fin);
	fclose(fout);

	if (write_mem) {
		save_memory_full();
		save_crawl_mem();
	}
	if (do_save_library)
	{
		char * libname=NULL;
		asprintf(&libname,"%s.%s",filename_out,"lib");
		if (!libname) fatal("Cannot open .lib file for output");
		
		bankA000_offset=0x1000;
		save_used_functions_as_executable(libname);
		free(libname);

	}



	// restore terminal
#if TERMINAL
	tcsetattr(0, TCSANOW, &told);
	fcntl(0, F_SETFL, oldf);
#endif

	return error;
}
