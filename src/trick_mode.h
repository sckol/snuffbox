#if !defined(__TRICK_MODE_H)
#define __TRICK_MODE_H

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
*   @brief Function used to stop trick mode playback
*
*   @retval void
*/
extern void trickMode_stop(void);

/**
*   @brief Function used to start trick mode playback
*
*   @param directory    I    Path to directory where the PVR files are held
*   @param pFileIn      I/O  Pointer to current input file
*   @param pWhichIndex  I/O  Pointer to PVR file index
*   @param pOffset      I/O  Pointer to offset within the current PVR file
*   @param fileOut      I    Output file (demux)
*   @param direction    I    Play direction
*   @param speed        I    Play speed
*   @param videoPid     I    The PID for the video stream
*
*   @retval void
*/
extern int trickMode_play(char* directory, int *pFileIn, int *pWhichIndex, int *pOffset, int fileOut, stb810_trickModeDirection direction, stb810_trickModeSpeed speed, int videoPid);

/**
*   @brief Function used to change the speed of a trick mode
*
*   @param speed        I    Play speed
*
*   @retval void
*/
extern void trickMode_setSpeed(stb810_trickModeSpeed speed);

#endif /* __TRICK_MODE_H      Do not add any thing below this line */
