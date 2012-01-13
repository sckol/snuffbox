#if !defined(__RTSP_H)
#define __RTSP_H

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

typedef struct stream_files
{
	unsigned int index;
	char *stream;
	char *name;
	char *description;
	char *poster;
	char *thumb;
	unsigned int pidv;
	unsigned int pida;
	unsigned int pidp;
	unsigned int vformat;
	unsigned int aformat;
	struct stream_files *next;
} streams_struct;

typedef struct
{
	char streamname[MENU_ENTRY_INFO_LENGTH];
	int pida;           //Audio Pid
	int pidv;           //Video Pid
	int pidp;           //PCR pid
	int vformat;        //the stream type assignment for the selected video pid
	int aformat;        //the stream type assignment for the selected audio pid
	int device;         //which /dev/dvb/adapter to use (2 or 3)
	unsigned int port;  //what RTSP port to use
	char ip[32];           //which ip address to connect to

	int custom_url;		// it this is not 0, then this stream is treated as custom url
} rtsp_stream_info;

// dgk
struct rtsp_control_t {
	double start;
	double end;
	float scale;
	char stopFlag;
	char exitFlag;
	char restartFlag;
	int enabled;

	double currentScale;
	double currentPos;
	double lastPos;

	int startFromPos;

	struct timeval countstart;
};
// end dgk

/*******************
* EXPORTED DATA    *
********************/

extern interfaceListMenu_t rtspMenu;
//extern sliderContainer rtspSlider;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

/**
*   @brief Function used to build the RTSP menu data structures
*
*   @retval void
*/
void rtsp_buildMenu(interfaceMenu_t *pParent);

int rtsp_fillStreamMenu(interfaceMenu_t *pMenu, void* pArg);

void rtsp_cleanupMenu();

/**
*   @brief Function used to stop RTSP input display
*
*   @param which    I       Screen to stop video on

*   @retval void
*/
void rtsp_stopVideo(int which);

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
int rtsp_playURL(int which, char *URL, char* description, char* thumbnail);

#endif /* __RTSP_H      Do not add any thing below this line */
