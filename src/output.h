#if !defined(__OUTPUT_H)
#define __OUTPUT_H

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
#include "app_info.h"

/*******************
* EXPORTED MACROS  *
********************/

#define MAX_PASSWORD_LENGTH (128)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef enum _colorSetting_t
{
	colorSettingSaturation = 0,
	colorSettingContrast = 1,
	colorSettingBrightness = 2,
	colorSettingHue = 3,
	colorSettingCount = 3, // don't include Hue for now...
} colorSetting_t;

/*******************
* EXPORTED DATA    *
********************/

extern stbTimeZoneDesc_t timezones[];
extern interfaceListMenu_t OutputMenu;
#ifdef ENABLE_DVB
extern interfaceListMenu_t DVBSubMenu;
#endif

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

/**
*   @brief Function used to set up the output menu
*
*   @retval void
*/
void output_buildMenu(interfaceMenu_t *pParent);

#ifdef ENABLE_DVB
int output_fillDVBMenu(interfaceMenu_t *pMenu, void* pArg);
#endif

#ifdef ENABLE_PROVIDER_PROFILES
/** Check provider profile is already selected, and if not set Profile menu as current
 * @return 0 if profile is selected
 */
int output_checkProfile(void);
#endif

/**
*   @brief Function used to clean up leading zeroes in IP addresses
*
*   @retval value
*/
char* inet_addr_prepare( char *value);

long output_getColorValue(void *pArg);
void output_setColorValue(long value, void *pArg);

int  getParam(const char *path, const char *param, const char *defaultValue, char *output);
int  setParam(const char *path, const char *param, const char *value);

#ifdef __cplusplus
}
#endif

#endif /* __OUTPUT_H      Do not add any thing below this line */
