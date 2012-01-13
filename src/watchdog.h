#if !defined(__WATCHDOG_H)
#define __WATCHDOG_H

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

/*******************
* EXPORTED MACROS  *
********************/

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/*******************
* EXPORTED DATA    *
********************/

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

/**
*   @brief Function used to initialise and start the watchdog
*
*   @retval void
*/
extern void watchdog_init(void);

/**
*   @brief Function used to de-initialise and stop the watchdog
*
*   @retval void
*/
extern void watchdog_deinit(void);

#endif /* __WATCHDOG_H      Do not add any thing below this line */
