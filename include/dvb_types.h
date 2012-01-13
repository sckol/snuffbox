#if !defined(__DVB_TYPES_H)
	#define __DVB_TYPES_H

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

/** @defgroup dvb_types Common DVB data structure definitions
 *  @ingroup  StbMainApp
 */

/*******************
* INCLUDE FILES    *
********************/

#include "defines.h"

/*******************
* EXPORTED MACROS  *
********************/

#define TS_PACKET_SIZE (188)

#define FILESIZE_THRESHOLD (1024*1024*1024)

#define BER_THRESHOLD (30000)

#define H264             (0x1b)
#define MPEG2            (0x01)
#define MP3              (0x03)
#define AAC              (0x0f)
#define AC3              (0x6a)
#define TXT              (0x06)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/** @ingroup dvb_types
 * Possible input tuners
 */
typedef enum
{
	inputTuner0 = 0,
	inputTuner1,
	inputTuners
} tunerFormat;

/** @ingroup dvb_types
 * The modes in which DVB can operate
 */
typedef enum
{
	DvbMode_Watch,  /**< Watching the output of a tuner */
	DvbMode_Record, /**< Recording the output of a tuner */
	DvbMode_Play    /**< Playing back a recorded file */
#ifdef ENABLE_MULTI_VIEW
	, DvbMode_Multi /**< Watching four channels simultaneously */
#endif
} DvbMode_t;

#endif /* __DVB_TYPES_H      Do not add any thing below this line */
