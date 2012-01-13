#ifndef __PLAYLIST_H
#define __PLAYLIST_H

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

/** @file playlist.h Playlists and Favorites backend
 */
/** @defgroup playlist Playlists and Favorites features
 *  @ingroup StbMainApp
 */

/****************
* INCLUDE FILES *
*****************/

#include "interface.h"
#include "xspf.h"

/*******************
* EXPORTED MACROS  *
********************/

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

#define PLAYLIST_ERR_OK         (0)
#define PLAYLIST_ERR_DOWNLOAD   (1)
#define PLAYLIST_ERR_PARSE      (2)

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern interfaceListMenu_t playlistMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

void playlist_buildMenu(interfaceMenu_t *pParent);

int playlist_addUrl(char* url, char *description);

int playlist_setLastUrl(char* url);

int playlist_startNextChannel(int direction, void* pArg);

int playlist_setChannel(int channel, void* pArg);

int playlist_streamStart();

char *playlist_getLastURL();

/** @ingroup playlist
 *  Download and parse playlist from specified URL
 *  @param[in]  url            URL of the playlist
 *  @param[in]  pEntryCallback Function to be called for each playlist entry
 *  @param[in]  pArg           User data to be used in callback
 *  @return     0 on success
 *  @sa         playlist_getFromBuffer()
 */
int playlist_getFromURL(const char *url, xspfEntryHandler pEntryCallback, void *pArg);

/** @ingroup playlist
 *  Parse playlist from buffer
 *  @param[in]  data           Buffer with playlist data
 *  @param[in]  size           Size of buffer
 *  @param[in]  pEntryCallback Function to be called for each playlist entry
 *  @param[in]  pArg           User data to be used in callback
 *  @return     0 on success
 *  @sa         playlist_getFromURL()
 */
int playlist_getFromBuffer(const char *data, const size_t size, xspfEntryHandler pEntryCallback, void *pArg);

#ifdef __cplusplus
}
#endif

#endif // __PLAYLIST_H
