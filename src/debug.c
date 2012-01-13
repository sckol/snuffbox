
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include "debug.h"

#ifdef ALLOC_DEBUG

#define MAX_ALLOCS (256)

typedef struct _alloc_info_t {
	unsigned int addr;
	unsigned int present;
	char source[32];
} alloc_info_t;

static int initialized = 0;
static alloc_info_t allocs[MAX_ALLOCS];

static void dbg_cat_alloc(void)
{
	int i = 0;
	int count = 0;

	if (initialized == 0)
	{
		memset(allocs, 0, sizeof(allocs));
		initialized = 1;
	}

	printf("==========ALLOCS==============\n");

	for (i=0; i<MAX_ALLOCS; i++)
	{
		if (allocs[i].present != 0)
		{
			printf(" [%08X]%s", allocs[i].addr, allocs[i].source);
			count++;
			if (count > 0 && count % 2 == 0)
			{
				printf("\n");
			}
		}
	}

	printf("\n------------------------------\n");
}

static void dbg_add_alloc(void *ptr, const char *source)
{
	int found = 0;
	int available = -1;
	int i;

	if (initialized == 0)
	{
		memset(allocs, 0, sizeof(allocs));
		initialized = 1;
	}

	for (i=0; i<MAX_ALLOCS; i++)
	{
		if (allocs[i].addr == (unsigned int)ptr && allocs[i].present != 0)
		{
			found = 1;
			eprintf("already found %08X in alloc map\n", (unsigned int)ptr);
			return;
		}
		if (available == -1 && allocs[i].present == 0)
		{
			available = i;
		}
	}

	if (available >= 0)
	{
		allocs[available].addr = (unsigned int)ptr;
		allocs[available].present = 1;
		strcpy(allocs[available].source, source);
	} else {
		eprintf("too many allocs\n");
	}
}

static void dbg_del_alloc(void *ptr)
{
	int found = 0;
	int i;

	if (initialized == 0)
	{
		memset(allocs, 0, sizeof(allocs));
		initialized = 1;
	}

	for (i=0; i<MAX_ALLOCS; i++)
	{
		if (allocs[i].addr == (unsigned int)ptr && allocs[i].present != 0)
		{
			found = 1;
			allocs[i].present = 0;
			allocs[i].addr = 0;
			return;
		}
	}

	if (found == 0)
	{
		eprintf("can't find %08X in alloc map\n", (unsigned int)ptr);
	}
}

void *dbg_calloc(size_t nmemb, size_t size, const char *location)
{
	void *p = calloc(nmemb, size);

	dbg_add_alloc(p, location);
	printf("%08X CALLOC by %s\n", (unsigned int)p, location);
	fflush(stdout);

	dbg_cat_alloc();

	return p;
}

void *dbg_malloc(size_t size, const char *location)
{
	void *p = malloc(size);

	dbg_add_alloc(p, location);
	printf("%08X MALLOC by %s\n", (unsigned int)p, location);
	fflush(stdout);

	dbg_cat_alloc();

	return p;
}

void dbg_free(void *ptr, const char *location)
{
	dbg_del_alloc(ptr);
	printf("%08X FREE by %s\n", (unsigned int)ptr, location);
	fflush(stdout);

	free(ptr);

	dbg_cat_alloc();
}

void *dbg_realloc(void *ptr, size_t size, const char *location)
{
	void *p = realloc(ptr, size);

	dbg_del_alloc(ptr);
	printf("%08X FREE-REALLOC by %s\n", (unsigned int)ptr, location);
	dbg_add_alloc(p, location);
	printf("%08X MALLOC-REALLOC by %s\n", (unsigned int)p, location);
	fflush(stdout);

	dbg_cat_alloc();

	return p;
}

#endif // #ifdef ALLOC_DEBUG
