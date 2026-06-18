/*
 * save_exec_lib.c
 *
 * Copyright (C) 2026 - Jakub Husak
 *
 * This file is part of the Atari Action! Compiler project
 *
 * Atari Action! Compiler is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * Atari Action! Compiler is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari Action! Compiler; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <stdio.h>
#include <stdint.h>

extern uint8_t visited[65536];
extern uint8_t read6502(uint16_t addr);
#include "inc/jumptab.c"
#include "actionc.h"
static void write_word_le(FILE *f, uint16_t v)
{
	fputc(v & 0xff, f);
	fputc(v >> 8, f);
}

int save_used_functions_as_executable(const char *filename)
{
	FILE *f;
	uint32_t addr;


	int header_generated=0;

	addr = 0xA000;

	// those two are good enough for alitrra os


	// those ar neede by atari os
	//for (int i=0xa300; i<0xa800;i++)
	//	visited[i]=1;
	//for (int i=0xb004; i<0xb06e;i++)
	//	visited[i]=1;
	//for (int i=0xa000; i<0xb800;i++)
	//	visited[i]=1;
	// visited[0xb0b1]=1;
	// visited[0xb0b2]=1;


	while (addr < 0xC000)
	{
		/* first used byte in segment */
		while (addr < 0xC000 && !visited[addr])
			addr++;

		if (addr >= 0xC000)
			break;

		if (!header_generated) {
			f = fopen(filename, "wb");
			if (!f)
				return 0;
			/* preamble Atari DOS */
			write_word_le(f, 0xFFFF);

			for (uint32_t i=0; i<sizeof(jumptab); i++)
				fputc(jumptab[i], f);
			header_generated=1;
		}


		uint32_t start = addr;
		uint32_t end = addr;

		uint32_t gap = 0;

		while (++addr < 0xC000)
		{
			if (visited[addr])
			{
				gap = 0;
				end = addr;
			}
			else
			{
				gap++;

				/*
				 * more than 5 zeros ends segment
				 */
				if (gap >= 5)
					break;
			}
		}

		write_word_le(f, (uint16_t)start);
		write_word_le(f, (uint16_t)end);

		for (uint32_t a = start; a <= end; a++) {
			if (a<0xb000)
				fputc(action_lib[(uint16_t)a-0xA000+0x1000], f);
			else
				fputc(action_lib[(uint16_t)a-0xB000], f);

		}
	}

	if (header_generated)
		fclose(f);
	return 1;
}
