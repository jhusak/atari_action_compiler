/*
 * devices.c - emulation of H:, P:, E: and K: Atari devices
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2010 Atari800 development team (see DOC/CREDITS)
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* Bibliography:
   [OSMAN] - Atari Home Computer System Technical Reference Notes - Operating System
             User's Manual - CA016555 Rev. A - 1982 Atari, Inc.
 */
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
//#include <file.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "devices.h"
#include "fake6502.h"
#define TRUE 1
#define FALSE 0
#define UWORD	uint16_t
#define UBYTE	uint8_t
#define CPU_regY	Y
#define CPU_regX	X
#define CPU_regA	A

#define Util_fclose(fp, tmpbuf)             fclose(fp)
#define Util_fopen(filename, mode, tmpbuf)  fopen(filename, mode)
#define Util_DIR_SEP_CHAR '/'
#define MEMORY_dGetByte(x)				read6502(x)
#define MEMORY_dGetWord(x)				read6502word(x)
#define MEMORY_dGetWordAligned(x)		MEMORY_dGetWord(x)
#define MEMORY_dPutByte(x,y)		write6502(x,y)
#define MEMORY_dPutWord(x,y)		write6502word(x,y)
/* H: device emulation --------------------------------------------------- */

/* host path for each H: unit */
char Devices_atari_h_dir[FILENAME_MAX];

/* read only mode for H: device */
int Devices_h_read_only = FALSE;

/* H device rename; one can add 'D' in command line */
char Devices_h_device_name = 'D';

/* Devices_h_current_dir must be empty or terminated with Util_DIR_SEP_CHAR;
   only Util_DIR_SEP_CHAR can be used as a directory separator here */
char Devices_h_current_dir[FILENAME_MAX];

/* stream open via H: device per IOCB */
static FILE *h_fp[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

/* H: text mode per IOCB */
static int h_textmode[8]={0};

/* H: last read character per IOCB */
static int h_lastbyte[8];

/* last read character was CR, per IOCB */
static int h_wascr[8];

/* last operation: 'o': open, 'r': read, 'w': write, 'p': point, 'b': binary
   load, per IOCB. This is needed to apply fseek(fp, 0, SEEK_CUR) between reads
   and writes in update (12) mode, and to support the read-ahead of 1 byte
   in Devices_h_read. */
static char h_lastop[8];

/* IOCB #, 0-7 */
static int h_iocb;

/* filename as specified after "Hn:" */
static char atari_filename[FILENAME_MAX];

#ifdef DO_RENAME
/* new filename (no directories!) */
static char new_filename[FILENAME_MAX];
#endif

/* atari_filename applied to H:'s current dir, with Util_DIR_SEP_CHARs only */
static char atari_path[FILENAME_MAX];

/* full filename for the current operation */
static char host_path[FILENAME_MAX];

int Devices_H_CountOpen(void)
{
	int r = 0;
	int i;
	for (i = 0; i < 8; i++)
		if (h_fp[i] != NULL)
			r++;
	return r;
}

void Devices_H_CloseAll(void)
{
	int i;
	for (i = 0; i < 8; i++)
		if (h_fp[i] != NULL) {
			Util_fclose(h_fp[i], h_tmpbuf[i]);
			h_fp[i] = NULL;
		}
}


void Devices_Exit(void)
{
	Devices_H_CloseAll();
}

#define IS_DIR_SEP(c) ((c) == '/' || (c) == '\\' || (c) == ':' || (c) == '>')

static int Devices_IsValidForFilename(char ch)
{
	if ((ch >= 'A' && ch <= 'Z')
	 || (ch >= 'a' && ch <= 'z')
	 || (ch >= '0' && ch <= '9'))
		return TRUE;
	switch (ch) {
	case '!':
	case '#':
	case '$':
	case '&':
	case '\'':
	case '(':
	case ')':
	case '*':
	case '-':
	case '.':
	case '?':
	case '@':
	case '_':
		return TRUE;
	default:
		return FALSE;
	}
}

UWORD Devices_SkipDeviceName(void)
{
	UWORD bufadr;
	for (bufadr = MEMORY_dGetWordAligned(Devices_ICBALZ); ; bufadr++) {
		char c = (char) MEMORY_dGetByte(bufadr);
		if (c == ':')
			return (UWORD) (bufadr + 1);
		if (c < '!' || c > '\x7e')
			return 0;
	}
}

static UWORD Devices_GetAtariPath(char *p)
{
	UWORD bufadr = Devices_SkipDeviceName();
	if (bufadr != 0) {
		while (p < atari_filename + sizeof(atari_filename) - 1) {
			char c = (char) MEMORY_dGetByte(bufadr);
			if (Devices_IsValidForFilename(c) || IS_DIR_SEP(c) || c == '<') {
				*p++ = c;
				bufadr++;
			}
			else {
				/* end of filename */
				/* now apply it to Devices_h_current_dir */
				const char *q = atari_filename;
				*p = '\0';
				if (IS_DIR_SEP(*q)) {
					/* absolute path on H: device */
					q++;
					p = atari_path;
				}
				else {
					strcpy(atari_path, Devices_h_current_dir);
					p = atari_path + strlen(atari_path);
				}
				for (;;) {
					/* we are here at the beginning of a path element,
					   i.e. at the beginning of atari_path or after Util_DIR_SEP_CHAR */
					if (*q == '<'
					 || (*q == '.' && q[1] == '.' && (q[2] == '\0' || IS_DIR_SEP(q[2])))) {
						/* "<" or "..": parent directory */
						if (p == atari_path) {
							CPU_regY = 150; /* Sparta: directory not found */
							CPU_SetN;
							return 0;
						}
						do
							p--;
						while (p > atari_path && p[-1] != Util_DIR_SEP_CHAR);
						if (*q == '.') {
							if (q[2] != '\0')
								q++;
							q++;
						}
						q++;
						continue;
					}
					if (IS_DIR_SEP(*q)) {
						/* duplicate DIR_SEP */
						CPU_regY = 165; /* bad filename */
						CPU_SetN;
						return 0;
					}
					do {
						if (p >= atari_path + sizeof(atari_path) - 1) {
							CPU_regY = 165; /* bad filename */
							CPU_SetN;
							return 0;
						}
						*p++ = *q;
						if (*q == '\0')
							return bufadr;
						q++;
					} while (!IS_DIR_SEP(*q));
					*p++ = Util_DIR_SEP_CHAR;
					q++;
				}
			}
		}
	}
	CPU_regY = 165; /* bad filename */
	CPU_SetN;
	return 0;
}

static int Devices_GetIOCB(void)
{
	if ((CPU_regX & 0x8f) != 0) {
		CPU_regY = 134; /* invalid IOCB number */
		CPU_SetN;
		return FALSE;
	}
	h_iocb = CPU_regX >> 4;
	return TRUE;
}

void Util_catpath(char *result, const char *path1, const char *path2)
{
        snprintf(result, FILENAME_MAX,
                path1[0] == '\0' || path2[0] == Util_DIR_SEP_CHAR || path1[strlen(path1) - 1] == Util_DIR_SEP_CHAR
                        ? "%s%s" : "%s" Util_DIR_SEP_STR "%s", path1, path2);
}

static UWORD Devices_GetHostPath(int set_textmode)
{
	UWORD bufadr;
	bufadr = Devices_GetAtariPath(atari_filename);
	if (bufadr == 0)
		return 0;
	Util_catpath(host_path, Devices_atari_h_dir, atari_path);
	return bufadr;
}

void Devices_H_Open(void)
{
	FILE *fp;
	UBYTE aux1;

	if (Devices_GetHostPath(TRUE) == 0)
		return;

	if (!Devices_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL)
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);

	fp = NULL;
	h_wascr[h_iocb] = FALSE;
	h_lastop[h_iocb] = 'o';

	aux1 = MEMORY_dGetByte(Devices_ICAX1Z);
	switch (aux1) {
	case 4:
		/* don't bother using "r" for textmode:
		   we want to support LF, CR/LF and CR, not only native EOLs */
		fp = Util_fopen(host_path, "rb", h_tmpbuf[h_iocb]);
		if (fp != NULL) {
			CPU_regY = 1;
			CPU_ClrN;
		}
		else {
			CPU_regY = 170; /* file not found */
			CPU_SetN;
		}
		break;
	case 8: /* write: "w" */
	case 9: /* write at end of file (append): "a" */
	case 12: /* write and read (update): "r+" || "w+" */
	case 13: /* append and read: "a+" */
		if (Devices_h_read_only) {
			CPU_regY = 163; /* disk write-protected */
			CPU_SetN;
			break;
		}
		{
			char mode[4];
			char *p = mode + 1;
			mode[0] = (aux1 & 1) ? 'a' : (aux1 < 12) ? 'w' : 'r';
			if (!h_textmode[h_iocb])
				*p++ = 'b';
			if (aux1 >= 12)
				*p++ = '+';
			*p = '\0';
			fp = Util_fopen(host_path, mode, h_tmpbuf[h_iocb]);
			if (fp == NULL && aux1 == 12) {
				mode[0] = 'w';
				fp = Util_fopen(host_path, mode, h_tmpbuf[h_iocb]);
			}
		}
		if (fp != NULL) {
			CPU_regY = 1;
			CPU_ClrN;
		}
		else {
			CPU_regY = 144; /* device done error */
			CPU_SetN;
		}
		break;
	default:
		CPU_regY = 168; /* invalid device command */
		CPU_SetN;
		break;
	}
	h_fp[h_iocb] = fp;
}

void Devices_H_Close(void)
{
	if (!Devices_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		Util_fclose(h_fp[h_iocb], h_tmpbuf[h_iocb]);
		h_fp[h_iocb] = NULL;
	}
	CPU_regY = 1;
	CPU_ClrN;
}

void Devices_H_Read(void)
{
	if (!Devices_GetIOCB())
		return;

	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] != 'r') {
			if (h_lastop[h_iocb] == 'w')
				fseek(h_fp[h_iocb], 0, SEEK_CUR);
			h_lastbyte[h_iocb] = fgetc(h_fp[h_iocb]);
			h_lastop[h_iocb] = 'r';
		}
		ch = h_lastbyte[h_iocb];
		if (ch != EOF) {
			if (h_textmode[h_iocb]) {
				switch (ch) {
				case 0x0d:
					h_wascr[h_iocb] = TRUE;
					ch = 0x9b;
					break;
				case 0x0a:
					if (h_wascr[h_iocb]) {
						/* ignore LF next to CR */
						ch = fgetc(h_fp[h_iocb]);
						if (ch != EOF) {
							if (ch == 0x0d) {
								h_wascr[h_iocb] = TRUE;
								ch = 0x9b;
							}
							else
								h_wascr[h_iocb] = FALSE;
						}
						else {
							CPU_regY = 136; /* end of file */
							CPU_SetN;
							break;
						}
					}
					else
						ch = 0x9b;
					break;
				default:
					h_wascr[h_iocb] = FALSE;
					break;
				}
			}
			CPU_regA = (UBYTE) ch;
			/* [OSMAN] p. 79: Status should be 3 if next read would yield EOF.
			   But to set the stream's EOF flag, we need to read the next byte. */
			h_lastbyte[h_iocb] = fgetc(h_fp[h_iocb]);
			CPU_regY = feof(h_fp[h_iocb]) ? 3 : 1;
			CPU_ClrN;
		}
		else {
			CPU_regY = 136; /* end of file */
			CPU_SetN;
		}
	}
	else {
		CPU_regY = 136; /* end of file; XXX: this seems to be what Atari DOSes return */
		CPU_SetN;
	}
}

void Devices_H_Write(void)
{
	if (!Devices_GetIOCB())
		return;
	if (h_fp[h_iocb] != NULL) {
		int ch;
		if (h_lastop[h_iocb] == 'r')
			fseek(h_fp[h_iocb], 0, SEEK_CUR);
		h_lastop[h_iocb] = 'w';
		ch = CPU_regA;
		if (ch == 0x9b && h_textmode[h_iocb])
			ch = '\n';
		fputc(ch, h_fp[h_iocb]);
		CPU_regY = 1;
		CPU_ClrN;
	}
	else {
		CPU_regY = 135; /* attempted to write to a read-only device */
		            /* XXX: this seems to be what Atari DOSes return */
		CPU_SetN;
	}
}

void Devices_H_Status(void)
{

	CPU_regY = 146; /* function not implemented in handler; XXX: check file existence? */
	CPU_SetN;
}




/* New handling of H: device.
   Previously we simply replaced C: device in OS with our H:.
   Now we don't change ROM for H: patch, but add H: to HATABS in RAM
   and put the device table and patches in unused address space
   (0xd100-0xd1ff), which is meant for 'new devices' (like hard disk).
   We have to continuously check if our H: is still in HATABS,
   because RESET routine in Atari OS clears HATABS and initializes it
   using a table in ROM (see Devices_PatchOS).
   Before we put H: entry in HATABS, we must make sure that HATABS is there.
   For example a program that doesn't use Atari OS can use this memory area
   for its own data, and we shouldn't place 'H' there.
   We also allow an Atari program to change address of H: device table.
   So after we put H: entry in HATABS, we only check if 'H' is still where
   we put it (h_entry_address).
   Devices_UpdateHATABSEntry and Devices_RemoveHATABSEntry can be used to add
   other devices than H:.
   Keep in mind that H: can be renamed to whatever you want in command line.
   */

#define HATABS 0x31a

UWORD Devices_UpdateHATABSEntry(char device, UWORD entry_address,
							   UWORD table_address)
{
	UWORD address;
	if (entry_address != 0 && MEMORY_dGetByte(entry_address) == device)
		return entry_address;
	if (MEMORY_dGetByte(HATABS) != 'P' || MEMORY_dGetByte(HATABS + 3) != 'C'
		|| MEMORY_dGetByte(HATABS + 6) != 'E' || MEMORY_dGetByte(HATABS + 9) != 'S'
		|| MEMORY_dGetByte(HATABS + 12) != 'K')
		return entry_address;
	for (address = HATABS + 15; address < HATABS + 33; address += 3) {
		if (MEMORY_dGetByte(address) == device)
			return address;
		if (MEMORY_dGetByte(address) == 0) {
			MEMORY_dPutByte(address, device);
			MEMORY_dPutWord(address + 1, table_address);
			return address;
		}
	}
	/* HATABS full */
	return entry_address;
}

void Devices_UpdatePatches(void)
{
	/* change memory attributes for the area, where we put
	   the H: handler table and patches */
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_OPEN, H_PATCH_OPEN - 1);
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_CLOS, H_PATCH_CLOS - 1);
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_READ, H_PATCH_READ - 1);
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_WRIT, H_PATCH_WRIT - 1);
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_STAT, H_PATCH_STAT - 1);
	MEMORY_dPutWord(H_TABLE_ADDRESS + Devices_TABLE_SPEC, H_PATCH_SPEC - 1);
}

static UWORD h_entry_address = 0;

void Devices_Frame(void)
{
	h_entry_address = Devices_UpdateHATABSEntry(Devices_h_device_name, h_entry_address, H_TABLE_ADDRESS);
	Devices_UpdatePatches();
}

/*
vim:ts=4:sw=4:
*/
