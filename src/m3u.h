#ifndef __M3U_H
#define __M3U_H

/*

Elecard STB820 Demo Application
Copyright (C) 2009  Elecard Devices

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

#include <stdio.h>

#include "defines.h"
#include "app_info.h"
#include "interface.h"

/*******************
* EXPORTED MACROS  *
********************/

#define EXTM3U  "#EXTM3U"
#define EXTINF  "#EXTINF"

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

typedef struct __m3uEntry_t
{
	char title[MENU_ENTRY_INFO_LENGTH];
	char url[MAX_URL];
} m3uEntry_t;

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern char m3u_description[MENU_ENTRY_INFO_LENGTH];
extern char m3u_url[MAX_URL];

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

int m3u_readEntry(FILE* f);
FILE*  m3u_initFile(char *m3u_filename, const char* mode);
int m3u_addEntry(char *m3u_filename, char *url, char *description);
int m3u_getEntry(char *m3u_filename, int selected);
int m3u_findUrl(char *m3u_filename, char *url);
int m3u_createFile(char *m3u_filename);
int m3u_deleteEntryByIndex(char *m3u_filename, int index);
int m3u_deleteEntryByUrl(char *m3u_filename, char *url);
int m3u_replaceEntryByIndex(char *m3u_filename, int index, char *url, char *description);
int m3u_readEntryFromBuffer(char **pData, int *length);

#endif //__M3U_H
