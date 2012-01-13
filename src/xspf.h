#if !defined(__XSPF_H)
#define __XSPF_H

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

#include "xmlconfig.h"

/*******************
* EXPORTED MACROS  *
********************/

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/** Function to be called for each playlist entry on parsing
 *  Optional xmlConfigHandle_t parameter may contain pointer to XML structure of a given track.
 */
typedef void (*xspfEntryHandler)(void *, const char *, const char *, const xmlConfigHandle_t);

/*******************
* EXPORTED DATA    *
********************/

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

/** Function used to parse XSPF data from buffer.
 * @param[in] data      Buffer containing XSPF data
 * @param[in] pCallback Function to be called for each playlist entry
 * @param[in] pArg      User data to be passed to be used in callback
 * @return    0 on success
 */
int xspf_parseBuffer(const char *data, xspfEntryHandler pCallback, void *pArg);

#ifdef __cplusplus
}
#endif

#endif /* __XSPF_H      Do not add any thing below this line */
