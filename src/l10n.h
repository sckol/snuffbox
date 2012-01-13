#if !defined(__L10N_H)
#define __L10N_H

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

/****************
* INCLUDE FILES *
*****************/

#include "interface.h"

/*******************
* EXPORTED MACROS  *
********************/

#define MAX_LANG_NAME_LENGTH (128)
#define _T(String)           l10n_getText( (String) )

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern const char          l10n_languageDir[];
extern char                l10n_currentLanguage[MAX_LANG_NAME_LENGTH];
extern interfaceListMenu_t LanguageMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

char *l10n_getText(const char* key);
int   l10n_init(const char* languageName);
void  l10n_cleanup();
int   l10n_switchLanguage(const char* newLanguage);
int   l10n_initLanguageMenu(interfaceMenu_t *pMenu, void* pArg);

#ifdef __cplusplus
}
#endif

#endif /* __L10N_H      Do not add any thing below this line */
