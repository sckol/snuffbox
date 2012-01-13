
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

#include "defines.h"
#include "platform.h"
#include "common.h"

/*******************
* EXPORTED MACROS  *
********************/

#ifdef TRACE
	#define TRACEE printf("ENTER: %s\n", __FUNCTION__);
	#define TRACEL printf("LEAVE: %s\n", __FUNCTION__);
	#define TRACEF printf("FAILE: %s\n", __FUNCTION__);
#else
	#define TRACEE
	#define TRACEL
	#define TRACEF
#endif

#ifdef ALLOC_DEBUG
	#define dcalloc(x,y)	dbg_calloc(x, y, __FUNCTION__)
	#define dmalloc(x)		dbg_malloc(x, __FUNCTION__)
	#define	dfree(x)		dbg_free(x, __FUNCTION__)
	#define drealloc(x,y)	dbg_realloc(x, y, __FUNCTION__)
#else
	#define dcalloc(x,y)	calloc(x, y)
	#define dmalloc(x)		malloc(x)
	#define	dfree(x)		free(x)
	#define drealloc(x,y)	realloc(x, y)
#endif

/*********************************
* EXPORTED FUNCTION DEFINITIONS  *
**********************************/

#ifdef __cplusplus
extern "C" {
#endif

void *dbg_calloc(size_t nmemb, size_t size, const char *location);
void *dbg_malloc(size_t size, const char *location);
void dbg_free(void *ptr, const char *location);
void *dbg_realloc(void *ptr, size_t size, const char *location);

#ifdef __cplusplus
};
#endif
