#if !defined(__TELETEXT_H)
#define __TELETEXT_H

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

#include "defines.h"

#ifdef ENABLE_TELETEXT

#include "dvb.h"

#include <stdlib.h>

/*******************
* EXPORTED MACROS  *
********************/

#define TELETEXT_PACKET_BUFFER_SIZE (5*TS_PACKET_SIZE)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/*******************
* EXPORTED DATA    *
********************/

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

void teletext_init();

/**
*   @brief Function takes PES packets from buffer of TS packets
* 
*   @param buf		I	Packets buffer
*   @param size	I	Buffer size
*/
void teletext_readPESPacket(unsigned char *buf, size_t size);

/* Displays teletext */
void teletext_displayTeletext();

#endif /* ENABLE_TELETEXT */

#endif
