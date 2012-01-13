#if !defined(__SOUND_H)
	#define __SOUND_H

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
*   @brief Function used to set the current volume level
*
*   @param Volume level (0-34)
*
*   @retval void
*/
extern void sound_setVolume(long value);

/**
*   @brief Function used to set up the sound
*
*   @retval void
*/
extern int sound_init(void);

/**
*   @brief Function used to reinitialize sound system
*
*   @retval void
*/
extern int sound_restart(void);

#endif /* __SOUND_H      Do not add any thing below this line */
