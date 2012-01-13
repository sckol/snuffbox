#ifndef __VOIP_H
#define __VOIP_H

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

/*******************
* EXPORTED MACROS  *
********************/

#define MAX_VOIP_ADDRESS_LENGTH (128)

#define VOIP_SOCKET	"/tmp/voip.socket"

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

typedef struct __voipEntry_t
{
	char title[MENU_ENTRY_INFO_LENGTH];
	char uri[MAX_VOIP_ADDRESS_LENGTH];
} voipEntry_t;

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern interfaceListMenu_t VoIPMenu;
extern interfaceListMenu_t AddressBookMenu;
extern interfaceListMenu_t MissedCallsMenu;
extern interfaceListMenu_t AnsweredCallsMenu;
extern interfaceListMenu_t DialedNumbersMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

void voip_init();
void voip_cleanup();
void voip_buildMenu(interfaceMenu_t *pParent);
int voip_dialNumber(interfaceMenu_t *pMenu, void *pArg);
int voip_fillMenu(interfaceMenu_t *pMenu, void *pArg);
int voip_answerCall(interfaceMenu_t *pMenu, void *pArg);
int voip_hangup(interfaceMenu_t *pMenu, void *pArg);
void voip_setBuzzer();

#endif //__VOIP_H
