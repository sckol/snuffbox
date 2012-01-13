#ifndef __MENU_APP_H
#define __MENU_APP_H

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

extern interfaceListMenu_t interfaceMainMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

void menu_init();
void menu_cleanup();
int  open_browser(interfaceMenu_t* pMenu, void* pArg);

#endif /* __MENU_APP_H      Do not add any thing below this line */
