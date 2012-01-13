#if !defined(__DLNA_H)
#define __DLNA_H
#ifdef ENABLE_DLNA

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

/*******************
* INCLUDE FILES    *
********************/

#include "interface.h"

/*******************
* EXPORTED DATA    *
********************/

extern interfaceListMenu_t BrowseServersMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

int  dlna_start(void);
void dlna_stop(void);

int  dlna_initServerBrowserMenu(interfaceMenu_t *pMenu, void* pArg);
void dlna_buildDLNAMenu(interfaceMenu_t *pParent);
int  dlna_setChannel(int channel, void* pArg);
int  dlna_startNextChannel(int direction, void* pArg);

#endif // #ifdef ENABLE_DLNA
#endif /* __DLNA_H      Do not add any thing below this line */
