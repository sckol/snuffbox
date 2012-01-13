#if !defined(__RUTUBE_H)
#define __RUTUBE_H

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
#include "interface.h"
#include "xmlconfig.h"

/*******************
* EXPORTED MACROS  *
********************/

#define RUTUBE_XML "/tmp/rutube_export.txt"

/*********************
* EXPORTED TYPEDEFS  *
**********************/
/** The structure of an element of asset list. There is the list of movie categories,
* keeping in RutubeCategories. Each category element has pArg pointer to the first asset element in
* current category. Each asset element has pointer to the next asset element - *nextInCategory.
*/
typedef struct __rutube_asset_t
{
	char *title;
	char *url;
	char *thumbnail;
	char *description;

	struct __rutube_asset_t *nextInCategory; /**< Next asset in it's category */
} rutube_asset_t;

/** Function to be called for each playlist entry on parsing
 *  Optional xmlConfigHandle_t parameter may contain pointer to XML structure of a given track.
 */
typedef void (*rutubeEntryHandler)(void *, const char *, const char *, const xmlConfigHandle_t);

/*******************
* EXPORTED DATA    *
********************/

extern interfaceListMenu_t RutubeCategories;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif
	void rutube_buildMenu(interfaceMenu_t* pParent);

	/** Prev/next Rutube movie
	  *  @param[in] pArg Ignored
	  *  @return 0 if successfully changed
	  *  @return 1 if there is no prev/next movie in menu
	 */
	int  rutube_startNextChannel(int direction, void* pArg);

	/** Set Rutube movie using currently downloaded video list
	 *  @param[in] pArg Ignored
	 *  @return 0 if successfully changed
	 */
	int  rutube_setChannel(int channel, void* pArg);

	/** Get permanent link to currently playing Rutube stream
	 * @return Statically allocated string, empty if Rutube is not active
	 */
	char *rutube_getCurrentURL();

	void rutube_cleanupMenu();
#ifdef __cplusplus
}
#endif

#endif /* __RUTUBE_H      Do not add any thing below this line */
