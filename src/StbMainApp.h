
/*

Elecard STB820 Demo Application
Copyright (C) 2007  Elecard Devices

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA

*/

/****************
* INCLUDE FILES *
*****************/

#include <time.h>

#include "interface.h"
#include "debug.h"

/*******************
* EXPORTED MACROS  *
********************/

#ifdef STB225
	#define DID_KEYBOARD (16)
#else
	#define DID_KEYBOARD (0)
#endif

#define DID_FRONTPANEL (4)

#define DID_STANDBY (5)

#define FREE(x) if(x) { dfree(x); (x) = NULL; }

/*********************
* EXPORTED TYPEDEFS  *
**********************/

extern volatile int keepCommandLoopAlive;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

int  helperFileExists(const char* filename);

int  helperStartApp(const char* filename);

int  helperReadLine(int file, char* buffer);

int  helperParseLine(const char *path, const char *cmd, const char *pattern, char *out, char stopChar);

int  helperParseMmio(int addr);

/** strcpy without characters unsupported by FAT filesystem
 * @param[out] dst Destination pointer
 * @param[in]  src Source stream
 * @return     dst
 */
char *helperStrCpyTrimSystem(char *dst, char *src);

void helperFlushEvents();

interfaceCommand_t helperGetEvent(int flush);

int  helperCheckDirectoryExsists(const char *path);

char *helperEthDevice(int i);

void signal_handler(int signal);

void tprintf(const char *str, ...);

int  helperCheckUpdates();

/** Copy src string to new dest buffer.
 * @param[out] dest If already allocated and have enough space, will be used without changing pointer. If buffer will not be big enough, realloc would be used on *dest.
 * @param[in]  stc  If null, *dest will be freed and nulled.
 * @return 0 if src was copied successfully
 */
int  helperSafeStrCpy( char** dest, const char* src );

/** UTC equivalent for mktime() */
time_t gmktime(struct tm *t);

#ifdef __cplusplus
}
#endif
