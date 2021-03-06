
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

/***********************************************
* INCLUDE FILES                                *
************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <fcntl.h>

/// sockets ///
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//////////////

#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <sys/ioctl.h>

#include <pthread.h>
#include <signal.h>

#include <directfb.h>
#include "app_info.h"
#include "debug.h"
#include "sem.h"
#include "gfx.h"
#include "interface.h"
#include "StbMainApp.h"
#include "sdp.h"
#include "rtp_func.h"
#include "rtp.h"
#include "rtsp.h"
#include "media.h"
#include "dlna.h"
#ifdef STB225
#include "phStbSbmSink.h"
#endif
#include "list.h"
#include "rtp_common.h"
#include "l10n.h"
#include "playlist.h"
#include "output.h"
#include "tools.h"

#ifdef ENABLE_VIDIMAX
#include "vidimax.h"
#endif

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define STREAM_INFO_SET(screen, channel) ((screen << 16) | (channel))
#define STREAM_INFO_GET_SCREEN(info)     (info >> 16)
#define STREAM_INFO_GET_STREAM(info)     (info & 0xFFFF)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static void rtsp_setupPlayControl(void *pArg);
static int rtsp_stream_change(interfaceMenu_t *pMenu, void* pArg);
static int rtsp_stream_start(void* pArg);
static int rtsp_setChannel(int channel, void* pArg);
static int rtsp_play_callback(interfacePlayControlButton_t button, void *pArg);
static int rtsp_setChannelFromURL(interfaceMenu_t *pMenu, char *value, char* description, char* thumbnail, void* pArg);

static int rtsp_checkStream(void *pArg);

static void rtsp_setStopTimer(int which, int length);
static void rtsp_setStateCheckTimer(int which, int bEnable, int bRunNow);

int rtsp_startNextChannel(int direction, void* pArg);
int rtsp_getPosition(double *plength,double *pposition);

static streams_struct *add_stream(streams_struct **ppstream_head, char *stream, int index);
static int rtsp_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

static streams_struct *get_stream(unsigned int index);
static void *rtsp_list_updater(void *pArg);
static int get_rtsp_streams(streams_struct **ppstream_head);
static int rtsp_displayStreamMenu(void* pArg);
static int rtsp_menuEntryDisplay(interfaceMenu_t* pMenu, DFBRectangle *rect, int i);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static int collectFlag;
static pthread_t collectThread;
static rtsp_stream_info stream_info;

//static int selected;
static streams_struct *pstream_head=NULL;
static streams_struct **ppstream_head=&pstream_head;

#ifndef STBxx
static int pipe_id;
#endif

//static struct rtp_list *streams = NULL;
static interfaceListMenu_t rtspStreamMenu;

/* start/stop semaphore */
static pmysem_t  rtsp_semaphore;

static char rtsp_lasturl[MAX_URL] = "rtsp://";

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceListMenu_t rtspMenu;
//sliderContainer rtspSlider;

struct rtsp_control_t rtspControl;

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

void clean_list(streams_struct *stream_ptr)
{
	//recursively clears linked list

	//dprintf("%s: clean list\n", __FUNCTION__);

	if ( stream_ptr->next!=NULL )
	{
		clean_list((streams_struct *) stream_ptr->next);
	}

	dfree(stream_ptr->stream);
	FREE(stream_ptr->name);
	FREE(stream_ptr->description);
	FREE(stream_ptr->thumb);
	FREE(stream_ptr->poster);
	dfree(stream_ptr);
}

static void rtsp_urlHandler(void *pArg, const char *location, const char *desc, xmlConfigHandle_t track)
{
	int            *pIndex = (int*)pArg;
	streams_struct *pstream;
	char           *ptr;

	if( location[0] )
	{
		pstream = add_stream(ppstream_head, (char *)location, *pIndex);

		helperSafeStrCpy(&pstream->name, desc);

		if( track && pstream )
		{
			ptr = (char*)xmlConfigGetText(track, "descr");
			if( ptr && *ptr )
			{
				//dprintf("%s: Adding '%s' description to %03d stream\n", __FUNCTION__, ptr, *pIndex);
				helperSafeStrCpy( &pstream->description, ptr );
			}

			ptr = (char*)xmlConfigGetText(track, "thumb");
			if( ptr && *ptr )
			{
				//dprintf("%s: Adding '%s' thumb to %03d stream\n", __FUNCTION__, ptr, *pIndex);
				helperSafeStrCpy( &pstream->thumb, ptr );
			}

			ptr = (char*)xmlConfigGetText(track, "poster");
			if( ptr && *ptr )
			{
				//dprintf("%s: Adding '%s' poster to %03d stream\n", __FUNCTION__, ptr, *pIndex);
				helperSafeStrCpy( &pstream->poster, ptr );
			}
		}

		(*pIndex)++;
	}
}

int find_streams(streams_struct **ppstream_head)
{
	//http get streams.txt
	char info_url[MAX_URL];
	int  i;
	int  index = 0;

	dprintf("%s: in\n", __FUNCTION__);

	mysem_get(rtsp_semaphore);

	if( appControlInfo.rtspInfo[screenMain].usePlaylistURL && appControlInfo.rtspInfo[screenMain].streamInfoUrl[0] != 0 )
	{
		eprintf("%s: trying URL '%s'\n", __FUNCTION__, appControlInfo.rtspInfo[screenMain].streamInfoUrl);
		playlist_getFromURL( appControlInfo.rtspInfo[screenMain].streamInfoUrl, rtsp_urlHandler, (void*)&index );
	} else
	{
		i = 0;
		while (appControlInfo.rtspInfo[screenMain].streamInfoFiles[i] != NULL)
		{
			sprintf(info_url, "http://%s/%s", appControlInfo.rtspInfo[screenMain].streamInfoIP, appControlInfo.rtspInfo[screenMain].streamInfoFiles[i]);
			eprintf("%s: trying IP '%s'\n", __FUNCTION__, info_url);
			if( playlist_getFromURL( info_url, rtsp_urlHandler, (void*)&index ) == PLAYLIST_ERR_OK )
				break;
			i++;
		}
	}

	mysem_release(rtsp_semaphore);

	return index == 0 ? -1 : 0;
}

static int rtsp_timerEvent(void *pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);

	dprintf("%s: Stop video (will wait for end of stream)\n", __FUNCTION__);

	//rtsp_stopVideo(which);
	rtsp_setStopTimer(which, 0);

	return 0;
}

static void rtsp_setStopTimer(int which, int length)
{
	dprintf("%s: set timer for %d seconds\n", __FUNCTION__, length);

	if (length > 0)
	{
		interface_removeEvent(rtsp_checkStream, (void*)STREAM_INFO_SET(which, 0));
		interface_addEvent(rtsp_timerEvent, (void*)STREAM_INFO_SET(which, 0), length*1000, 1);
	} else
	{
		interface_removeEvent(rtsp_timerEvent, (void*)STREAM_INFO_SET(which, 0));
		if (length == 0)
		{
			interface_addEvent(rtsp_checkStream, (void*)STREAM_INFO_SET(which, 0), 5000, 1);
		} else
		{
			interface_removeEvent(rtsp_checkStream, (void*)STREAM_INFO_SET(which, 0));
		}
	}
}


static int rtsp_stateTimerEvent(void *pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	double length_stream = 0.0;
	double position_stream = 0.0;
	int		ret_val;
	DFBVideoProviderStatus status;

	
	status = gfx_getVideoProviderStatus(which);
	switch( status )
	{
		case DVSTATE_FINISHED:
		case DVSTATE_STOP:
			eprintf("%s: status = DVSTATE_STOP || DVSTATE_FINISHED\n", __FUNCTION__);
			
			if( status == DVSTATE_FINISHED )
				interface_showMessageBox(_T("ERR_STREAM_NOT_SUPPORTED"), thumbnail_error, 0);
			interface_playControlSlider(0,0,0);
#ifdef ENABLE_VIDIMAX
			if (appControlInfo.vidimaxInfo.active){
				vidimax_stopVideoCallback();
			}
#endif
			rtsp_stopVideo(screenMain);
			break;
		case DVSTATE_STOP_REWIND:
			appControlInfo.playbackInfo.scale = 1.0;
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			interface_playControlSelect(interfacePlayControlPlay);
			interface_notifyText(NULL, 1);
		default:
			if (!gfx_videoProviderIsPaused(screenMain))
			{
				ret_val	=	rtsp_getPosition(&length_stream,&position_stream);
				//eprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);
				if((ret_val == 0)&&(position_stream <= length_stream))
				{
					interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position_stream);
					rtsp_setStateCheckTimer(which, 1, 0);
				}
				else if( ret_val == 0 )
				{
					eprintf("%s: ret_val == 0\n", __FUNCTION__);
					rtsp_stopVideo(screenMain);
				}
			} else
			{
				rtsp_setStateCheckTimer(which, 1, 0);
			}
	}

	return 0;
}

static void rtsp_setStateCheckTimer(int which, int bEnable, int bRunNow)
{
	if (bEnable)
	{
		if (bRunNow)
		{
			eprintf("%s: bRunNow = 1, Update slider\n", __FUNCTION__);
			rtsp_stateTimerEvent((void*)STREAM_INFO_SET(which, 0));
		}
		interface_addEvent(rtsp_stateTimerEvent, (void*)STREAM_INFO_SET(which, 0), 1000, 1);
	} else
	{
		// At the end make focused play controls but not slider 
		interface_playControlSlider(0, 0, 0);
		interface_playControlSetInputFocus(inputFocusPlayControl);
		interface_playControlSelect(interfacePlayControlStop);

		interface_removeEvent(rtsp_stateTimerEvent, (void*)STREAM_INFO_SET(which, 0));
		interface_notifyText(NULL, 1);
	}
}

void rtsp_stopVideo(int which)
{
	dprintf("%s: in\n", __FUNCTION__);

	mysem_get(rtsp_semaphore);
	dprintf("%s: got sem\n", __FUNCTION__);

	if ( appControlInfo.rtspInfo[which].active != 0 )
	{
		rtsp_setStopTimer(which, -1);
		rtsp_setStateCheckTimer(which, 0, 0);

		interface_playControlSlider(0, 0, 0);

		interface_playControlSelect(interfacePlayControlStop);

		dprintf("%s: Stop video screen %s\n", __FUNCTION__, which ? "Pip" : "Main");
		eprintf("%s: Stop video screen %s\n", __FUNCTION__, which ? "Pip" : "Main");
		gfx_stopVideoProvider(which, 1, 1);
		appControlInfo.rtspInfo[which].active = 0;

		rtspControl.enabled = 0;
		rtspControl.startFromPos = 0;
		rtspControl.currentPos = 0;

		interface_disableBackground();
		
		//interface_playControlDisable(1);

		//rtsp_fillMenu();
	}

	mysem_release(rtsp_semaphore);
}

static streams_struct *add_stream(streams_struct **ppstream_head, char *stream, int index)
{
	streams_struct* stream_ptr=NULL;
	streams_struct* stream_ptr_prev=NULL;
	size_t stream_size = strlen(stream)+1;

	if ( (*ppstream_head) == NULL )
	{
		//if still no ppstream_head structure create one
		(*ppstream_head) = dmalloc(sizeof(streams_struct));
		stream_ptr_prev = NULL;
		stream_ptr = (*ppstream_head);
	} else
	{
		stream_ptr = (*ppstream_head);
		while ( stream_ptr->next != NULL )
		{
			stream_ptr = stream_ptr->next;
		}

		//otherwise point previous struct to next struct and create that
		stream_ptr->next = dmalloc(sizeof(streams_struct));
		stream_ptr_prev = stream_ptr;
		stream_ptr = (streams_struct *) stream_ptr_prev->next;
	}

	stream_ptr->index       = index;
	stream_ptr->name        = NULL;
	stream_ptr->description = NULL;
	stream_ptr->thumb       = NULL;
	stream_ptr->poster      = NULL;
	stream_ptr->next        = NULL;
	stream_ptr->stream      = dmalloc(stream_size);
	memcpy(stream_ptr->stream, stream, stream_size);
	
	return stream_ptr;
}


// FIXME: some data at the end of a stream is discarded because it is still being processed in demuxer buffer
// when network streaming is finished
static int rtsp_checkStream(void *pArg)
{
	dprintf("%s: in\n");

	interface_addEvent(rtsp_checkStream, pArg, 5000, 1);

	return 0;
}

static void *rtsp_list_updater(void *pArg)
{
	int i;
	int which;
	int sleepTime = 1;

	which = (int) pArg;

	while (collectFlag)
	{
		interface_showLoadingAnimation();

		dprintf("%s: rtsp_list_updater: collecting/waiting\n", __FUNCTION__);

		i = 0;
		while (i++ < sleepTime*10)
		{
			if (collectFlag)
			{
				usleep(100000);
			} else
			{
				dprintf("%s: stop waiting\n", __FUNCTION__);
				interface_hideLoadingAnimation();
				break;
			}
		}

		if (collectFlag)
		{
			get_rtsp_streams(ppstream_head);

			if(ppstream_head == NULL)
			{
				collectFlag--;
			} else
			{
				collectFlag = 0;
			}

			interface_hideLoadingAnimation();

			rtsp_displayStreamMenu((void*)which);

			sleepTime = 2;
		}
	}

	collectThread = 0;

	dprintf("%s: exit normal\n", __FUNCTION__);

	return NULL;
}

static int get_rtsp_streams(streams_struct **ppstream_head)
{
	int ret;
	int i = 0;
	char buf[PATH_MAX];

	dprintf("%s: in\n", __FUNCTION__);

	if ( pstream_head != NULL && *ppstream_head != NULL )
	{
		clean_list(*ppstream_head);
		*ppstream_head = NULL;
	}

	dprintf("%s: call find_streams\n", __FUNCTION__);

	ret = find_streams(ppstream_head);

	if ( !((ret == 0)&&(pstream_head != NULL)) )
	{
		int file;

		dprintf("%s: open %s\n", __FUNCTION__, appControlInfo.rtspInfo[screenMain].streamFile);

		file = open(appControlInfo.rtspInfo[screenMain].streamFile, O_RDONLY);

		if ( file > 0 )
		{
			while ( helperReadLine(file, buf) == 0 )
			{
				if ( strlen(buf) > 0 )
				{
					add_stream(ppstream_head, buf, i);
				}
				i++;
			}
		}
		ret = file;
	}

	return ret;
}

int rtsp_startVideo(int which)
{
	int ret;
	char qualifier[64];
	char pipeString[256];
	char *str;
	double length_stream = 0.0;
	double position_stream = 0.0;

	interface_showLoadingAnimation();

	mysem_get(rtsp_semaphore);

	media_slideshowStop(1);

	gfx_stopVideoProviders(which);

	qualifier[0] = 0;
	sprintf(pipeString,"rtsp://%s:%d/%s",
		stream_info.ip,
		stream_info.port,
		stream_info.streamname);
	// xWorks links workaround
	if( (str = strstr(pipeString, "?manualDetectVideoInfo")) != NULL )
		*str = 0;
	dprintf("%s: url=%s\n", __FUNCTION__,pipeString);
	ret = gfx_startVideoProvider(pipeString, which, 1, qualifier);

	if (ret != 0)
	{
		mysem_release(rtsp_semaphore);
		interface_showMessageBox(_T("ERR_STREAM_UNAVAILABLE"), thumbnail_error, 0);
		eprintf("RTSP: Failed to start video provider '%s'\n", pipeString);
		return -1;
	}

#ifdef ENABLE_VERIMATRIX
	if (appControlInfo.useVerimatrix != 0)
	{
		eprintf("RTSP: Enable verimatrix...\n");
		if (gfx_enableVideoProviderVerimatrix(screenMain, VERIMATRIX_INI_FILE) != 0)
		{
			interface_showMessageBox(_T("ERR_VERIMATRIX_INIT"), thumbnail_error, 0);
		}
	}
#endif
#ifdef ENABLE_SECUREMEDIA
	if (appControlInfo.useSecureMedia != 0)
	{
		eprintf("RTSP: Enable securemedia...\n");
		if (gfx_enableVideoProviderSecureMedia(screenMain) != 0)
		{
			interface_showMessageBox(_T("ERR_SECUREMEDIA_INIT"), thumbnail_error, 0);
		}
	}
#endif

	rtsp_setStateCheckTimer(which, 1, 1);

	if(!gfx_getVideoProviderHasVideo(screenMain))
	{
		if(appControlInfo.slideshowInfo.state == slideshowDisabled)
		{
			interface_setBackground(0,0,0,0xFF, INTERFACE_WALLPAPER_IMAGE);
		}
	}
	else
	{
		media_slideshowStop(1);
	}
	if (gfx_isTrickModeSupported(screenMain))
	{
		interface_playControlSetButtons( interface_playControlGetButtons()|interfacePlayControlFastForward|interfacePlayControlRewind );
	}

	appControlInfo.rtspInfo[which].active = 1;

	interface_playControlSelect(interfacePlayControlStop);
	interface_playControlSelect(interfacePlayControlPlay);

	rtspControl.enabled = 1;

	ret = rtsp_getPosition(&length_stream,&position_stream);
	if(ret == 0)
	{
		//dprintf("%s: start from pos %f\n", __FUNCTION__, position_stream);
		if(rtspControl.startFromPos == 0)
			interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position_stream);
		rtsp_setStateCheckTimer(screenMain, 1, 0);
	}else
	{
		interface_playControlSlider(0, 0, 0);
	}

	interface_displayMenu(1);
	//rtsp_fillMenu();

	dprintf("%s: released sem\n", __FUNCTION__);
	mysem_release(rtsp_semaphore);

	interface_hideLoadingAnimation();
	return 0;
}

int rtsp_startNextChannel(int direction, void* pArg)
{
	int which;
	streams_struct* stream_ptr=NULL;
	streams_struct* prev_ptr=NULL;
	streams_struct* new_ptr=NULL;
	char url[MAX_URL];

	streams_struct* first=NULL;

	which = STREAM_INFO_GET_SCREEN((int)pArg);

	switch( appControlInfo.playbackInfo.playlistMode )
	{
		case playlistModeFavorites:
			sprintf(url,"rtsp://%s:%d/%s",
				stream_info.ip,
				stream_info.port,
				stream_info.streamname);
			playlist_setLastUrl(url);
			return playlist_startNextChannel(direction,(void*)-1);
#ifdef ENABLE_DLNA
		case playlistModeDLNA:
			sprintf(url,"rtsp://%s:%d/%s",
				stream_info.ip,
				stream_info.port,
				stream_info.streamname);
			return dlna_startNextChannel(direction,url);
#endif
		case playlistModeIPTV:
			return rtp_startNextChannel(direction,(void*)-1);
		default:;
	}

	if ( pstream_head != NULL )
	{
		prev_ptr = NULL;
		stream_ptr = pstream_head;

		while ( stream_ptr != NULL )
		{
			if (first == NULL)
			{
				first = stream_ptr;
			}
			if ( strcmp(stream_info.ip, appControlInfo.rtspInfo[screenMain].streamIP) == 0 && strcmp(stream_info.streamname, stream_ptr->stream) == 0 )
			{
				if (direction == 0)
				{
					new_ptr = stream_ptr->next;
				} else
				{
					new_ptr = prev_ptr;
				}
				if (new_ptr != NULL) // don't break if it's first/last stream in list
				{
					break;
				}
			}
			prev_ptr = stream_ptr;
			stream_ptr = (streams_struct *) stream_ptr->next;
		}
	}

	//dprintf("%s: new %08X, first %08X, last %08X\n", __FUNCTION__, new_ptr, first, prev_ptr);

	if (new_ptr == NULL && first != prev_ptr) // we have more than one stream in list
	{
		if (direction == 0)
		{
			new_ptr = first;
		} else
		{
			new_ptr = prev_ptr;
		}
	}

	if (new_ptr != NULL)
	{
		//dprintf("%s: change to stream %s, index %d\n", __FUNCTION__, new_ptr->name, new_ptr->index);
		rtsp_stream_change(interfaceInfo.currentMenu, (void*)STREAM_INFO_SET(which, new_ptr->index));
	}

	return 0;
}


static int rtsp_displayStreamMenu(void* pArg)
{
	//int position = 0;
	char channelEntry[MENU_ENTRY_INFO_LENGTH];
	int streamNumber = 0;
	int which;
	streams_struct* stream_ptr=NULL;

	which = (int) pArg;

	interface_clearMenuEntries((interfaceMenu_t*)&rtspStreamMenu);

	interface_showLoadingAnimation();

	//find_streams(ppstream_head);
	//get_rtsp_streams(ppstream_head);
	if ( pstream_head != NULL )
	{
		stream_ptr = pstream_head;

		while ( stream_ptr != NULL )
		{
			sprintf(channelEntry, "%d: %s", streamNumber+1, stream_ptr->name ? stream_ptr->name : stream_ptr->stream);
			
			interface_addMenuEntryCustom((interfaceMenu_t*)&rtspStreamMenu, interfaceMenuEntryText, channelEntry, strlen(channelEntry)+1, 1, rtsp_stream_change, NULL, NULL, rtsp_menuEntryDisplay, (void*)STREAM_INFO_SET(which, streamNumber), thumbnail_vod);
			//menuInfra_setEntry(&rtspStreamMenu, position++, menuEntryText, channelEntry, rtsp_stream_change, (void*)STREAM_INFO_SET(which, stream_ptr->index));
			//dprintf("%s: Compare current %s\n", __FUNCTION__, channelEntry);
			if ( strcmp(stream_info.ip, appControlInfo.rtspInfo[screenMain].streamIP) == 0 && strcmp(stream_info.streamname, stream_ptr->stream) == 0 )
			{
				interface_setSelectedItem((interfaceMenu_t*)&rtspStreamMenu, streamNumber);
			}
			streamNumber++;
			stream_ptr = (streams_struct *) stream_ptr->next;
		}

	}

	if ( streamNumber == 0 )
	{
		char *str;
		str = _T("NO_MOVIES");
		interface_addMenuEntryDisabled((interfaceMenu_t*)&rtspStreamMenu, str, thumbnail_info);
	}
/*
	menuInfra_setSelected(&rtspStreamMenu, selected);
	menuInfra_display(&rtspStreamMenu);
*/

	interface_hideLoadingAnimation();

	interface_displayMenu(1);

	return 0;
}

int rtsp_getPosition(double *plength,double *pposition)
{
	double length;
	double position;

	length = gfx_getVideoProviderLength(screenMain);
	dprintf("%s: -- length = %f \n", __FUNCTION__, length);
	if (length<2)
		return -1;

	position = gfx_getVideoProviderPosition(screenMain);
	dprintf("%s: -- position = %f \n", __FUNCTION__, position);
	if (position>length)
		return length;

	*plength				=	length;
	*pposition				=	position;
	rtspControl.end		=	length;
	rtspControl.start	=	position;

	return 0;
}

static int rtsp_play_callback(interfacePlayControlButton_t button, void *pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	//int streamNumber = STREAM_INFO_GET_STREAM((int)pArg);
	int res = 0;
	char url[MAX_URL];
	int ret_val;
	double position = 0.0, length_stream = 0.0;

	dprintf("%s: in\n", __FUNCTION__);
	eprintf("%s: in\n", __FUNCTION__);

	if ( button == interfacePlayControlPrevious )
	{
		rtsp_startNextChannel(1, pArg);
	} else if ( button == interfacePlayControlNext )
	{
		rtsp_startNextChannel(0, pArg);
	} else if ( button == interfacePlayControlSetPosition )
	{
		if ( !appControlInfo.rtspInfo[which].active )
		{
			position					=	interface_playControlSliderGetPosition();
			rtspControl.startFromPos	=	1;
			appControlInfo.playbackInfo.scale = 1.0;
			rtsp_startVideo(which);
			gfx_setVideoProviderPosition(screenMain,position);
			ret_val	=	rtsp_getPosition(&length_stream,&position);
			//dprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);
			if((ret_val == 0)&&(position <= length_stream))
			{
				interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position);
			}
			rtspControl.startFromPos	=	0;
		} else
		{
			position	=	interface_playControlSliderGetPosition();
			if (gfx_videoProviderIsPaused(screenMain))
			{
				gfx_resumeVideoProvider(screenMain);
			}
			gfx_setVideoProviderPosition(screenMain,position);
			//appControlInfo.playbackInfo.scale = 1.0
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			if( appControlInfo.playbackInfo.scale == 1.0 )
			{
				interface_notifyText(NULL, 0);
				interface_playControlSelect(interfacePlayControlPlay);
			} else
			{
				sprintf(url, "%1.0fx", appControlInfo.playbackInfo.scale);
				interface_notifyText(url, 0);
				interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
			}
		}
		return 0;
	} else if ( button == interfacePlayControlPlay )
	{
		if ( !appControlInfo.rtspInfo[which].active )
		{
			appControlInfo.playbackInfo.scale = 1.0;
			rtsp_startVideo(which);
		} else
		{
			appControlInfo.playbackInfo.scale = 1.0;
			if (gfx_videoProviderIsPaused(screenMain))
			{
				gfx_resumeVideoProvider(screenMain);
			}
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			interface_notifyText(NULL, 0);
			interface_playControlSelect(interfacePlayControlPlay);
		}
	} else if ( button == interfacePlayControlStop )
	{
		if ( appControlInfo.rtspInfo[which].active )
		{
			void *show_menu = NULL;
			switch (appControlInfo.playbackInfo.playlistMode)
			{
				case playlistModeIPTV: show_menu = (void*)&rtpStreamMenu[screenMain]; break;
				case playlistModeFavorites: show_menu = (void*)&playlistMenu; break;
#ifdef ENABLE_DLNA
				case playlistModeDLNA: show_menu = (void*)&BrowseServersMenu; break;
#endif
				default: show_menu = (void*)&rtspStreamMenu;
			}
			rtsp_stopVideo(which);
			eprintf("%s: () interface_menuActionShowMenu, interfaceInfo.currentMenu = %p\n", 
				__FUNCTION__, interfaceInfo.currentMenu);
				
#ifdef ENABLE_VIDIMAX
			if (interfaceInfo.currentMenu == (interfaceMenu_t*)&VidimaxMenu){
				show_menu = (void*)&VidimaxMenu;
			}
#endif
			interface_menuActionShowMenu(interfaceInfo.currentMenu, show_menu);
			interface_showMenu(1, 1);
		}
	} else if ( button == interfacePlayControlPause )
	{
		if ( appControlInfo.rtspInfo[which].active )
		{
			if (gfx_videoProviderIsPaused(screenMain))
			{
				//appControlInfo.playbackInfo.scale = 1.0;
				gfx_resumeVideoProvider(screenMain);
				gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
				interface_playControlSelect(interfacePlayControlPlay);
			} else
			{
				eprintf("%s: gfx_stopVideoProvider...\n", __FUNCTION__);
				gfx_stopVideoProvider(screenMain, 0, 0);
				interface_playControlSelect(interfacePlayControlPause);
			}
		}
	} else if ( button == interfacePlayControlFastForward )
	{
		if ( !appControlInfo.rtspInfo[which].active )
		{
			rtsp_startVideo(which);
			//appControlInfo.playbackInfo.scale = 2.0;
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
		} else
		{
			float newScale;

			if (gfx_videoProviderIsPaused(screenMain))
				gfx_resumeVideoProvider(screenMain);

			if( appControlInfo.playbackInfo.scale >= MAX_SCALE )
				newScale = 0.0;
			else if( appControlInfo.playbackInfo.scale > 0.0 )
				newScale = appControlInfo.playbackInfo.scale * 2;
			else if( appControlInfo.playbackInfo.scale < -2.0 )
				newScale = appControlInfo.playbackInfo.scale / 2;
			else
				newScale = 1.0;

			if( newScale != 0.0 && gfx_setSpeed(screenMain, newScale) == 0 )
				appControlInfo.playbackInfo.scale = newScale;
		}
		if( appControlInfo.playbackInfo.scale == 1.0 )
		{
			interface_notifyText(NULL, 0);
			interface_playControlSelect(interfacePlayControlPlay);
		} else
		{
			sprintf(url, "%1.0fx", appControlInfo.playbackInfo.scale);
			interface_notifyText(url, 0);
			interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
		}
	} else if ( button == interfacePlayControlRewind )
	{
		if ( !appControlInfo.rtspInfo[which].active )
		{
			rtsp_startVideo(which);
			//appControlInfo.playbackInfo.scale = 2.0;
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
		} else
		{
			float newScale;

			if (gfx_videoProviderIsPaused(screenMain))
				gfx_resumeVideoProvider(screenMain);

			if( appControlInfo.playbackInfo.scale >= 2.0 )
				newScale = appControlInfo.playbackInfo.scale / 2;
			else if( appControlInfo.playbackInfo.scale > 0.0 )
				newScale = -2.0;
			else if( appControlInfo.playbackInfo.scale > -MAX_SCALE )
				newScale = appControlInfo.playbackInfo.scale * 2;
			else
				newScale = 0.0;

			if( newScale != 0.0 && gfx_setSpeed(screenMain, newScale) == 0 )
				appControlInfo.playbackInfo.scale = newScale;
		}
		if( appControlInfo.playbackInfo.scale == 1.0 )
		{
			interface_notifyText(NULL, 0);
			interface_playControlSelect(interfacePlayControlPlay);
		} else
		{
			sprintf(url, "%1.0fx", appControlInfo.playbackInfo.scale);
			interface_notifyText(url, 0);
			interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
		}
	} else if (button == interfacePlayControlInfo)
	{
		interface_playControlSliderEnable(!interface_playControlSliderIsEnabled());
		interface_displayMenu(1);
		return 0;
	} else if (button == interfacePlayControlAddToPlaylist)
	{
		sprintf(url,"rtsp://%s:%d/%s",
			stream_info.ip,
			stream_info.port,
			stream_info.streamname);
		playlist_addUrl(url, stream_info.streamname);
	} else if(button == interfacePlayControlMode && appControlInfo.playbackInfo.playlistMode == playlistModeIPTV && appControlInfo.rtpMenuInfo.epg[0] != 0 )
	{
		rtp_showEPG(which, rtsp_setupPlayControl);
	} else
	{
		// default action
		return 1;
	}

	if ( res != 0 && appControlInfo.rtspInfo[which].active )
	{
		void *show_menu;
		switch (appControlInfo.playbackInfo.playlistMode)
		{
			case playlistModeFavorites: show_menu = (void*)&playlistMenu; break;
#ifdef ENABLE_DLNA
			case playlistModeDLNA: show_menu = (void*)&BrowseServersMenu; break;
#endif
			default: show_menu = (void*)&rtspStreamMenu;
		}
		rtsp_stopVideo(which);
		
		eprintf("%s: (2) interface_menuActionShowMenu, interfaceInfo.currentMenu = %p\n", 
			__FUNCTION__, interfaceInfo.currentMenu);
		
		interface_menuActionShowMenu(interfaceInfo.currentMenu, show_menu);
		interface_showMenu(1, 0);
		interface_showMessageBox(_T("ERR_CONNECT"), thumbnail_error, 0);
	}

	interface_displayMenu(1);

	dprintf("%s: done\n", __FUNCTION__);

	return 0;
}

static void rtsp_setupPlayControl(void *pArg)
{
	int streamNumber = STREAM_INFO_GET_STREAM((int)pArg);
	int buttons;
	buttons = interfacePlayControlStop|interfacePlayControlPause|interfacePlayControlRewind|interfacePlayControlFastForward|interfacePlayControlPlay;
	buttons |= appControlInfo.playbackInfo.playlistMode != playlistModeFavorites ? interfacePlayControlAddToPlaylist : interfacePlayControlMode;

	if (stream_info.custom_url == 0 || appControlInfo.playbackInfo.playlistMode != playlistModeNone)
	{
		buttons |= interfacePlayControlPrevious|interfacePlayControlNext;
	}
	if( streamNumber != CHANNEL_CUSTOM )
	{
		sprintf(appControlInfo.playbackInfo.description, "%s: %s", _T("MOVIE"), stream_info.streamname);
		appControlInfo.playbackInfo.thumbnail[0] = 0;
		appControlInfo.playbackInfo.channel = streamNumber+1;
	}

	interface_playControlSetup(rtsp_play_callback, pArg, buttons, appControlInfo.playbackInfo.description, thumbnail_vod);

	if (stream_info.custom_url == 0 || appControlInfo.playbackInfo.playlistMode != playlistModeNone)
	{
		playControlChannelCallback set_channel;
		switch (appControlInfo.playbackInfo.playlistMode)
		{
			case playlistModeFavorites:
				set_channel = playlist_setChannel;
				saveAppSettings();
				break;
			case playlistModeIPTV:
				set_channel = rtp_setChannel;
				sprintf(appControlInfo.rtpMenuInfo.lastUrl,"rtsp://%s:%d/%s",
				stream_info.ip,
				stream_info.port,
				stream_info.streamname);
				saveAppSettings();
				break;
#ifdef ENABLE_DLNA
			case playlistModeDLNA: set_channel = dlna_setChannel; break;
#endif
			default:
				set_channel = rtsp_setChannel;
		}
		interface_playControlSetChannelCallbacks(rtsp_startNextChannel, set_channel);
	}
	if( appControlInfo.playbackInfo.channel >= 0 )
		interface_channelNumberShow(appControlInfo.playbackInfo.channel);
}

static int rtsp_stream_start(void* pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	//int streamNumber = STREAM_INFO_GET_STREAM((int)pArg);

	dprintf("%s: stream check\n", __FUNCTION__);

	//printf("play: show menu %d, active %d\n", interfaceInfo.showMenu, appControlInfo.rtspInfo[which].active);

	if ( appControlInfo.rtspInfo[which].active == 0 )
	{

		eprintf("%s: starting video\n", __FUNCTION__);

		appControlInfo.playbackInfo.scale = 1.0;

		rtsp_startVideo(which);

		eprintf("%s: started video\n", __FUNCTION__);

		//if ( appControlInfo.rtspInfo[which].active != 0 )
		rtsp_setupPlayControl(pArg);
		/*else
		{
			interface_playControlDisable(0);
			stream_info.streamname[0] = 0;
			rtsp_fillMenu();
			return -1;
		}*/
	}

	eprintf("%s: refill menu\n", __FUNCTION__);
	//rtsp_fillMenu();
	rtsp_displayStreamMenu((void*)which);

	helperFlushEvents();

	eprintf("%s: done\n", __FUNCTION__);

	return appControlInfo.rtspInfo[which].active != 0 ? 0 : -1;
}

static streams_struct * get_stream(unsigned int index)
{
	streams_struct* stream_ptr = pstream_head;
	while ( stream_ptr && stream_ptr->index != index )
		stream_ptr = (streams_struct *) stream_ptr->next;
	return stream_ptr;
}

static int rtsp_stream_change(interfaceMenu_t *pMenu, void* pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	unsigned int streamNumber = STREAM_INFO_GET_STREAM((int)pArg);
	streams_struct* stream_ptr=NULL;
	int ret;

	dprintf("%s: in\n", __FUNCTION__);
	eprintf("%s: in\n", __FUNCTION__);

	interface_removeEvent(rtsp_stream_start, pArg);

	if ( appControlInfo.rtspInfo[which].active != 0 )
	{
		/*interface_playControlDisable(1);
		stream_info.streamname[0] = 0;*/
		eprintf("RTSP: stop video at %d\n", which);
		// force showState to NOT be triggered
		interfacePlayControl.activeButton = interfacePlayControlStop;
		rtsp_stopVideo(which);
	}

	if (streamNumber != CHANNEL_CUSTOM)
	{
		dprintf("%s: collect if needed\n", __FUNCTION__);

		if ( pstream_head != NULL )
		{
			//dprintf("%s: already have list\n", __FUNCTION__);
			stream_ptr = pstream_head;
		} else
		{
			//dprintf("%s: new list\n", __FUNCTION__);
			//ip_buildIPStreamMenu(which);
			//ret=find_streams(ppstream_head);
			ret=get_rtsp_streams(ppstream_head);
			dprintf("%s: Found streams %i\n", __FUNCTION__,ret);
			if ( (ret == 0)&&(pstream_head != NULL) )
			{
				stream_ptr = pstream_head;
			} else
			{
				interface_showMessageBox(_T("ERR_DEFAULT_STREAM"), thumbnail_warning, 0);
				eprintf("RTSP: Failed to find default stream\n");
				return -1;
			}
		}

		stream_ptr = get_stream(streamNumber);
		if( !stream_ptr )
		{
			interface_showMessageBox(_T("ERR_STREAM_IN_LIST"), thumbnail_error, 0);
			eprintf("RTSP: Stream number %d not found in linked list\n", streamNumber);
			return -1;
		}

		//dprintf("%s: stream_ptr->stream %s\n", __FUNCTION__, stream_ptr->stream);

		stream_info.port = appControlInfo.rtspInfo[which].RTSPPort;
		strcpy(stream_info.ip, appControlInfo.rtspInfo[which].streamIP);
		strcpy(stream_info.streamname, stream_ptr->stream);
		appControlInfo.playbackInfo.playlistMode = playlistModeNone;
		stream_info.custom_url = 0;
	}

	if ( rtsp_stream_start(pArg) == 0 )
	{
		interface_showMenu(0, 1);

		//interface_menuActionShowMenu(pMenu, (void*)&rtspMenu);

		return 0;
	}

	//rtsp_startVideo(which);

	return -1;
}

static int rtsp_setChannel(int channel, void* pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	streams_struct* stream_ptr=NULL;

	if( channel < 1 || pstream_head == NULL )
	{
		return 1;
	}
	channel--;
	stream_ptr = get_stream( (unsigned int)channel );
	if( !stream_ptr )
	{
		return 1;
	}
	rtsp_stream_change((interfaceMenu_t*)&rtspStreamMenu, (void*)STREAM_INFO_SET(which, channel));

	return 0;
}

int rtsp_fillStreamMenu(interfaceMenu_t *pMenu, void* pArg)
{
	int which;
	//int position = 0;
	char *str;

	which = (int) pArg;

	interface_clearMenuEntries((interfaceMenu_t*)&rtspStreamMenu);

	str = _T("SEARCHING_MOVIES");
	interface_addMenuEntryDisabled((interfaceMenu_t*)&rtspStreamMenu, str, thumbnail_search);

	collectFlag++;

	if( collectThread == 0)
	{
		pthread_create(&collectThread, NULL, rtsp_list_updater, (void*)which);
		pthread_detach(collectThread);
	}

	interface_setSelectedItem((interfaceMenu_t*)&rtspStreamMenu, MENU_ITEM_MAIN);

	interface_menuActionShowMenu(pMenu, (void*)&rtspStreamMenu);

	return 0;
}

static int rtsp_setChannelFromURL(interfaceMenu_t *pMenu, char *value, char* description, char* thumbnail, void* pArg)
{
	int which = (int) pArg;
	int res;
	rtsp_stream_info sinfo;
	url_desc_t url;

	if( value == NULL )
		return 1;

	strcpy(rtsp_lasturl, value);

	memset(&sinfo, 0, sizeof(rtsp_stream_info));

	if ((res = parseURL(value, &url)) != 0)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_URL"), thumbnail_error, 0);
		return -1;
	}

	if (url.protocol != mediaProtoRTSP)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_PROTO"), thumbnail_error, 0);
		return -1;
	}

	inet_addr_prepare(url.address);
	if (inet_addr(url.address) == INADDR_NONE || inet_addr(url.address) == INADDR_ANY)
	{
		struct hostent * h = gethostbyname(url.address);
		if (!h){
			interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
			return -1;
		}
		strcpy(url.address, inet_ntoa(*(struct in_addr*)h->h_addr));
		//eprintf ("%s: %s\n", __FUNCTION__, url.address);
	}

	strcpy(sinfo.ip, url.address);
	sinfo.port = url.port;
	strcpy(sinfo.streamname, url.stream);
	sinfo.custom_url = 1;
	memcpy(&stream_info, &sinfo, sizeof(rtsp_stream_info));

	if( !description )
		sprintf(appControlInfo.playbackInfo.description, "%s: rtsp://%s:%d/%s", _T("MOVIE"), stream_info.ip, stream_info.port, stream_info.streamname);
	else
		strcpy(appControlInfo.playbackInfo.description, description);
	if( !thumbnail )
		appControlInfo.playbackInfo.thumbnail[0] = 0;
	else
		strcpy(appControlInfo.playbackInfo.thumbnail, thumbnail);

	dprintf("%s: Change to %s\n", __FUNCTION__, sinfo.streamname);

	return rtsp_stream_change(pMenu, (void*)STREAM_INFO_SET(which, CHANNEL_CUSTOM));
}

int rtsp_playURL(int which, char *URL, char* description, char* thumbnail)
{
	return rtsp_setChannelFromURL(interfaceInfo.currentMenu, URL, description, thumbnail, (void*)which);
}

int rtsp_setChannelFromUserURL(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	appControlInfo.playbackInfo.playlistMode = playlistModeNone;
	return rtsp_setChannelFromURL(pMenu, value, NULL, NULL, pArg);
}

static char *rtsp_urlvalue(int num, void *pArg)
{
	return num == 0 ? rtsp_lasturl : NULL;
}

static int rtsp_enterURL(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_CUSTOM_RTSP_URL"), "\\w+", rtsp_setChannelFromUserURL, rtsp_urlvalue, inputModeABC, pArg);
	return 0;
}

void rtsp_cleanupMenu()
{
	if ( pstream_head != NULL && *ppstream_head != NULL )
	{
		clean_list(*ppstream_head);
		*ppstream_head = NULL;
	}

	mysem_destroy(rtsp_semaphore);
}

void rtsp_buildMenu(interfaceMenu_t *pParent)
{
	int rtsp_icons[4] = { 0, 0, statusbar_f3_add, statusbar_f4_enterurl };

	createListMenu(&rtspStreamMenu, _T("MOVIE_LIST"), thumbnail_vod, rtsp_icons, pParent,
					   /* interfaceInfo.clientX, interfaceInfo.clientY,
					   interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
					   NULL, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&rtspStreamMenu, rtsp_keyCallback);

	mysem_create(&rtsp_semaphore);

	rtspControl.startFromPos = 0;

	memset(&stream_info, 0, sizeof(rtsp_stream_info));

#ifdef ENABLE_TEST_MODE
	{
		char desc[MENU_ENTRY_INFO_LENGTH];
		strcpy(stream_info.streamname, "a.mpg");
		strcpy(stream_info.ip, "192.168.200.1");
		stream_info.port = 554;
		stream_info.custom_url = 1;
		sprintf(desc, "%s: %s", _T("MOVIE"), "rtsp://192.168.200.1:554/a.mpg");
		interface_playControlSetup(rtsp_play_callback, (void*)STREAM_INFO_SET(0, CHANNEL_CUSTOM), interfacePlayControlStop|interfacePlayControlPause|interfacePlayControlRewind|interfacePlayControlFastForward|interfacePlayControlPlay|interfacePlayControlPrevious|interfacePlayControlNext, desc, thumbnail_vod);
		/*sprintf(desc, "%s: rtsp://%s:%d/%s", _T("MOVIE"), stream_info.ip, stream_info.port, stream_info.streamname);
		interface_playControlSetup(rtsp_play_callback, NULL, (void*)STREAM_INFO_SET(0, CHANNEL_CUSTOM), interfacePlayControlStop|interfacePlayControlPause|interfacePlayControlRewind|interfacePlayControlFastForward|interfacePlayControlPlay, desc, IMAGE_DIR "thumbnail_vod.png", 0);*/
	}
#endif // #ifdef ENABLE_TEST_MODE

	//rtsp_fillMenu();
}

static int rtsp_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	unsigned int streamNumber;
	int ret;
	streams_struct* stream_ptr=NULL;
	char URL[MAX_URL];

	if( cmd->command == interfaceCommandBlue )
	{
		rtsp_enterURL(pMenu, (void*)screenMain);
		return 0;
	}

	if(pMenu->selectedItem < 0)
	{
		return 1;
	}

	streamNumber = STREAM_INFO_GET_STREAM((int)pArg);

	if ((int)pArg >= 0)
	{
		if (cmd->command == interfaceCommandYellow || cmd->command == interfaceCommandGreen)
		{
			dprintf("%s: collect if needed\n", __FUNCTION__);

			if ( pstream_head != NULL )
			{
				stream_ptr = pstream_head;
			} else
			{
				ret=get_rtsp_streams(ppstream_head);
				dprintf("%s: Found streams %i\n", __FUNCTION__,ret);
				eprintf("%s: Found streams %i\n", __FUNCTION__,ret);
				if ( (ret == 0)&&(pstream_head != NULL) )
				{
					stream_ptr = pstream_head;
				} else
				{
					interface_showMessageBox(_T("ERR_DEFAULT_STREAM"), thumbnail_warning, 0);
					eprintf("RTSP: Failed to find default stream\n");
					return 0;
				}
			}

			stream_ptr = get_stream(streamNumber);

			if ( !stream_ptr )
			{
				interface_showMessageBox(_T("ERR_STREAM_IN_LIST"), thumbnail_error, 0);
				eprintf("RTSP: Stream number not found in linked list\n");
				return 0;
			}

			sprintf(URL, "rtsp://%s:%d/%s", appControlInfo.rtspInfo[screenMain].streamIP, appControlInfo.rtspInfo[screenMain].RTSPPort, stream_ptr->stream);
			if( cmd->command == interfaceCommandYellow )
			{
				eprintf("RTSP: Add to Playlist '%s'\n",URL);
				playlist_addUrl(URL, stream_ptr->name ? stream_ptr->name : stream_ptr->stream);
			} else if (cmd->command == interfaceCommandGreen )
			{
				eprintf("RTSP: Stream %03d: '%s'\n", streamNumber, URL);
				if( stream_ptr->poster && stream_ptr->poster[0] )
				{
					interface_showPosterBox( stream_ptr->description,
					                         stream_ptr->name ? stream_ptr->name : stream_ptr->stream,
					                         INTERFACE_BORDER_RED, INTERFACE_BORDER_GREEN, INTERFACE_BORDER_BLUE, INTERFACE_BORDER_ALPHA,
					                         thumbnail_vod,
					                         stream_ptr->poster, NULL, (void*)streamNumber);
				}
				//interface_showMessageBox(URL, thumbnail_info, 0);
			}
			return 0;
		}
	}

	return 1;
}

static int rtsp_menuEntryDisplay(interfaceMenu_t* pMenu, DFBRectangle *rect, int i)
{
	if( pMenu->menuType != interfaceMenuList )
	{
		eprintf("%s: unsupported menu type %d\n", __FUNCTION__, pMenu->menuType);
		return -1;
	}
	int selected                   = pMenu->selectedItem == i;
	interfaceListMenu_t *pListMenu = (interfaceListMenu_t *)pMenu;
	int fh;
	int x,y;
	int r,g,b,a;

	switch( pListMenu->listMenuType )
	{
		case interfaceListMenuIconThumbnail:
			if ( pListMenu->baseMenu.menuEntry[i].type == interfaceMenuEntryText )
			{
				char entryText[MENU_ENTRY_INFO_LENGTH];
				int maxWidth;
				char *second_line;
				unsigned int index;
				streams_struct *stream_ptr;
				if ( selected )
				{
					DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
					// selection rectangle
					gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, rect->x, rect->y, rect->w, rect->h);
					/*
					if ( pArrow != NULL )
					{
						//dprintf("%s: draw arrow\n", __FUNCTION__);
						if ( pListMenu->listMenuType == interfaceListMenuBigThumbnail || pListMenu->listMenuType == interfaceListMenuNoThumbnail )
						{
							x = interfaceInfo.clientX+interfaceInfo.paddingSize;
							y = interfaceInfo.clientY+(interfaceInfo.paddingSize+fh)*(itemDisplayIndex+1)-INTERFACE_ARROW_SIZE;
						} else
						{
							x = interfaceInfo.clientX+interfaceInfo.paddingSize;
							y = interfaceInfo.clientY+(interfaceInfo.paddingSize+pListMenu->baseMenu.thumbnailHeight)*(itemDisplayIndex+1)-(pListMenu->baseMenu.thumbnailHeight+INTERFACE_ARROW_SIZE)/2;
						}
						interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR INTERFACE_ARROW_IMAGE, x, y, INTERFACE_ARROW_SIZE, INTERFACE_ARROW_SIZE, 0, 0, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop);
					}*/
				}
				if ( pListMenu->listMenuType == interfaceListMenuIconThumbnail && pListMenu->baseMenu.menuEntry[i].thumbnail > 0 )
				{
					x = rect->x+interfaceInfo.paddingSize+/*INTERFACE_ARROW_SIZE +*/ pListMenu->baseMenu.thumbnailWidth/2;
					y = rect->y+pListMenu->baseMenu.thumbnailHeight/2;
					index = STREAM_INFO_GET_STREAM((int)pListMenu->baseMenu.menuEntry[i].pArg);
					if( appControlInfo.rtspInfo[screenMain].usePlaylistURL == 0 || //pListMenu->baseMenu.menuEntry[i].thumbnail != thumbnail_multicast ||
						(stream_ptr = get_stream(index)) == NULL ||
					    stream_ptr->thumb == NULL || stream_ptr->thumb[0] == 0 ||
					    0 != interface_drawImage(DRAWING_SURFACE, stream_ptr->thumb, x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignCenter|interfaceAlignMiddle, 0, 0)
					)
						interface_drawImage(DRAWING_SURFACE, resource_thumbnails[pListMenu->baseMenu.menuEntry[i].thumbnail], x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignCenter|interfaceAlignMiddle, 0, 0);
					//interface_drawIcon(DRAWING_SURFACE, pListMenu->baseMenu.menuEntry[i].thumbnail, x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, 0, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignCenter|interfaceAlignMiddle);
				}
				//dprintf("%s: draw text\n", __FUNCTION__);
				if ( pListMenu->baseMenu.menuEntry[i].isSelectable )
				{
					//r = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_RED : INTERFACE_BOOKMARK_RED;
					//g = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_GREEN : INTERFACE_BOOKMARK_GREEN;
					//b = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_BLUE : INTERFACE_BOOKMARK_BLUE;
					//a = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_ALPHA : INTERFACE_BOOKMARK_ALPHA;
					r = INTERFACE_BOOKMARK_RED;
					g = INTERFACE_BOOKMARK_GREEN;
					b = INTERFACE_BOOKMARK_BLUE;
					a = INTERFACE_BOOKMARK_ALPHA;
				} else
				{
					r = INTERFACE_BOOKMARK_DISABLED_RED;
					g = INTERFACE_BOOKMARK_DISABLED_GREEN;
					b = INTERFACE_BOOKMARK_DISABLED_BLUE;
					a = INTERFACE_BOOKMARK_DISABLED_ALPHA;
				}
				pgfx_font->GetHeight(pgfx_font, &fh);
				second_line = strchr(pListMenu->baseMenu.menuEntry[i].info, '\n');
				if ( pListMenu->listMenuType == interfaceListMenuNoThumbnail )
				{
					maxWidth = interfaceInfo.clientWidth - interfaceInfo.paddingSize*4/*- INTERFACE_ARROW_SIZE*/ - INTERFACE_SCROLLBAR_WIDTH;
					x = rect->x+interfaceInfo.paddingSize;//+INTERFACE_ARROW_SIZE;
					y = rect->y;
				} else
				{
					maxWidth = interfaceInfo.clientWidth - interfaceInfo.paddingSize*5/* - INTERFACE_ARROW_SIZE*/ - interfaceInfo.thumbnailSize - INTERFACE_SCROLLBAR_WIDTH;
					x = rect->x+interfaceInfo.paddingSize*2/*+INTERFACE_ARROW_SIZE*/+pListMenu->baseMenu.thumbnailWidth;
					if (second_line)
						y = rect->y + pListMenu->baseMenu.thumbnailHeight/2 - 2; // + fh/4;
					else
						y =rect->y + pListMenu->baseMenu.thumbnailHeight/2 + fh/4;
				}

				tprintf("%c%c%s\t\t\t|%d\n", i == pListMenu->baseMenu.selectedItem ? '>' : ' ', pListMenu->baseMenu.menuEntry[i].isSelectable ? ' ' : '-', pListMenu->baseMenu.menuEntry[i].info, pListMenu->baseMenu.menuEntry[i].thumbnail);
				if (second_line)
				{
					char *info_second_line = entryText + (second_line - pListMenu->baseMenu.menuEntry[i].info) + 1;
					int length;

					interface_getMenuEntryInfo(pMenu,i,entryText,MENU_ENTRY_INFO_LENGTH);

					*second_line = 0;
					gfx_drawText(DRAWING_SURFACE, pgfx_font, r, g, b, a, x, y, pListMenu->baseMenu.menuEntry[i].info, 0, i == pListMenu->baseMenu.selectedItem);

					length = getMaxStringLengthForFont(pgfx_smallfont, info_second_line, maxWidth-fh);
					info_second_line[length] = 0;
					gfx_drawText(DRAWING_SURFACE, pgfx_smallfont, r, g, b, a, x+fh, y+fh, info_second_line, 0, i == pListMenu->baseMenu.selectedItem);
					*second_line = '\n';
				} else
				{
					gfx_drawText(DRAWING_SURFACE, pgfx_font, r, g, b, a, x, y, pListMenu->baseMenu.menuEntry[i].info, 0, i == pListMenu->baseMenu.selectedItem);
				}
			} else
			{
				return interface_menuEntryDisplay(pMenu, rect, i);
			}
			break;
		default:
			return interface_menuEntryDisplay(pMenu, rect, i);
	}

	return 0;
}
