#if !defined(__RTP_H)
	#define __RTP_H

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
#include "off_air.h"

/*******************
* EXPORTED MACROS  *
********************/

#define RTP_EPG_ERROR_TIMEOUT    (3000)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

/*typedef struct
{
	char streamname[PATH_MAX];
	int pida;			//Audio Pid
	int pidv;			//Video Pid
	int pidp;			//PCR pid
	int vformat;		//the stream type assignment for the selected video pid
	int aformat;		//the stream type assignment for the selected audio pid
	int device;			//which /dev/dvb/adapter to use (2 or 3)
	unsigned int port;	//what RTP port to use
	char ip[32];			//which ip address to connect to

} rtp_stream_info;*/

/*******************
* EXPORTED DATA    *
********************/

extern interfaceListMenu_t rtpStreamMenu[screenOutputs];
extern interfaceEpgMenu_t  rtpRecordMenu;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

/** Function used to build the RTP menu data structures
*   @param[in]  pParent Parent menu of rtpMenu
*/
void rtp_buildMenu(interfaceMenu_t *pParent);

/** Function used to init contents of RTP menu
*
*   Called when user enters IPTV stream menu. 
*   Should fill menu with streams, and if it's impossible to do immediately,
*   run thread in background that collects streams from SAP or http playlist.
*   @param[in] pMenu Pointer to rtpMenu
*   @param[in] pArg  Screen (main or pip) to build menu on
*
*   @return 0 on success
*/
int rtp_initStreamMenu(interfaceMenu_t *pMenu, void* pArg);

/** Function used to init contents of RTP EPG menu
*   Calls menuActionShowMenu on success to switch to rtpEpgMenu
*
*   @param[in]  pMenu Pointer to calling menu
*   @param[in]  pArg  Stream number in current playlist
*
*   @return 0 on success
*/
int rtp_initEpgMenu(interfaceMenu_t *pMenu, void* pArg);

void rtp_cleanupMenu();

/** Function used to stop RTP input display
*
*   @param[in] which Screen to stop video on
*/
void rtp_stopVideo(int which);

int rtp_playURL(int which, char *value, char *description, char *thumbnail);

int rtp_startNextChannel(int direction, void* pArg);

int rtp_setChannel(int channel, void* pArg);

void rtp_getPlaylist(int which);

int rtp_showEPG(int which, playControlSetupFunction pSetup);

/** Returns internal channel name of specified URL
 *  @param[in] url Stream url
 *  @return CHANNEL_CUSTOM if stream not found in list
 */
int rtp_getChannelNumber(const char *url);

/** Cleans up previously acquired IPTV EPG
 *  Should be called after changing EPG url
 */
void rtp_cleanupEPG();

/** Cleans IPTV channel list
 *  Should be called after changing IPTV playlist url
 *  @param[in] which Should always be screenMain
 */
void rtp_cleanupPlaylist(int which);

#ifdef ENABLE_PVR
/** Record current or last played RTP stream
 *  @return pvr_record return value
 */
int rtp_recordNow();
#endif

#endif /* __RTP_H      Do not add any thing below this line */
