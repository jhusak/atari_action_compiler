#ifndef DEVICES_H_
#define DEVICES_H_

#include <stdio.h> /* FILENAME_MAX */
//#include "atari.h" /* UWORD */
#define UWORD uint16_t

#define Util_DIR_SEP_STR "/"
int Devices_Initialise(int *argc, char *argv[]);
void Devices_Exit(void);
int Devices_PatchOS(void);
void Devices_Frame(void);
void Devices_UpdatePatches(void);


extern uint8_t force_write;

UWORD Devices_SkipDeviceName(void);

extern char Devices_atari_h_dir[FILENAME_MAX];
extern int Devices_h_read_only;

extern char Devices_h_exe_path[FILENAME_MAX];

extern char Devices_h_device_name;

extern char Devices_h_current_dir[FILENAME_MAX];

int Devices_H_CountOpen(void);
void Devices_H_CloseAll(void);
void Devices_H_Status(void);
void Devices_H_Open(void);
void Devices_H_Close(void);
void Devices_H_Read(void);
void Devices_H_Write(void);
#define H_DEVICE_BEGIN  0xd140
#define H_TABLE_ADDRESS 0xd140
#define H_PATCH_OPEN    0xd150
#define H_PATCH_CLOS    0xd153
#define H_PATCH_READ    0xd156
#define H_PATCH_WRIT    0xd159
#define H_PATCH_STAT    0xd15c
#define H_PATCH_SPEC    0xd15f
#define H_DEVICE_END    0xd161


#define	Devices_ICHIDZ	0x0020
#define	Devices_ICDNOZ	0x0021
#define	Devices_ICCOMZ	0x0022
#define	Devices_ICSTAZ	0x0023
#define	Devices_ICBALZ	0x0024
#define	Devices_ICBAHZ	0x0025
#define	Devices_ICPTLZ	0x0026
#define	Devices_ICPTHZ	0x0027
#define	Devices_ICBLLZ	0x0028
#define	Devices_ICBLHZ	0x0029
#define	Devices_ICAX1Z	0x002a
#define	Devices_ICAX2Z	0x002b

#define Devices_IOCB0   0x0340
#define	Devices_ICHID	0x0000
#define	Devices_ICDNO	0x0001
#define	Devices_ICCOM	0x0002
#define	Devices_ICSTA	0x0003
#define	Devices_ICBAL	0x0004
#define	Devices_ICBAH	0x0005
#define	Devices_ICPTL	0x0006
#define	Devices_ICPTH	0x0007
#define	Devices_ICBLL	0x0008
#define	Devices_ICBLH	0x0009
#define	Devices_ICAX1	0x000a
#define	Devices_ICAX2	0x000b
#define	Devices_ICAX3	0x000c
#define	Devices_ICAX4	0x000d
#define	Devices_ICAX5	0x000e
#define	Devices_ICAX6	0x000f

#define Devices_TABLE_OPEN	0
#define Devices_TABLE_CLOS	2
#define Devices_TABLE_READ	4
#define Devices_TABLE_WRIT	6
#define Devices_TABLE_STAT	8
#define Devices_TABLE_SPEC	10
#define Devices_TABLE_INIT	12

UWORD Devices_UpdateHATABSEntry(char device, UWORD entry_address, UWORD table_address);
void Devices_RemoveHATABSEntry(char device, UWORD entry_address, UWORD table_address);

#endif /* DEVICES_H_ */
