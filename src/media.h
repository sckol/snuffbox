#if !defined(__MEDIA_H)
#define __MEDIA_H

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
* EXPORTED MACROS  *
********************/

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/*******************
* EXPORTED DATA    *
********************/

extern const char usbRoot[];
extern char       currentPath[];
extern interfaceListMenu_t MediaMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

int media_startPlayback();

/**
*   @brief Function used to initialize Media play control and start playback of appControlInfo.mediaInfo.filename
*
*   @retval void
*/
int media_streamStart(void);

/**
*   @brief Function used to stop Media playback
*
*   @retval void
*/
void media_stopPlayback(void);

/**
*   @brief Function used to build the media menu data structures
*
*   @retval void
*/
void media_buildMediaMenu(interfaceMenu_t *pParent);

void media_cleanupMenu(void);

/**
*   @brief Function used to build the media progress slider data structure
*
*   @retval void
*/
void media_buildSlider(void);

/**
*   @brief Function used to set the current media playback position
*
*   @param  value		IN		Location within media file (percentage)
*
*   @retval void
*/
void media_setPosition(long value);

/**
*   @brief Function used to play custom path or URL
*
*   @param  which		IN		Screen number
*   @param  URL			IN		Should be not null
*   @param  description	IN		If not null, description of playing stream to be set to
*   @param  thumbnail	IN		If not null, custom thumbnail to be set to
*
*   @retval void
*/
int  media_playURL(int which, char* URL, char *description, char* thumbnail);

int  media_slideshowStart(void);

int  media_slideshowStop(int disable);

int  media_slideshowNext(int direction);

int  media_slideshowSetMode(int mode);

int  media_slideshowSetTimeout(int timeout);

int  media_initUSBBrowserMenu(interfaceMenu_t *pMenu, void* pArg);

int  media_initSambaBrowserMenu(interfaceMenu_t *pMenu, void* pArg);

int  media_setMode(interfaceMenu_t *pMenu, void *pArg);

/**
*   @brief Function used to determine count of USB flash or hard disk drives (not CD drives)
*
*   @retval int Storage count
*/
int  media_scanStorages(void);

void media_storagesChanged(void);

/**
*   @brief Function used to determine media type from file extension
*
*   @return mediaAll if extension is unknown
*/
mediaType media_getMediaType(char *filename);

/** Callback for scandir function, used to select mounted USB storages
 */
int media_select_usb(const struct dirent * de);

#ifdef __cplusplus
}
#endif

#endif /* __MEDIA_H      Do not add any thing below this line */
