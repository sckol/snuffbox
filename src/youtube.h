#if !defined(__YOUTUBE_H)
#define __YOUTUBE_H

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

#include <directfb.h>
#include "interface.h"

/*******************
* EXPORTED MACROS  *
********************/

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef void (*youtubeEntryHandler)(const char *, const char *, const char *);

/*******************
* EXPORTED DATA    *
********************/

extern interfaceListMenu_t YoutubeMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif
	void youtube_buildMenu(interfaceMenu_t* pParent);

	/** Prev/next Youtube channel
	 *  @param[in] pArg Ignored
	 *  @return 0 if successfully changed
	 */
	int  youtube_startNextChannel(int direction, void* pArg);

	/** Set Youtube channel using currently downloaded video list
	 *  @param[in] pArg Ignored
	 *  @return 0 if successfully changed
	 */
	int  youtube_setChannel(int channel, void* pArg);

	/** Get permanent link to currently playing YouTube stream
	 * @return Statically allocated string, empty if YouTube is not active
	 */
	char *youtube_getCurrentURL();

	/** Play YouTube stream, predefined in appControlInfo.mediaInfo.lastFile
	 * @return 0 on success
	 */
	int youtube_streamStart();
#ifdef __cplusplus
}
#endif

#endif /* __YOUTUBE_H      Do not add any thing below this line */
