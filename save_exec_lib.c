#include <stdio.h>
#include <stdint.h>

extern uint8_t visited[65536];
extern uint8_t read6502(uint16_t addr);

static void write_word_le(FILE *f, uint16_t v)
{
	fputc(v & 0xff, f);
	fputc(v >> 8, f);
}

int save_used_functions_as_executable(const char *filename)
{
	FILE *f;
	uint32_t addr;

	f = fopen(filename, "wb");
	if (!f)
		return 0;

	/* preambuła Atari DOS */
	write_word_le(f, 0xFFFF);

	addr = 0xA000;

	while (addr < 0xC000)
	{
		/* szukaj początku segmentu */
		while (addr < 0xC000 && !visited[addr])
			addr++;

		if (addr >= 0xC000)
			break;

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
				 * Dzielimy dopiero gdy mamy >=5 zer.
				 * Pierwsze 4 zera zostają w segmencie.
				 */
				if (gap >= 5)
					break;

				//end = addr;
			}
		}

		write_word_le(f, (uint16_t)start);
		write_word_le(f, (uint16_t)end);

		for (uint32_t a = start; a <= end; a++)
			fputc(read6502((uint16_t)a), f);
	}

	fclose(f);
	return 1;
}
