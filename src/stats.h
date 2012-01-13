#ifndef __STATS_H
#define __STATS_H

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

#include "defines.h"

#ifdef ENABLE_STATS

#include <time.h>
#include "interface.h"

/*******************
* EXPORTED MACROS  *
********************/

#define STATS_PATH     "/config/StbMainApp/"
#define STATS_FILE     "/config/StbMainApp/stats.txt"
#define STATS_TMP_FILE "/config/StbMainApp/stats_today.txt"
#define STATS_RESOLUTION      (60)
#define STATS_SAMPLE_COUNT    (24*60*60 / STATS_RESOLUTION)
#define STATS_UPDATE_INTERVAL (1000*STATS_RESOLUTION)

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

typedef struct __statsInfo_t
{
	time_t startTime;
	time_t endTime;
	time_t today;
	int watched[STATS_SAMPLE_COUNT];
} statsInfo_t;

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern statsInfo_t statsInfo;
extern interfaceListMenu_t StatsMenu;
extern time_t stats_lastTime;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

int stats_buildMenu(interfaceMenu_t* pParent);
int stats_init();
int stats_save();
int stats_load();

#endif //ENABLE_STATS
#endif //__STATS_H
