
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
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>

/// sockets ///
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//////////////

#include <directfb.h>

#include "app_info.h"
#include "debug.h"
#include "media.h"
#include "sem.h"
#include "gfx.h"
#include "l10n.h"
#include "StbMainApp.h"
#include "menu_app.h"
#include "rtp.h"
#include "rtsp.h"
#include "rutube.h"
#include "off_air.h"
#include "sdp.h"
#include "list.h"
#include "rtp_common.h"
#include "playlist.h"
#include "sound.h"
#include "samba.h"
#include "dlna.h"
#include "youtube.h"

#ifdef ENABLE_VIDIMAX
#include "vidimax.h"
#endif

#if (defined STB225)
#include <sys/types.h>
#include <dirent.h>	
#endif

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define STREAM_INFO_SET(screen, channel) ((screen << 16) | (channel))

#define STREAM_INFO_GET_SCREEN(info)     (info >> 16)

#define STREAM_INFO_GET_STREAM(info)    (info & 0xFFFF)

#define MAX_BUF_LEN_GETSTREAMS (188*88)

#define MAX_READ_COUNT_GETSTREAMS (120)

#define MENU_ITEM_LAST (-3)
#define MENU_ITEM_PREV (-4)

#define ROOT         (media_browseType == mediaBrowseTypeUSB ? usbRoot : sambaRoot)
#define IS_USB(path) (strncmp((path), usbRoot, strlen(usbRoot))==0)
#define IS_SMB(path) (strncmp((path), sambaRoot, strlen(sambaRoot))==0)

#define FORMAT_ICON (appControlInfo.mediaInfo.typeIndex < 0 ? thumbnail_file : media_formats[appControlInfo.mediaInfo.typeIndex].icon )

#define IS_URL(x) ((strncasecmp((x), "http://", 7) == 0) || (strncasecmp((x), "rtmp://", 7) == 0) || (strncasecmp((x), "https://", 8) == 0) || (strncasecmp((x), "ftp://", 6) == 0))

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

typedef enum __mediaBrowseType_t
{
	mediaBrowseTypeUSB = 0,
	mediaBrowseTypeSamba,
	mediaBrowseTypeCount
} mediaBrowseType_t;

typedef struct __mediaFormatInfo_t
{
	char *name;
	char *ext;
} mediaFormatInfo_t;

typedef struct __mediaFormats_t
{
	const mediaFormatInfo_t* info;
	const size_t count;
	const imageIndex_t icon;
} mediaFormats_t;

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

/** Refresh content of BrowseFilesMenu
 *  Can change currentPath automatically if necessary. Initializes media_currentDirEntries and media_currentFileEntries. Don't use blocks.
 *  @param[in] pMenu Should be &BrowseFilesMenu
 *  @param[in] pArg  Index of selected item in updated menu. Can also have special values MENU_ITEM_LAST and MENU_ITEM_BACK
 */
static int  media_refreshFileBrowserMenu(interfaceMenu_t *pMenu, void* pArg);

/** ActivatedAction of BrowseFilesMenu
 *  Calls media_refreshFileBrowserMenu using blocks.
 */
static int  media_fillFileBrowserMenu(interfaceMenu_t *pMenu, void* pArg);

static int  media_fillSettingsMenu(interfaceMenu_t *pMenu, void* pArg);

//static int  media_startPlayback();
int  media_startPlayback();
void media_pauseOrStop(int stop);
inline static void media_pausePlayback();
static int  media_stream_change(interfaceMenu_t *pMenu, void* pArg);
#ifdef ENABLE_AUTOPLAY
static int  media_auto_play(void* pArg);
static int  media_stream_selected(interfaceMenu_t *pMenu, void* pArg);
static int  media_stream_deselected(interfaceMenu_t *pMenu, void* pArg);
#endif
static void media_setupPlayControl(void* pArg);
static int  media_check_storages(void* pArg);
static int  media_check_storage_availability(interfaceMenu_t *pMenu, void* pArg);
static int  media_leaving_browse_menu(interfaceMenu_t *pMenu, void* pArg);
static int  media_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
static int  media_settingsKeyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

int         media_startNextChannel(int direction, void* pArg);
int         media_getPosition(double *plength,double *pposition);
static void media_setStateCheckTimer(int which, int bEnable, int bRunNow);
static int  media_browseFolderMenu(interfaceMenu_t *pMenu, void* pArg);
static int  media_upFolderMenu(interfaceMenu_t *pMenu, void* pArg);
static int  media_toggleMediaType(interfaceMenu_t *pMenu, void* pArg);
static int  media_toggleSlideshowMode(interfaceMenu_t *pMenu, void* pArg);

static void media_freeBrowseInfo();

static int  media_select(const struct dirent * de, char *dir, int fmt);
static int  media_select_current(const struct dirent * de);
static int  media_select_list(const struct dirent * de);
static int  media_select_dir(const struct dirent * de);
static int  media_select_image(const struct dirent * de);

static int  media_adjustStoragePath(char* oldPath, char* newPath);
static void media_getFileDescription();

static int  media_slideshowEvent(void *pArg);
static int  media_showCover(char* filename);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static const mediaFormatInfo_t media_videoFormats[] =
{
	{ "MPEG-1",              ".m1v" },
	{ "MPEG-1",              ".mpv" },
	{ "MPEG-1",              ".dat" },
	{ "MPEG-2",              ".vob" },
	{ "MPEG-2",              ".mpg" },
	{ "MPEG-2",              ".mpeg" },
	{ "MPEG-2",              ".m2v" },
	{ "MPEG-4",              ".mp4" },
	{ "DivX/XviD",           ".avi" },
	{ "Windows Media Video", ".wmv" },
	{ "H.264 Video",         ".h264" },
	{ "H.264 Video",         ".264" },
	{ "MPEG TS",             ".m2ts" },
	{ "MPEG TS",             ".mpt" },
	{ "MPEG TS",             ".ts" },
	{ "MPEG TS",             ".mts" },
	{ "MPEG TS",             ".m2t" },
	{ "MPEG TS",             ".trt" },
	{ "Flash Video",         ".flv" },
	{ "MPEG-2 PS",           ".m2p" },
	{ "Matroska",            ".mkv" },
	{ "H.264 ES",            ".es_v_h264" },
	{ "MPEG-2 ES",           ".es_v_mpeg2" },
	{ "MPEG-4 ES",           ".es_v_mpeg4" }
};
static const mediaFormatInfo_t media_audioFormats[] =
{
	{ "MP3",                 ".mp3" },
	{ "Windows Media Audio", ".wma" },
	{ "Ogg Vorbis",          ".ogg" },
	{ "AAC",                 ".aac" },
	{ "AC3",                 ".ac3" },
	{ "MPEG-2 Audio",        ".m2a" },
	{ "MPEG-2 Audio",        ".mp2" },
	{ "MPEG-1 Audio",        ".m1a" },
	{ "MPEG-1 Audio",        ".mp1" },
	{ "MPEG-1 Audio",        ".mpa" },
	{ "Audio ES",            ".aes" }
};
static const mediaFormatInfo_t media_imageFormats[] =
{
	{ "JPEG",                ".jpeg" },
	{ "JPG",                 ".jpg" },
	{ "PNG",                 ".png" },
	{ "BMP",                 ".bmp" }
};

// Order must match mediaType enum from app_info.h
static const mediaFormats_t media_formats[] = {
	{ media_videoFormats, sizeof(media_videoFormats)/sizeof(mediaFormatInfo_t), thumbnail_usb_video },
	{ media_audioFormats, sizeof(media_audioFormats)/sizeof(mediaFormatInfo_t), thumbnail_usb_audio },
	{ media_imageFormats, sizeof(media_imageFormats)/sizeof(mediaFormatInfo_t), thumbnail_usb_image }
};

static int media_forceRestart = 0;

/* start/stop semaphore */
static pmysem_t  media_semaphore;
static pmysem_t  slideshow_semaphore;

interfaceListMenu_t media_settingsMenu;

/* File browser */
interfaceListMenu_t BrowseFilesMenu;

static struct dirent **media_currentDirEntries = NULL;
static int             media_currentDirCount = 0;
static struct dirent **media_currentFileEntries = NULL;
static int             media_currentFileCount = 0;

static char  * playingPath = (char*)usbRoot;
static char    media_previousDir[PATH_MAX] = { 0 };
static char    media_failedImage[PATH_MAX] = { 0 };
static char    media_failedMedia[PATH_MAX] = { 0 };

static mediaBrowseType_t media_browseType = mediaBrowseTypeUSB;

// sound volume fade-in effect
static pthread_t fadeinThread = 0;
static int fadeinCurVol = 100;
static int fadeinDestVol = 100;
static void *volume_fade_in(void *pArg);

#ifdef ENABLE_AUTOPLAY
static int playingStream = 0;
#endif

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceListMenu_t MediaMenu;
const  char    usbRoot[] = "/usb/";
// current path for selected media type
char           currentPath[PATH_MAX] = "/usb/";

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

void media_getFilename(int fileNumber)
{
	appControlInfo.playbackInfo.playlistMode = playlistModeNone;
	appControlInfo.playbackInfo.playingType = appControlInfo.mediaInfo.typeIndex;
	snprintf(appControlInfo.mediaInfo.filename, PATH_MAX, "%s%s", currentPath, media_currentFileEntries[ fileNumber - media_currentDirCount - 1 /* ".." */ ]->d_name);
}

mediaType media_getMediaType(char *filename)
{
	size_t i, nameLength, searchLength;
	nameLength = strlen(filename);

	for( i = 0; i < media_formats[mediaVideo].count; ++i)
	{
		searchLength = strlen(media_formats[mediaVideo].info[i].ext);
		//dprintf("%s: %s : %s\n", __FUNCTION__,de->d_name,searchStrings[i]);
		if(nameLength>searchLength && strcasecmp(&filename[nameLength-searchLength], media_formats[mediaVideo].info[i].ext)==0)
			return mediaVideo;
	}
	for( i = 0; i < media_formats[mediaAudio].count; ++i)
	{
		searchLength = strlen(media_formats[mediaAudio].info[i].ext);
		//dprintf("%s: %s : %s\n", __FUNCTION__,de->d_name,searchStrings[i]);
		if(nameLength>searchLength && strcasecmp(&filename[nameLength-searchLength], media_formats[mediaAudio].info[i].ext)==0)
			return mediaAudio;
	}
	for( i = 0; i < media_formats[mediaImage].count; ++i)
	{
		searchLength = strlen(media_formats[mediaImage].info[i].ext);
		//dprintf("%s: %s : %s\n", __FUNCTION__,de->d_name,searchStrings[i]);
		if(nameLength>searchLength && strcasecmp(&filename[nameLength-searchLength], media_formats[mediaImage].info[i].ext)==0)
			return mediaImage;
	}
	return mediaAll;
}

static int  media_adjustStoragePath(char *oldPath, char *newPath)
{
	char *filePath;
	struct dirent **storages;
	int             storageCount;
	int i;
	char _currentPath[PATH_MAX];

	if( media_scanStorages() < 1 )
		return -1;
	/* 10 == strlen("/usb/Disk "); */
	filePath = index(&oldPath[10],'/');
	if(filePath == NULL)
		return -1;
	dprintf("%s: %s\n", __FUNCTION__,filePath);
	strcpy(_currentPath,currentPath);
	strcpy(currentPath,usbRoot);
	storageCount = scandir(usbRoot, &storages, media_select_dir, alphasort);
	strcpy(currentPath,_currentPath);
	if(storageCount < 0)
	{
		eprintf("media: Error getting list of storages at '%s'\n",usbRoot);
		return -3;
	}
	for( i = 0; i < storageCount ; ++i )
	{
		sprintf(newPath,"%s%s%s", usbRoot, storages[i]->d_name, filePath);
		dprintf("%s: Trying '%s'\n", __FUNCTION__,newPath);
		if(helperFileExists(newPath))
		{
			for( ; i < storageCount; ++i )
				free(storages[i]);
			free(storages);
			return 0;
		}
		free(storages[i]);
	}
	free(storages);
	return -2;
}

int media_play_callback(interfacePlayControlButton_t button, void *pArg)
{
	double position,length_stream;
	int ret_val;
	char URL[MAX_URL], *str, *description;
	int j;

	//dprintf("%s: in\n", __FUNCTION__);

	switch(button)
	{
	case interfacePlayControlPrevious:
		media_startNextChannel(1, pArg);
		break;
	case interfacePlayControlNext:
		media_startNextChannel(0, pArg);
		break;
	case interfacePlayControlAddToPlaylist:
		if( appControlInfo.playbackInfo.playlistMode != playlistModeFavorites && appControlInfo.mediaInfo.filename[0] != 0)
		{
			if( appControlInfo.mediaInfo.bHttp )
			{
#ifdef ENABLE_YOUTUBE
				if( strstr( appControlInfo.mediaInfo.filename, "youtube.com/" ) != NULL )
				{
					playlist_addUrl( youtube_getCurrentURL(), interfacePlayControl.description );
					break;
				}
#endif
#ifdef ENABLE_RUTUBE
				if( strstr( appControlInfo.mediaInfo.filename, "rutube.ru/" ) != NULL )
				{
					playlist_addUrl( rutube_getCurrentURL(), interfacePlayControl.description );
					break;
				}
#endif
				str = appControlInfo.mediaInfo.filename;
				description = interfacePlayControl.description;
			} else
			{
				snprintf(URL, MAX_URL,"file://%s",appControlInfo.mediaInfo.filename);
				str = URL;
				description = rindex(str, '/');
				if( description != NULL )
				{
					description++;
				}
			}
			eprintf("media: Add to Playlist '%s' (%s)\n", str, description);
			playlist_addUrl(str, description);
		}
		break;
	case interfacePlayControlPlay:
		if ( !appControlInfo.mediaInfo.active )
		{
			appControlInfo.playbackInfo.scale = 1.0;
			media_startPlayback();
		}
		if (appControlInfo.mediaInfo.active && gfx_isTrickModeSupported(screenMain))
		{
			appControlInfo.playbackInfo.scale = 1.0;
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			interface_notifyText(NULL, 0);
			interface_playControlSelect(interfacePlayControlPlay);
			interface_playControlRefresh(1);
		}
		break;
	case interfacePlayControlFastForward:
		if ( !appControlInfo.mediaInfo.active )
		{
			media_startPlayback();
			if(gfx_isTrickModeSupported(screenMain))
			{
				gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			}
		}
		if (appControlInfo.mediaInfo.active && gfx_isTrickModeSupported(screenMain))
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
			sprintf(URL, "%1.0fx", appControlInfo.playbackInfo.scale);
			interface_notifyText(URL, 0);
			interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
		}
		break;
	case interfacePlayControlRewind:
		if ( !appControlInfo.mediaInfo.active )
		{
			media_startPlayback();
			if(gfx_isTrickModeSupported(screenMain))
			{
				gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			}
		}
		if (appControlInfo.mediaInfo.active && gfx_isTrickModeSupported(screenMain))
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
			sprintf(URL, "%1.0fx", appControlInfo.playbackInfo.scale);
			interface_notifyText(URL, 0);
			interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
		}
		break;
	case interfacePlayControlStop:
		if ( appControlInfo.mediaInfo.active || appControlInfo.mediaInfo.paused )
			media_stopPlayback();
		break;
	case interfacePlayControlPause:
		if (appControlInfo.mediaInfo.paused)
		{
			media_startPlayback();
			if( appControlInfo.playbackInfo.scale == 1.0 )
			{
				interface_notifyText(NULL, 0);
				interface_playControlSelect(interfacePlayControlPlay);
			} else
			{
				sprintf(URL, "%1.0fx", appControlInfo.playbackInfo.scale);
				interface_notifyText(URL, 0);
				interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
			}
		} else
		{
			if ( appControlInfo.mediaInfo.active )
				media_pausePlayback();
		}
		break;
	case interfacePlayControlSetPosition:
		if (appControlInfo.soundInfo.muted) j =-1;
		else {
			j = fadeinDestVol;
			if (appControlInfo.soundInfo.volumeLevel > j)
				j = appControlInfo.soundInfo.volumeLevel;
			fadeinCurVol = fadeinDestVol = 0;
			sound_setVolume(0);
		}

		if (!appControlInfo.mediaInfo.active && !appControlInfo.mediaInfo.paused )
		{
			position					=	interface_playControlSliderGetPosition();
			appControlInfo.playbackInfo.scale = 1.0;
			media_startPlayback();
			gfx_setVideoProviderPosition(screenMain,position);
			ret_val	=	media_getPosition(&length_stream,&position);
			//dprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);
			if((ret_val == 0)&&(position < length_stream))
			{
				interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position);
			}
		}else
		{
			position	=	interface_playControlSliderGetPosition();
			if (appControlInfo.mediaInfo.paused)
			{
				media_startPlayback();
			}
			gfx_setVideoProviderPosition(screenMain, position);
			if( gfx_isTrickModeSupported(screenMain) )
				gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			else
				appControlInfo.playbackInfo.scale = 1.0;
			// Call to media_startPlayback resets slider position, moving slider back:
			ret_val	=	media_getPosition(&length_stream,&position);
			//dprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);
			if((ret_val == 0)&&(position < length_stream))
			{
				interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position);
			}
			if( appControlInfo.playbackInfo.scale == 1.0 )
			{
				interface_notifyText(NULL, 0);
				interface_playControlSelect(interfacePlayControlPlay);
			} else
			{
				sprintf(URL, "%1.0fx", appControlInfo.playbackInfo.scale);
				interface_notifyText(URL, 0);
				interface_playControlSelect(appControlInfo.playbackInfo.scale > 0.0 ? interfacePlayControlFastForward : interfacePlayControlRewind);
			}
		}

		if (j!=-1) {
			fadeinDestVol = j;
			fadeinCurVol = 0;
		}

		break;
	case interfacePlayControlInfo:
		interface_playControlSliderEnable(!interface_playControlSliderIsEnabled());
		interface_displayMenu(1);
		return 0;
		break;
	case interfaceCommandGreen:
		interface_showMenu(1, 0);
		media_fillSettingsMenu(interfaceInfo.currentMenu,pArg);
		return 0;
		break;
	case interfacePlayControlMode:
		if( appControlInfo.playbackInfo.playlistMode == playlistModeIPTV && appControlInfo.rtpMenuInfo.epg[0] != 0 )
		{
			rtp_showEPG(screenMain, media_setupPlayControl);
			return 0;
		}
		break;
	default:
		// default action
		//dprintf("%s: unknown button = %d \n", __FUNCTION__,button);
		return 1;
		break;
	}

	interface_displayMenu(1);
	//dprintf("%s: done\n", __FUNCTION__);
	return 0;
}

#ifdef ENABLE_AUTOPLAY
static int media_auto_play(void* pArg)
{
	return media_stream_change((interfaceMenu_t*)&BrowseFilesMenu, pArg);
}

static int media_stream_selected(interfaceMenu_t *pMenu, void* pArg)
{
	//int streamNumber = (int)pArg;

	//dprintf("%s: in\n", __FUNCTION__);

	if ( /*strcmp(appControlInfo.mediaInfo.filename, &streams->items[streamNumber], sizeof(struct media_desc)) != 0 ||*/
		appControlInfo.mediaInfo.active == 0 )
	{
		playingStream = 0;

		//dprintf("%s: event %08X\n", __FUNCTION__, pArg);

		interface_addEvent(media_auto_play, pArg, 3000, 1);
	}

	//dprintf("%s: done\n", __FUNCTION__);
	return 0;
}

static int media_stream_deselected(interfaceMenu_t *pMenu, void* pArg)
{
	//dprintf("%s: in\n", __FUNCTION__);

	//dprintf("%s: event %08X\n", __FUNCTION__, pArg);

	interface_removeEvent(media_auto_play, pArg);

	if ( playingStream == 0 )
	{
		interface_playControlDisable(1);
		appControlInfo.mediaInfo.filename[0] = 0;
		media_stopPlayback();
	}

	//dprintf("%s: done\n", __FUNCTION__);
	return 0;
}
#endif

int media_startNextChannel(int direction, void* pArg)
{
	int             indexChange = (direction?-1:1);
	char            playingDir[MAX_URL];
	char           *str;
	struct dirent **playDirEntries;
	int             playDirCount;
	int             current_index = -1;
	int             new_index = -1;
	int             i;
	static int		lock = 0;
	int				counter = 0;

	if (lock)
	{
		dprintf("%s: Block recursion!\n", __FUNCTION__);
		return 0;
	}

	switch(appControlInfo.playbackInfo.playlistMode)
	{
		case playlistModeNone: break;
		case playlistModeFavorites:
			if( appControlInfo.mediaInfo.bHttp )
				str = appControlInfo.mediaInfo.filename;
			else
			{
				sprintf(playingDir,"file://%s",appControlInfo.mediaInfo.filename);
				str = playingDir;
			}
#ifdef ENABLE_YOUTUBE
			if( appControlInfo.playbackInfo.streamSource != streamSourceYoutube )
#endif
			playlist_setLastUrl(str);
			return playlist_startNextChannel(direction,(void*)-1);
		case playlistModeIPTV:
			return rtp_startNextChannel(direction, pArg);
		case playlistModeDLNA:
#ifdef ENABLE_DLNA
			return dlna_startNextChannel(direction,appControlInfo.mediaInfo.filename);
#else
			return 1;
#endif
		case playlistModeYoutube:
#ifdef ENABLE_YOUTUBE
			return youtube_startNextChannel(direction, pArg);
#else
			return 1;
#endif
		case playlistModeRutube:
#ifdef ENABLE_RUTUBE
			return rutube_startNextChannel(direction, pArg);
#else
			return 1;
#endif
	}
	if( appControlInfo.mediaInfo.bHttp )
	{
		//media_stopPlayback();
		return 1;
	}

	/* We are already playing some file in some (!=current) dir */
	/* So we need to navigate back to that dir and select next/previous file to play */
	strcpy(playingDir,appControlInfo.mediaInfo.filename);
	*(rindex(playingDir,'/')+1) = 0;
	playingPath = playingDir;
	playDirCount = scandir(playingDir, &playDirEntries, media_select_current, alphasort);
	if(playDirCount < 0)
	{
		interface_showMessageBox(_T("ERR_FILE_NOT_FOUND"), thumbnail_error, 0);
		return 1;
	}

	if(playDirCount < 2) /* Nothing to navigate to */
	{
		eprintf("%s: '%s' file count < 2\n", __FUNCTION__, playingDir);
		goto cleanup;
	}

	/* Trying to get index of playing file in directory listing */
	for( i = 0 ; i < playDirCount; ++i )
	{
		if(strstr(appControlInfo.mediaInfo.filename,playDirEntries[i]->d_name))
		{
			current_index = i;
			break;
		}
	}

	do
	{
		interfaceCommand_t cmd;

		if(appControlInfo.mediaInfo.playbackMode == playback_random)
		{
			srand(time(NULL));
			if(playDirCount < 3)
				new_index = (current_index +1 ) % playDirCount;
			else
				while( (new_index = rand() % playDirCount) == current_index );
		} else if(current_index>=0) // if we knew previous index, we can get new index
		{
			new_index = (current_index+playDirCount+indexChange) % playDirCount;
		}
		sprintf(appControlInfo.mediaInfo.filename,"%s%s",playingDir,playDirEntries[new_index]->d_name);
		snprintf(appControlInfo.playbackInfo.description, sizeof(appControlInfo.playbackInfo.description), "%s: %s", _T( "RECORDED_VIDEO"), playDirEntries[new_index]->d_name);
		appControlInfo.playbackInfo.description[sizeof(appControlInfo.playbackInfo.description)-1] = 0;
		appControlInfo.playbackInfo.thumbnail[0] = 0;

		if (new_index >= 0)
		{
			/* fileName already in the right place, pArg = -1 */
			lock = 1;
			dprintf("%s: new index = %d, playing '%s'\n", __FUNCTION__, new_index, appControlInfo.mediaInfo.filename);
			media_stream_change(interfaceInfo.currentMenu, (void*)CHANNEL_CUSTOM);
			lock = 0;
			if (appControlInfo.mediaInfo.active)
			{
				//dprintf("%s: finish.\n", __FUNCTION__);
				break;
			}
		}

		while ((cmd = helperGetEvent(0)) != interfaceCommandNone)
		{
			dprintf("%s: got command %d\n", __FUNCTION__, cmd);
			if (cmd != interfaceCommandCount)
			{
				dprintf("%s: exit on command %d\n", __FUNCTION__, cmd);
				/* Flush any waiting events */
				helperGetEvent(1);
				for( i = 0 ; i < playDirCount; ++i )
					free(playDirEntries[i]);
				free(playDirEntries);
				return -1;
			}
		}
		if (!keepCommandLoopAlive)
		{
			break;
		}

		dprintf("%s: at %d tried %d next in %d step %d\n", __FUNCTION__, current_index, new_index, playDirCount, indexChange);

		counter++;
		current_index = new_index;
	} while (new_index >= 0 && counter < playDirCount);

cleanup:
	for( i = 0 ; i < playDirCount; ++i )
		free(playDirEntries[i]);
	free(playDirEntries);

	return 0;
}

static void media_getFileDescription()
{
	char *str;
	str = strrchr(appControlInfo.mediaInfo.filename, '/');
	if( str == NULL )
		str = appControlInfo.mediaInfo.filename;
	else
		str++;
	snprintf(appControlInfo.playbackInfo.description, sizeof(appControlInfo.playbackInfo.description), "%s: %s", _T( "RECORDED_VIDEO"), str);
	appControlInfo.playbackInfo.description[sizeof(appControlInfo.playbackInfo.description)-1] = 0;
}

/** Start playing specified or preset stream
 *  @param[in] pArg Stream number in BrowseFilesMenu or CHANNEL_CUSTOM
 */
static int media_stream_change(interfaceMenu_t *pMenu, void* pArg)
{
	int fileNumber = (int)pArg;

	dprintf("%s: in, fileNumber = %d\n", __FUNCTION__, fileNumber);

	if( CHANNEL_CUSTOM != fileNumber )
		appControlInfo.mediaInfo.bHttp = 0;

	if( CHANNEL_CUSTOM != fileNumber && fileNumber >= 0 &&
		(mediaImage == appControlInfo.mediaInfo.typeIndex || (appControlInfo.mediaInfo.typeIndex < 0 &&
		 mediaImage == media_getMediaType(media_currentFileEntries[ fileNumber - media_currentDirCount - 1]->d_name))) )
	{
		snprintf(appControlInfo.slideshowInfo.filename, PATH_MAX, "%s%s", currentPath, media_currentFileEntries[ fileNumber - media_currentDirCount - 1 /* ".." */ ]->d_name);
		appControlInfo.slideshowInfo.showingCover = 0;
		dprintf("%s: starting slideshow from '%s'\n", __FUNCTION__, appControlInfo.slideshowInfo.filename);
		if( media_slideshowStart() == 0 )
		{
			interface_showSlideshowControl();
			return 0;
		}
		return 1;
	}

#ifdef ENABLE_AUTOPLAY
	interface_removeEvent(media_auto_play, pArg);
#endif

	gfx_stopVideoProviders(screenMain);

	media_setupPlayControl(pArg);

	dprintf("%s: start video\n", __FUNCTION__);

	appControlInfo.playbackInfo.scale = 1.0;

	media_startPlayback();	

	helperFlushEvents();

	dprintf("%s: done %d\n", __FUNCTION__, appControlInfo.mediaInfo.active);

	return appControlInfo.mediaInfo.active != 0 ? 0 : 1;
}

static void media_setupPlayControl(void* pArg)
{
	int fileNumber = (int)pArg;
	int buttons;
	playControlChannelCallback set_channel  = NULL;
	playControlChannelCallback next_channel = media_startNextChannel;

	buttons = interfacePlayControlPlay|interfacePlayControlPause|interfacePlayControlStop;
	if( appControlInfo.mediaInfo.bHttp == 0 )
		buttons |= interfacePlayControlPrevious|interfacePlayControlNext|interfacePlayControlMode;
	else
		switch( appControlInfo.playbackInfo.playlistMode )
		{
			case playlistModeNone: break;
			case playlistModeDLNA:
			case playlistModeIPTV:
			case playlistModeFavorites:
			case playlistModeYoutube:
			case playlistModeRutube:
				buttons |= interfacePlayControlPrevious|interfacePlayControlNext|interfacePlayControlMode;
				break;
		}

	if(appControlInfo.playbackInfo.playlistMode != playlistModeFavorites
#ifdef ENABLE_YOUTUBE
	   // youtube entries should never be added to playlist usual way
	   && !(appControlInfo.mediaInfo.bHttp && strstr( appControlInfo.mediaInfo.filename, "youtube.com/" ) != NULL)
#endif
#ifdef ENABLE_RUTUBE
	   && !(appControlInfo.mediaInfo.bHttp && strstr( appControlInfo.mediaInfo.filename, "rutube.ru/" ) != NULL)
#endif
	  )
	{
		buttons |= interfacePlayControlAddToPlaylist;
	}
#ifdef ENABLE_PVR
	if( appControlInfo.mediaInfo.bHttp )
		buttons|= interfacePlayControlRecord;
#endif

	if(CHANNEL_CUSTOM != fileNumber && fileNumber >= 0) // Playing from media menu - initializing play control
	{
		media_getFilename(fileNumber);
		media_getFileDescription();
		appControlInfo.playbackInfo.thumbnail[0] = 0;
		appControlInfo.playbackInfo.channel = -1;
	}

	interface_playControlSetup(media_play_callback,
		pArg,
		buttons,
		appControlInfo.playbackInfo.description,
		appControlInfo.mediaInfo.bHttp ? thumbnail_internet : ( IS_USB( appControlInfo.mediaInfo.filename ) ? thumbnail_usb : thumbnail_workstation) );
	set_channel = NULL;
	switch (appControlInfo.playbackInfo.playlistMode)
	{
		case playlistModeFavorites: set_channel = playlist_setChannel; break;
		case playlistModeIPTV:      set_channel = rtp_setChannel;
			strcpy(appControlInfo.rtpMenuInfo.lastUrl, appControlInfo.mediaInfo.filename);
			break;
#ifdef ENABLE_DLNA
		case playlistModeDLNA:      set_channel = dlna_setChannel; break;
#endif
#ifdef ENABLE_YOUTUBE
		case playlistModeYoutube:   
			set_channel  = youtube_setChannel;
			next_channel = youtube_startNextChannel;
			break;
#endif
#ifdef ENABLE_RUTUBE
		case playlistModeRutube:   
			set_channel  = rutube_setChannel;
			next_channel = rutube_startNextChannel;
			break;
#endif
		default:
			break;
	}
	interface_playControlSetChannelCallbacks(next_channel, set_channel);
	if( appControlInfo.playbackInfo.channel >= 0 )
		interface_channelNumberShow(appControlInfo.playbackInfo.channel);
}

int media_streamStart()
{
	return media_stream_change((interfaceMenu_t*)&BrowseFilesMenu, (void*)CHANNEL_CUSTOM);
}

static int media_onStop()
{
	media_stopPlayback();
	switch(appControlInfo.mediaInfo.playbackMode)
	{
	case playback_looped:
//#if (defined STB225)
//		return 0;
//#else
		return media_startPlayback();
//#endif
	case playback_sequential:
		return media_startNextChannel(0,NULL);
	default:
		break;
	}
	return 0;
}

int media_check_status(void *pArg)
{
	DFBVideoProviderStatus status;
	double 	length_stream;
	double 	position_stream;
	int		ret_val;

	status = gfx_getVideoProviderStatus(screenMain);

	dprintf("%s: %d\n", __FUNCTION__, status);

	switch( status )
	{
		case DVSTATE_FINISHED:
			interface_showMessageBox(_T("ERR_STREAM_NOT_SUPPORTED"), thumbnail_error, 3000);
		case DVSTATE_STOP:
			// FIXME: STB810/225's ES video provider returns 'finished' status immediately
			// after it has finished reading input file, while decoder still processes remaining
			// data in buffers.
			
#ifdef ENABLE_VIDIMAX
			if (appControlInfo.vidimaxInfo.active && !appControlInfo.vidimaxInfo.seeking){
				vidimax_stopVideoCallback();
			}
#endif	
			media_onStop();
			break;
		case DVSTATE_STOP_REWIND:
			appControlInfo.playbackInfo.scale = 1.0;
			gfx_setSpeed(screenMain, appControlInfo.playbackInfo.scale);
			interface_notifyText(NULL, 0);
			interface_playControlSelect(interfacePlayControlPlay);
		default:
			ret_val	=	media_getPosition(&length_stream,&position_stream);

			//dprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);

			if((ret_val == 0)&&(position_stream < length_stream))
			{
				interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position_stream);
			}
			else if( ret_val == 0 )
			{
				if(!appControlInfo.mediaInfo.paused)
				{
					media_onStop();
				}
			}
			
			if( interfaceInfo.notifyText[0] != 0 && appControlInfo.playbackInfo.scale == 1.0 )
			{
				interface_notifyText(NULL, 1);
			}
			
			interface_addEvent(media_check_status, pArg, 1000, 1);
	}

	return 0;
}

void media_stopPlayback(void)
{
	media_pauseOrStop(1);
}

inline static void media_pausePlayback()
{
	media_pauseOrStop(0);
}

void media_pauseOrStop(int stop)
{
	mysem_get(media_semaphore);

	//media_setStateCheckTimer(screenMain, 0, 0);
	if( interfaceInfo.notifyText[0] != 0 && appControlInfo.playbackInfo.scale == 1.0)
	{
		interface_notifyText(NULL, 1);
	}
	interface_playControlSetInputFocus(inputFocusPlayControl);

	if ( stop )
	{
		interface_notifyText(NULL,0);
		interface_playControlSlider(0, 0, 0);
		interface_playControlSelect(interfacePlayControlStop);
	} else
	{
		interface_playControlSelect(interfacePlayControlPause);
	}
	//interface_playControlRefresh(1);

	dprintf("%s: %s\n", __FUNCTION__, stop ? "stop" : "pause");

#ifndef STBxx
	media_forceRestart = (gfx_getVideoProviderLength(screenMain) == gfx_getVideoProviderPosition(screenMain));
#endif

	appControlInfo.mediaInfo.endOfStream = 0;

	interface_removeEvent(media_check_status, NULL);

	//dprintf("%s:\n -->>>>>>> Stop, stop: %d, stop restart: %d, length: %f, position: %f\n\n", __FUNCTION__, stop, media_forceRestart, gfx_getVideoProviderLength(screenMain), gfx_getVideoProviderPosition(screenMain));
	gfx_stopVideoProvider(screenMain, stop, stop);

	interface_setBackground(0,0,0,0,NULL);

	dprintf("%s: Set playback inactive\n", __FUNCTION__);
	appControlInfo.mediaInfo.active = 0;
	appControlInfo.mediaInfo.paused = !stop;

	mysem_release(media_semaphore);
}

int media_slideshowStart()
{
	char *failedDir, *str;
	int firstStart = appControlInfo.slideshowInfo.state == 0;
	interfacePlayControlButton_t slideshowButton;

	dprintf("%s: %s\n", __FUNCTION__, appControlInfo.slideshowInfo.filename);

	if( helperFileExists(appControlInfo.slideshowInfo.filename) )
	{
		slideshowButton = interfaceSlideshowControl.highlightedButton;
		if(gfx_videoProviderIsActive(screenMain))
		{
			if(gfx_getVideoProviderHasVideo(screenMain))
			{
				gfx_stopVideoProviders(screenMain);
				interface_playControlDisable(0);
				interface_playControlSetInputFocus(inputFocusSlideshow);
				//interfacePlayControl.visibleFlag = 0;
			}
		} else
		{
			interface_playControlDisable(0);
			interface_playControlSetInputFocus(inputFocusSlideshow);
			//interfacePlayControl.highlightedButton = 0;
			//interfacePlayControl.visibleFlag = 0;
		}
		if( firstStart && !(appControlInfo.slideshowInfo.showingCover) )
			interface_playControlSetInputFocus(inputFocusSlideshow);
		interfaceSlideshowControl.highlightedButton = slideshowButton;

		appControlInfo.slideshowInfo.state = appControlInfo.slideshowInfo.defaultState;
		if(gfx_decode_and_render_Image(appControlInfo.slideshowInfo.filename) != 0)
		{
			interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_usb_image, 3000);
			failedDir = rindex(media_failedImage,'/');
			if(media_failedImage[0] == 0 || ( failedDir != NULL && strncmp( appControlInfo.slideshowInfo.filename, media_failedImage, failedDir - media_failedImage ) != 0 ))
			{
				strcpy(media_failedImage,appControlInfo.slideshowInfo.filename); // memorizing first failed image
			} else if(strcmp(media_failedImage,appControlInfo.slideshowInfo.filename) != 0) // another failed picure, trying next
			{
				media_slideshowNext(0);
				return 0;
			}
			goto failure; // after some failed images we returned to first one - cycle
		}
		if( appControlInfo.slideshowInfo.state == 0 ) // slideshow was stopped in the middle
		{
			goto failure;
		}
		gfx_showImage(screenPip); //actually show picture
		if( appControlInfo.slideshowInfo.state > slideshowImage )
		{
			interface_addEvent(media_slideshowEvent, NULL, appControlInfo.slideshowInfo.timeout, 1);
		}
		media_failedImage[0] = 0; // image loaded successfully - reseting failed marker
		if( gfx_videoProviderIsActive( screenMain ) == 0 )
		{
			str = rindex(appControlInfo.slideshowInfo.filename, '/');
			if( str != NULL )
			{
				interface_playControlUpdateDescriptionThumbnail( str+1, thumbnail_usb_image );
				appControlInfo.playbackInfo.thumbnail[0] = 0;
				if( firstStart && interfacePlayControl.showOnStart )
					interface_playControlRefresh(0);
			}
		}
		interfaceSlideshowControl.enabled = 1;
		interface_displayMenu(1);
		return 0;
	}
	//interfaceSlideshowControl.enabled = appControlInfo.slideshowInfo.state > 0;
failure:
	media_slideshowStop(0);
	return 1;
}

int  media_slideshowSetMode(int mode)
{
	if (mode < 0)
	{
		switch(appControlInfo.slideshowInfo.defaultState)
		{
			case slideshowImage:
				appControlInfo.slideshowInfo.defaultState = slideshowShow;

				break;
			case slideshowShow:
				appControlInfo.slideshowInfo.defaultState = slideshowRandom;
				interface_addEvent(media_slideshowEvent, NULL, appControlInfo.slideshowInfo.timeout, 1);
				break;
			default:
				interface_removeEvent(media_slideshowEvent, NULL);
				appControlInfo.slideshowInfo.defaultState = slideshowImage;
		}
	}
	else
		appControlInfo.slideshowInfo.defaultState = mode;
	saveAppSettings();

	if( appControlInfo.slideshowInfo.state != slideshowDisabled )
	{
		appControlInfo.slideshowInfo.state = appControlInfo.slideshowInfo.defaultState;
		if (appControlInfo.slideshowInfo.state > slideshowImage )
		{
			interface_addEvent(media_slideshowEvent, NULL, appControlInfo.slideshowInfo.timeout, 1);
		} else
		{
			interface_removeEvent(media_slideshowEvent, NULL);
		}
	}

	return 0;
}

int  media_slideshowSetTimeout(int timeout)
{
	switch (timeout)
	{
		case VALUE_NEXT:
			switch(appControlInfo.slideshowInfo.timeout)
			{
				case 3000:
					appControlInfo.slideshowInfo.timeout = 10000;
					break;
				case 10000:
					appControlInfo.slideshowInfo.timeout = 30000;
					break;
				case 30000:
					appControlInfo.slideshowInfo.timeout = 60000;
					break;
				case 60000:
					appControlInfo.slideshowInfo.timeout = 120000;
					break;
				case 120000:
					appControlInfo.slideshowInfo.timeout = 300000;
					break;
				default:
					appControlInfo.slideshowInfo.timeout = 3000;
			}
			break;
		case VALUE_PREV:
			switch(appControlInfo.slideshowInfo.timeout)
			{
				case 300000:
					appControlInfo.slideshowInfo.timeout = 120000;
					break;
				case 120000:
					appControlInfo.slideshowInfo.timeout = 60000;
					break;
				case 60000:
					appControlInfo.slideshowInfo.timeout = 30000;
					break;
				case 30000:
					appControlInfo.slideshowInfo.timeout = 10000;
					break;
				default: // 10000
					appControlInfo.slideshowInfo.timeout = 3000;
			}
			break;
		default:
			appControlInfo.slideshowInfo.timeout = timeout;
	}
	return 0;
}

static int media_slideshowEvent(void *pArg)
{
	int result;

	dprintf("%s: in\n", __FUNCTION__);

	gfx_waitForProviders();
	mysem_get(media_semaphore);
	mysem_release(media_semaphore);
	if( appControlInfo.slideshowInfo.state == 0 )
		return 1;

	result = media_slideshowNext(0);

	if(result == 0)
	{
		mysem_get(slideshow_semaphore);
		interface_addEvent(media_slideshowEvent, NULL, appControlInfo.slideshowInfo.timeout, 1);
		mysem_release(slideshow_semaphore);
	}

	return result;
}

int  media_slideshowNext(int direction)
{
	int             indexChange = (direction?-1:1);
	struct dirent **imageDirEntries;
	int             imageDirCount;
	int             current_index = -1;
	int             new_index = -1;
	int             i;
	char           *delimeter,c;
	char            buf[BUFFER_SIZE];

	dprintf("%s: %d\n", __FUNCTION__, indexChange);

	delimeter = rindex(appControlInfo.slideshowInfo.filename,'/');
	if( delimeter == NULL )
		return -2;
	c = delimeter[1];
	delimeter[1] = 0;
	imageDirCount = scandir(appControlInfo.slideshowInfo.filename, &imageDirEntries, media_select_image, alphasort);
	delimeter[1] = c;

	if(imageDirCount < 0)
	{
		interface_removeEvent(media_slideshowEvent, NULL);
		interfaceSlideshowControl.enabled = 0;
		sprintf(buf,"%s: %s",_T("SLIDESHOW"),_T("ERR_FILE_NOT_FOUND"));
		interface_showMessageBox(buf, thumbnail_error, 0);
		return -1;
	}

	if (imageDirCount == 0)
	{
		/* There is no images on old place */
		free(imageDirEntries);
		interface_removeEvent(media_slideshowEvent, NULL);
		appControlInfo.slideshowInfo.state = slideshowImage;
		sprintf(buf,"%s: %s",_T("SLIDESHOW"),_T("ERR_FILE_NOT_FOUND"));
		interface_showMessageBox(buf, thumbnail_error, 0);
		return 1;
	}

	if(imageDirCount < 2) /* Nothing to navigate to */
	{
		for( i = 0 ; i < imageDirCount; ++i )
			free(imageDirEntries[i]);
		free(imageDirEntries);

		return 2;
	}

	/* Trying to get index of current image */
	for( i = 0 ; i < imageDirCount; ++i )
	{
		if(strcmp(&delimeter[1],imageDirEntries[i]->d_name) == 0)
		{
			current_index = i;
			break;
		}
	}
	if( appControlInfo.slideshowInfo.state == slideshowRandom )
	{
		srand(time(NULL));
		if(imageDirCount < 3)
			new_index = (current_index +1 ) % imageDirCount;
		else
			while( (new_index = rand() % imageDirCount) == current_index );
	}
	else if(current_index>=0)
	{
		new_index = (current_index+imageDirCount+indexChange) % imageDirCount;
	}

	if (new_index >= 0)
	{
		strcpy(&delimeter[1],imageDirEntries[new_index]->d_name);
	}

	for( i = 0 ; i < imageDirCount; ++i )
		free(imageDirEntries[i]);
	free(imageDirEntries);

	if (new_index >= 0)
	{
		/* fileName already in the right place, pArg = -1 */
		return media_slideshowStart();
	}
	return 0;
}

int media_slideshowStop(int disable)
{
	appControlInfo.slideshowInfo.state = 0;
	gfx_waitForProviders();
	mysem_get(slideshow_semaphore);
	interface_removeEvent(media_slideshowEvent, NULL);
	gfx_hideImage(screenPip);
	mysem_release(slideshow_semaphore);
	interfaceSlideshowControl.enabled = disable ? 0 : helperFileExists(appControlInfo.slideshowInfo.filename);
	interface_disableBackground();
	return 0;
}

static int media_showCover(char* filename)
{
	struct dirent **imageDirEntries;
	int             imageDirCount, i;
	char  *delimeter;

	strcpy(appControlInfo.slideshowInfo.filename,filename);

	delimeter = rindex(appControlInfo.slideshowInfo.filename,'/');
	if( delimeter == NULL )
		return -2;
	delimeter[1] = 0;
	imageDirCount = scandir(appControlInfo.slideshowInfo.filename, &imageDirEntries, media_select_image, alphasort);

	if(imageDirCount < 0)
	{
		appControlInfo.slideshowInfo.filename[0] = 0;
		media_slideshowStop(0);
		return -1;
	}

	if (imageDirCount == 0)
	{
		free(imageDirEntries);
		appControlInfo.slideshowInfo.filename[0] = 0;
		media_slideshowStop(0);
		return 1;
	}

	appControlInfo.slideshowInfo.showingCover = 1;
	strcpy(&delimeter[1],imageDirEntries[0]->d_name);
	for(i = 0; i < imageDirCount; ++i)
	{
		free(imageDirEntries[i]);
	}
	free(imageDirEntries);

	return media_slideshowStart();
}

int media_playURL(int which, char* URL, char *description, char* thumbnail)
{
	char *fileName = URL+7; /* 7 == strlen("file://"); */
	size_t url_length;

	if( IS_URL(URL) )
	{
		url_length = strlen(URL)+1;
		appControlInfo.mediaInfo.bHttp = 1;
		memcpy(appControlInfo.mediaInfo.filename,URL,url_length);
		if( description != NULL )
		{
			if( description != appControlInfo.playbackInfo.description )
				strncpy(appControlInfo.playbackInfo.description, description, sizeof(appControlInfo.playbackInfo.description)-1);
		} else
		{
			if( utf8_urltomb(appControlInfo.mediaInfo.filename, url_length,
			    appControlInfo.playbackInfo.description, sizeof(appControlInfo.playbackInfo.description)-1 ) < 0 )
			{
				strncpy(appControlInfo.playbackInfo.description, appControlInfo.mediaInfo.filename, sizeof(appControlInfo.playbackInfo.description)-1);
			}
		}
		appControlInfo.playbackInfo.description[sizeof(appControlInfo.playbackInfo.description)-1] = 0;
		if( thumbnail )
		{
			if( thumbnail != appControlInfo.playbackInfo.thumbnail )
				strcpy(appControlInfo.playbackInfo.thumbnail, thumbnail);
		}
		else
			appControlInfo.playbackInfo.thumbnail[0] = 0;
			//strcpy(appControlInfo.playbackInfo.thumbnail, resource_thumbnails[thumbnail_internet]);
			
		return media_stream_change((interfaceMenu_t *)&BrowseFilesMenu, (void*)CHANNEL_CUSTOM);
	}

	dprintf("%s: '%s'\n", __FUNCTION__,fileName);
	appControlInfo.mediaInfo.bHttp = 0;
	switch(media_getMediaType(fileName))
	{
		case mediaImage:
			{
				int res;
				strcpy(appControlInfo.slideshowInfo.filename,fileName);
				if( (res = media_slideshowStart()) == 0 )
				{
					interface_showSlideshowControl();
				}
				return res;
			}
		default:
#if (defined STB225)
			strcpy(appControlInfo.mediaInfo.filename,URL);
//			strcpy(appControlInfo.mediaInfo.filename,fileName);
#else
			strcpy(appControlInfo.mediaInfo.filename,fileName);
#endif
			if( description )
			{
				strncpy(appControlInfo.playbackInfo.description, description, sizeof(appControlInfo.playbackInfo.description)-1);
				appControlInfo.playbackInfo.description[sizeof(appControlInfo.playbackInfo.description)-1] = 0;
					//strcpy(appControlInfo.playbackInfo.thumbnail, resource_thumbnails[
					//( IS_USB( appControlInfo.mediaInfo.filename ) ? thumbnail_usb : thumbnail_workstation) ]);
			} else
				media_getFileDescription();
			if( thumbnail )
				strcpy(appControlInfo.playbackInfo.thumbnail, thumbnail);
			else
				appControlInfo.playbackInfo.thumbnail[0] = 0;
			return media_stream_change((interfaceMenu_t *)&BrowseFilesMenu, (void*)-1);
	}
}

static void *volume_fade_in(void *pArg) {
	while (1) {

	if (fadeinCurVol>=fadeinDestVol || appControlInfo.soundInfo.muted)
		usleep(300000);
	else {
		do {
			sound_setVolume(fadeinCurVol);
        		usleep(100000);
			if (fadeinCurVol != appControlInfo.soundInfo.volumeLevel || appControlInfo.soundInfo.muted) {
				break;
			}
			fadeinCurVol += 5;
		} while (fadeinCurVol<fadeinDestVol);
	        sound_setVolume(fadeinDestVol);
		appControlInfo.soundInfo.volumeLevel = fadeinDestVol;
		fadeinDestVol = 0;
	}
	}
	return NULL;
}

//static int media_startPlayback()
int media_startPlayback()
{
	int res;
	int alreadyExist;
	char altPath[PATH_MAX];
	char *fileName, *failedDir;

	int j;
	if (appControlInfo.soundInfo.muted) j =-1;
	else {
		j = fadeinDestVol;
		if (appControlInfo.soundInfo.volumeLevel > j)
			j = appControlInfo.soundInfo.volumeLevel;
		fadeinCurVol = fadeinDestVol = 0;
		sound_setVolume(0);
	}

	if( appControlInfo.mediaInfo.bHttp || helperFileExists(appControlInfo.mediaInfo.filename) )
	{
		fileName = appControlInfo.mediaInfo.filename;
		/* Check USB storage still plugged in */
		if( appControlInfo.mediaInfo.bHttp == 0 && IS_USB(fileName) )
		{
			strcpy(altPath,"/dev/sd__");
			altPath[7] = 'a' + appControlInfo.mediaInfo.filename[10] - 'A';
			if( appControlInfo.mediaInfo.filename[11] == '/' )
			{
				altPath[8] = 0;
			} else
			{
				altPath[8] = appControlInfo.mediaInfo.filename[22];
			}
			if( !check_file_exsists(altPath) )
			{
				interface_showMessageBox(_T("ERR_FILE_NOT_FOUND"), thumbnail_error, 3000);
				interface_hideLoading();
				if (j!=-1) sound_setVolume(j);
				return 1;
			}
		}
	} else
	{
		dprintf("%s: File '%s' not found on it's place\n", __FUNCTION__,appControlInfo.mediaInfo.filename);
		if(media_adjustStoragePath(appControlInfo.mediaInfo.filename, altPath) == 0)
		{
			fileName = altPath;
		} else
		{
			eprintf("media: File '%s' not found\n", appControlInfo.mediaInfo.filename);
			interface_showMessageBox(_T("ERR_FILE_NOT_FOUND"), thumbnail_error, 3000);
			interface_hideLoading();
			if (j!=-1) sound_setVolume(j);
			return 1;
		}
	}

	interface_showLoading();
	interface_displayMenu(1);

	interface_showLoadingAnimation();

	mysem_get(media_semaphore);

	dprintf("%s: Start media\n", __FUNCTION__);
	switch( appControlInfo.playbackInfo.playlistMode)
	{
		case  playlistModeNone:
			if( !appControlInfo.mediaInfo.bHttp )
			{
				appControlInfo.playbackInfo.streamSource = streamSourceUSB;
				strcpy( appControlInfo.mediaInfo.lastFile, appControlInfo.mediaInfo.filename );
			}
			break;
#ifdef ENABLE_YOUTUBE
		case playlistModeYoutube:
			strcpy(appControlInfo.mediaInfo.lastFile, youtube_getCurrentURL());
			break;
#endif
#ifdef ENABLE_RUTUBE
		case playlistModeRutube:
			strcpy(appControlInfo.mediaInfo.lastFile, rutube_getCurrentURL());
			break;
#endif
		default:;
	}
	saveAppSettings();
	appControlInfo.mediaInfo.endOfStream 			= 	0;
	appControlInfo.mediaInfo.endOfStreamReported 	= 	0;
	appControlInfo.mediaInfo.paused					= 	0;
	/* Creating provider can take some time, in which user can try to launch another file.
	To prevent it one should set active=1 in advance. (#10101) */
	appControlInfo.mediaInfo.active = 1;

	dprintf("%s: Start Media playback of %s force %d\n", __FUNCTION__, fileName, media_forceRestart);

	alreadyExist = gfx_videoProviderIsCreated(screenMain, fileName);

	res = gfx_startVideoProvider(fileName,
								screenMain,
								media_forceRestart,
								(appControlInfo.soundInfo.rcaOutput==1) ? "" : ":I2S0");

	dprintf("%s: Start provider: %d\n", __FUNCTION__, res);

	if ( res == 0 )
	{
		double 					length_stream;
		double 					position_stream;
		int						ret_val;


		/*// for HLS only
		if (strstr(appControlInfo.mediaInfo.filename, ".m3u8") != NULL){
			eprintf ("%s: .m3u8 started! setting position = 0...\n", __FUNCTION__);
			gfx_setVideoProviderPosition(screenMain, 0);		// here HLS video starts 	
		}
*/


#ifdef ENABLE_AUTOPLAY
		playingStream = 1;
#endif

		/* After video provider is created, it starts playing video.
		So hiding menus early as probing video position can take time on bad streams (#9351) */
		interface_disableBackground();
		interface_showMenu(0, 0);
		interface_hideLoadingAnimation();
		interface_hideLoading();

		media_failedMedia[0] = 0;
//SergA: wait some time for starting video provider
#if (defined STB225)
//		usleep(200000);
#endif
		if(!gfx_getVideoProviderHasVideo(screenMain))
		{
			if((appControlInfo.slideshowInfo.state == 0 || appControlInfo.slideshowInfo.showingCover == 1) && media_showCover(fileName) != 0)
			{
				interface_setBackground(0,0,0,0xFF, INTERFACE_WALLPAPER_IMAGE);
			}
		}
		else
		{
			media_slideshowStop(1);
			if( interfacePlayControl.descriptionThumbnail == thumbnail_usb_image ) // play control set to display slideshow info
			{
				// Refresh descrition
				if( appControlInfo.mediaInfo.bHttp == 0)
				{
					fileName = strrchr(appControlInfo.mediaInfo.filename, '/');
					if( fileName == NULL )
						fileName = appControlInfo.mediaInfo.filename;
					else
						fileName++;
					sprintf(altPath, "%s: %s", _T( "RECORDED_VIDEO"), fileName);
				}
				interface_playControlUpdateDescriptionThumbnail( appControlInfo.mediaInfo.bHttp ? appControlInfo.mediaInfo.filename : altPath,
					appControlInfo.mediaInfo.bHttp ? thumbnail_internet : ( IS_USB( appControlInfo.mediaInfo.filename ) ? thumbnail_usb : thumbnail_workstation) );
			}
		}
		if (gfx_isTrickModeSupported(screenMain))
		{
			interface_playControlSetButtons( interface_playControlGetButtons()|interfacePlayControlFastForward|interfacePlayControlRewind );
		}
		interface_playControlSelect(interfacePlayControlStop);
		interface_playControlSelect(interfacePlayControlPlay);
		interface_displayMenu(1);
#ifdef ENABLE_VERIMATRIX
		if (appControlInfo.useVerimatrix != 0 && alreadyExist == 0)
		{
			eprintf("media: Enable verimatrix...\n");
			if (gfx_enableVideoProviderVerimatrix(screenMain, VERIMATRIX_INI_FILE) != 0)
			{
				interface_showMessageBox(_T("ERR_VERIMATRIX_INIT"), thumbnail_error, 0);
			}
		}
#endif
#ifdef ENABLE_SECUREMEDIA
		if (appControlInfo.useSecureMedia != 0 && alreadyExist == 0)
		{
			eprintf("media: Enable securemedia...\n");
			if (gfx_enableVideoProviderSecureMedia(screenMain) != 0)
			{
				interface_showMessageBox(_T("ERR_SECUREMEDIA_INIT"), thumbnail_error, 0);
			}
		}
#endif
		ret_val	=	media_getPosition(&length_stream,&position_stream);
		if(ret_val == 0)
		{
			//dprintf("%s: start from pos %f\n", __FUNCTION__, position_stream);
			interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position_stream);	
			
			// for HLS only
#ifdef ENABLE_VIDIMAX	
			if (!appControlInfo.vidimaxInfo.active)					
#endif	
				if (strstr(appControlInfo.mediaInfo.filename, ".m3u8") != NULL){
					eprintf ("%s: .m3u8 started! setting position = %d...\n", __FUNCTION__, position_stream);
					gfx_setVideoProviderPosition(screenMain, position_stream);		// here HLS video starts 	
				}
							
		}else
		{
			interface_playControlSlider(0, 0, 0);			

			// for HLS only
#ifdef ENABLE_VIDIMAX	
			if (!appControlInfo.vidimaxInfo.active)					
#endif	
			if (strstr(appControlInfo.mediaInfo.filename, ".m3u8") != NULL){
				eprintf ("%s: .m3u8 started! setting position = 0...\n", __FUNCTION__);
				gfx_setVideoProviderPosition(screenMain, 0);		// here HLS video starts 	
			}
		}
		//media_setStateCheckTimer(screenMain, 1, 0);

		// Forces play control to reappear after detecting slider
		//interface_playControlSelect(interfacePlayControlStop);
		//interface_playControlSelect(interfacePlayControlPlay);
		//interface_playControlRefresh(1);


		interface_addEvent(media_check_status, NULL, 1000, 1);

		media_forceRestart = 0;
		mysem_release(media_semaphore);
		if (j!=-1) {
			fadeinDestVol = j;
			fadeinCurVol = 0;
			if (fadeinThread == 0) {
				pthread_create(&fadeinThread, NULL, volume_fade_in, NULL);
			}
		}
	} else
	{
		eprintf("media: Failed to play '%s'\n", fileName);
		media_forceRestart = 1;
		appControlInfo.mediaInfo.active = 0;
		mysem_release(media_semaphore);
		interface_hideLoadingAnimation();
		interface_hideLoading();
		interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);

		if( appControlInfo.mediaInfo.bHttp == 0 && appControlInfo.mediaInfo.playbackMode != playback_looped && appControlInfo.mediaInfo.playbackMode != playback_single )
		{
			failedDir = rindex(media_failedMedia,'/');
			if(media_failedMedia[0] == 0 || ( failedDir != NULL && strncmp( appControlInfo.mediaInfo.filename, media_failedMedia, failedDir - media_failedMedia ) != 0 ))
			{
				strcpy(media_failedMedia,appControlInfo.mediaInfo.filename);
			} else if(strcmp(media_failedMedia,appControlInfo.mediaInfo.filename) == 0)
			{
				if (j!=-1) sound_setVolume(j);
				return 1;
			}
			res = media_startNextChannel(0,NULL);
		}
		if (j!=-1) sound_setVolume(j);
	}

	dprintf("%s: finished %d\n", __FUNCTION__, res);	
	
	return res;
}

#if 0
static int media_startStopPlayback(interfaceMenu_t *pMenu, void *pArg)
{
	if ( appControlInfo.mediaInfo.active )
	{
		media_stopPlayback();
	} else
	{
		media_startPlayback();
	}

	dprintf("%s: display Media playback output\n", __FUNCTION__);
	//menuInfra_display((void*)&MediaMenu);
	interface_menuActionShowMenu((interfaceMenu_t *)&MediaMenu, NULL);

	return 0;
}
#endif

int media_setMode(interfaceMenu_t *pMenu, void *pArg)
{
	appControlInfo.mediaInfo.playbackMode++;
	appControlInfo.mediaInfo.playbackMode %= playback_modes;
	saveAppSettings();
#if (defined STB225)
		gfx_setVideoProviderPlaybackFlags(screenMain, DVPLAY_LOOPING, (appControlInfo.mediaInfo.playbackMode==playback_looped) );
#endif

	media_fillSettingsMenu(pMenu,pArg);

	//menuInfra_display((void*)&MediaMenu);
	//interface_showMenu(1, 1);
	interface_displayMenu(1);
	//interface_menuActionShowMenu((interfaceMenu_t *)&MediaMenu, NULL);

	return 0;
}


void media_cleanupMenu()
{
	mysem_destroy(media_semaphore);
	mysem_destroy(slideshow_semaphore);
}

void media_buildMediaMenu(interfaceMenu_t *pParent)
{
	int media_icons[4] = { 0,
		statusbar_f2_settings,
#ifdef ENABLE_FAVORITES
		statusbar_f3_add,
#else
		0,
#endif
		statusbar_f4_filetype };

	createListMenu(&BrowseFilesMenu, _T("RECORDED_LIST"), thumbnail_usb, media_icons, pParent,
		interfaceListMenuIconThumbnail, media_fillFileBrowserMenu, media_leaving_browse_menu, (void*)MENU_ITEM_LAST);
	interface_setCustomKeysCallback((interfaceMenu_t*)&BrowseFilesMenu, media_keyCallback);

	media_icons[0] = 0;
	media_icons[1] = statusbar_f2_playmode;
	media_icons[2] = statusbar_f3_sshow_mode;
	createListMenu(&media_settingsMenu, _T("RECORDED_LIST"), thumbnail_usb, media_icons, (interfaceMenu_t*)&BrowseFilesMenu,
		interfaceListMenuIconThumbnail, NULL, NULL/*media_refreshFileBrowserMenu*/, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&media_settingsMenu, media_settingsKeyCallback);

	mysem_create(&media_semaphore);
	mysem_create(&slideshow_semaphore);
}

static int media_stateTimerEvent(void *pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	double 	length_stream;
	double 	position_stream;
	int		ret_val;

	if (gfx_getVideoProviderStatus(which) == DVSTATE_FINISHED)
	{
		interface_showMessageBox(_T("ERR_STREAM_NOT_SUPPORTED"), thumbnail_error, 3000);
		interface_playControlSlider(0,0,0);
		switch(appControlInfo.mediaInfo.playbackMode)
		{
		case playback_sequential:
			media_startNextChannel(0,NULL);
			break;
		default:
			media_stopPlayback();
			break;
		}
	} else
	{
		ret_val	=	media_getPosition(&length_stream,&position_stream);

		//dprintf("%s: got position %f, set it\n", __FUNCTION__, position_stream);

		if((ret_val == 0)&&(position_stream < length_stream))
		{
			interface_playControlSlider(0, (unsigned int)length_stream, (unsigned int)position_stream);
		}
		else if( ret_val == 0 )
		{
			if(!appControlInfo.mediaInfo.paused)
			{
				switch(appControlInfo.mediaInfo.playbackMode)
				{
				case playback_sequential:
					media_startNextChannel(0,NULL);
					break;
				case playback_looped:
					media_stopPlayback();
					media_startPlayback();
					break;
				default:
					media_stopPlayback();
					break;
				}
			}
		}
		media_setStateCheckTimer(which, 1, 0);
		if( interfaceInfo.notifyText[0] != 0 && appControlInfo.playbackInfo.scale == 1.0 )
		{
			interface_notifyText(NULL, 1);
		}
	}

	return 0;
}

static void media_setStateCheckTimer(int which, int bEnable, int bRunNow)
{
	dprintf("%s: %s state timer\n", __FUNCTION__, bEnable ? "set" : "unset");

	if (bEnable)
	{
		if (bRunNow)
		{
			//dprintf("%s: Update slider\n", __FUNCTION__);
			media_stateTimerEvent((void*)STREAM_INFO_SET(which, 0));
		}
		interface_addEvent(media_stateTimerEvent, (void*)STREAM_INFO_SET(which, 0), 1000, 1);
	} else
	{
		interface_removeEvent(media_stateTimerEvent, (void*)STREAM_INFO_SET(which, 0));
		if( interfaceInfo.notifyText[0] != 0 && appControlInfo.playbackInfo.scale == 1.0)
		{
			interface_notifyText(NULL, 1);
		}
	}
}


int media_getPosition(double *plength,double *pposition)
{
	double length;
	double position;

	length = gfx_getVideoProviderLength(screenMain);
	dprintf("%s: -- length = %f \n", __FUNCTION__,length);
	if (length<2)
		return -1;

	position = gfx_getVideoProviderPosition(screenMain);
	dprintf("%s: -- position = %f \n", __FUNCTION__,position);
	if (position>length)
		return -1;

	*plength				=	length;
	*pposition				=	position;

	return 0;
}

/* File browser */

int media_select_usb(const struct dirent * de)
{
	struct stat stat_info;
	int         status;
	char        full_path[PATH_MAX];
	if (strncmp(de->d_name, "Drive ", sizeof("Drive ")-1) == 0)
	{
		// Skip CD/DVD Drives since we add them manually
		return 0;
	}
	sprintf(full_path,"%s%s",usbRoot,de->d_name);
	status = stat( full_path, &stat_info);
	if(status<0)
		return 0;
	return S_ISDIR(stat_info.st_mode) && (de->d_name[0] != '.');
}

static int media_select_dir(const struct dirent * de)
{
	struct stat stat_info;
	int         status;
	char        full_path[PATH_MAX];
	int			isRoot;

	isRoot = (strcmp(currentPath,usbRoot) == 0);
	if (isRoot && strncmp(de->d_name, "Drive ", sizeof("Drive ")-1) == 0)
	{
		// Skip CD/DVD Drives since we add them manually
		return 0;
	}
	sprintf(full_path,"%s%s",currentPath,de->d_name);
	status = lstat( full_path, &stat_info);
	if(status<0)
		return 0;
	return S_ISDIR(stat_info.st_mode) && !S_ISLNK(stat_info.st_mode) && (de->d_name[0] != '.');
}

int media_select_list(const struct dirent * de)
{
    return media_select(de, currentPath, appControlInfo.mediaInfo.typeIndex);
}

int media_select_current(const struct dirent * de)
{
    return media_select(de, playingPath, appControlInfo.playbackInfo.playingType);
}

static int media_select_image(const struct dirent * de)
{
	return media_select(de, appControlInfo.slideshowInfo.filename, mediaImage );
}

int media_select(const struct dirent * de, char *dir, int fmt)
{
	struct stat stat_info;
	int         status;
	int         searchLength,nameLength;
	size_t      i;
	static char full_path[PATH_MAX];
	sprintf(full_path,"%s%s",dir,de->d_name);
	//dprintf("%s: '%s'\n", __FUNCTION__,full_path);
	status = lstat( full_path, &stat_info);
	/* Files with size > 2GB will give overflow error */
	if(status<0 && errno != EOVERFLOW)
	{
		return 0;
	} else if (status>=0 && (!S_ISREG(stat_info.st_mode) || S_ISLNK(stat_info.st_mode)) )
	{
		return 0;
	}
	if( fmt < 0 )
	{
		return 1;
	}
	nameLength=strlen(de->d_name);
	for( i = 0; i < media_formats[fmt].count; ++i)
	{
		searchLength = strlen(media_formats[fmt].info[i].ext);
		//dprintf("%s: %s : %s\n", __FUNCTION__,de->d_name,searchStrings[i]);

 		if(nameLength>searchLength && strcasecmp(&de->d_name[nameLength-searchLength], media_formats[fmt].info[i].ext)==0)
		{
			//dprintf("%s: found ok\n", __FUNCTION__);
			return 1;
		}
	}
	return 0;
}

int media_scanStorages()
{
	int devCount = 0;
#if !(defined STB225)
	char dev_path[MAX_PATH_LENGTH], usb_path[MAX_PATH_LENGTH];
	unsigned int dev, part;
	/*
	for (dev=0; dev < 10; dev++)
	{
		sprintf(dev_path, "/dev/sr%d", dev);
		if (check_file_exsists(dev_path))
		{
			devCount++;
		}
	}
	*/
	for (dev=0; 'a'+dev<='z'; dev++)
	{
		sprintf(dev_path, "/dev/sd%c", 'a'+dev);
		//dprintf("%s: check dev %s\n", __FUNCTION__, dev_path);
		if (check_file_exsists(dev_path))
		{
			for (part=0; part<=8; part++)
			{
				/* Now check if we have such partition on given device */
				if (part == 0)
				{
					/* Workaround for partitionless devices */
					sprintf(dev_path, "/dev/sd%c", 'a'+dev);
					sprintf(usb_path, "%sDisk %c", usbRoot, 'A'+dev);
				} else
				{
					sprintf(dev_path, "/dev/sd%c%d", 'a'+dev, part);
					sprintf(usb_path, "%sDisk %c Partition %d", usbRoot, 'A'+dev, part);
				}
				//dprintf("%s: check part %s\n", __FUNCTION__, dev_path);
				if (check_file_exsists(dev_path))
				{
					eprintf("media: checking '%s'\n", usb_path);
					devCount+=helperCheckDirectoryExsists(usb_path);
				}
			}
		}
	}
#else //#if !(defined STB225)
	//autofs is removed by hotplug with mdev
	// so we can only calculate number of files in /usb
	DIR *usbDir = opendir(usbRoot);
	if( usbDir != NULL )
	{
		struct dirent *item = readdir(usbDir);
		while(item)
		{
//printf("%s[%d]: %s\n", __FILE__, __LINE__, item->d_name);
			if(	(strcmp(item->d_name, ".") != 0) &&
				(strcmp(item->d_name, "..") != 0) )
			{
//printf("%s[%d]: added\n", __FILE__, __LINE__);
				devCount++;
			}
			item = readdir(usbDir);
		}
	}
	else
		eprintf("%s: Failed to open %s directory\n", __FUNCTION__, usbRoot);
//printf("%s[%d]: devCount=%d\n", __FILE__, __LINE__, devCount);
#endif //#if !(defined STB225)
	dprintf("%s: found %d devices\n", __FUNCTION__, devCount);
	interfaceSlideshowControl.enabled = ( appControlInfo.slideshowInfo.filename[0] && ((devCount > 0) | (appControlInfo.slideshowInfo.state > 0)) );
	return devCount;
}

static int media_check_storage_availability(interfaceMenu_t *pMenu, void* pArg)
{
	interface_addEvent((eventActionFunction)media_check_storage_availability, NULL, 3000, 1);

	return 0;
}

/** Check the presence of USB storages
 * @param[in] pArg Should be NULL
 */
static int media_check_storages(void* pArg)
{
	int isRoot, devCount;
	int  selectedIndex = MENU_ITEM_LAST;

	isRoot = strcmp(currentPath,usbRoot) == 0;

	devCount = media_scanStorages();

	dprintf("%s: root %d devcount %d\n", __FUNCTION__, isRoot, devCount);

	if( isRoot || devCount == 0 )
		interface_addEvent(media_check_storages, NULL, 3000, 1);

	if( devCount == 0 || interfaceInfo.currentMenu != (interfaceMenu_t*)&BrowseFilesMenu )
		return 0;

	if(!helperCheckDirectoryExsists(currentPath))
	{
		media_previousDir[0] = 0;
		strcpy(currentPath, ROOT);
	} else
	{
		selectedIndex = interface_getSelectedItem((interfaceMenu_t *)&BrowseFilesMenu);
		//dprintf("%s: selected=%d\n", __FUNCTION__,selectedIndex);
		if(selectedIndex != MENU_ITEM_MAIN && selectedIndex != MENU_ITEM_BACK)
			interface_getMenuEntryInfo((interfaceMenu_t *)&BrowseFilesMenu, selectedIndex, media_previousDir, MENU_ENTRY_INFO_LENGTH);
	}

	media_fillFileBrowserMenu((interfaceMenu_t *)&BrowseFilesMenu, selectedIndex<0 ? (void*)selectedIndex : (void*)MENU_ITEM_LAST);

	interface_displayMenu(1);

	return 0;
}

void media_storagesChanged()
{
	if(strncmp(currentPath,usbRoot,strlen(usbRoot)) != 0)
		return;
	mysem_get(media_semaphore);
	media_refreshFileBrowserMenu((interfaceMenu_t *)&BrowseFilesMenu, (void*)MENU_ITEM_LAST);
	mysem_release(media_semaphore);
	if( interfaceInfo.currentMenu == (interfaceMenu_t *)&BrowseFilesMenu )
	{
		interface_displayMenu(1);
	}
}

static int media_leaving_browse_menu(interfaceMenu_t *pMenu, void* pArg)
{
	dprintf("%s: in\n", __FUNCTION__);

	interface_removeEvent(media_check_storages, NULL);

	media_freeBrowseInfo();

	return 0;
}

static void media_freeBrowseInfo()
{
	int i;

	if(media_currentDirEntries != NULL)
	{
		for( i = 0 ; i < media_currentDirCount; ++i )
			free(media_currentDirEntries[i]);
		free(media_currentDirEntries);
		media_currentDirEntries = NULL;
		media_currentDirCount = 0;
	}
	if(media_currentFileEntries != NULL)
	{
		for( i = 0 ; i < media_currentFileCount; ++i )
			free(media_currentFileEntries[i]);
		free(media_currentFileEntries);
		media_currentFileEntries = NULL;
		media_currentFileCount = 0;
	}
}

static int media_refreshFileBrowserMenu(interfaceMenu_t *pMenu, void* pArg)
{
	int             selectedItem = (int)pArg;
	int             i, dev;
	int             isRoot;
	int             hasDrives = 0, storageCount = 0;
	char            showPath[PATH_MAX];
	char           *str;
	int             file_icon;
	FILE           *f;

	dprintf("%s: sel %d prev %s\n", __FUNCTION__, selectedItem, media_previousDir);

	interface_clearMenuEntries((interfaceMenu_t *)&BrowseFilesMenu);

	strncpy(showPath, currentPath, PATH_MAX);

	/*
	*rindex(showPath,'/')=0;
	if(strlen(showPath)>=MENU_ENTRY_INFO_LENGTH)
	{
	    dprintf("%s: showPath=%s\n", __FUNCTION__,showPath);
	}
	else
	    interface_addMenuEntryDisabled((interfaceMenu_t *)&BrowseFilesMenu, showPath, 0);
	*/
	isRoot = (strcmp(currentPath, ROOT) == 0);
	media_freeBrowseInfo();

	if(!isRoot)
	{
		interface_addMenuEntry((interfaceMenu_t *)&BrowseFilesMenu, "..", media_upFolderMenu, pArg, thumbnail_folder);
		interface_setMenuName((interfaceMenu_t *)&BrowseFilesMenu,basename(showPath),MENU_ENTRY_INFO_LENGTH);
	}

	if( media_browseType == mediaBrowseTypeUSB )
	{
		if(isRoot)
		{
			str = _T("USB_AVAILABLE");
			interface_setMenuName((interfaceMenu_t *)&BrowseFilesMenu, str, strlen(str)+1);
		}

		for (dev=0; dev < 10; dev++)
		{
			sprintf(showPath, "/dev/sr%d", dev);
			if (check_file_exsists(showPath))
			{
				sprintf(showPath, "Drive %d", dev);
				if(isRoot)
				{
 					interface_addMenuEntry((interfaceMenu_t *)&BrowseFilesMenu, showPath, media_browseFolderMenu, (void*)(-1-dev), thumbnail_folder);
				}
				hasDrives++;
			}
		}
		storageCount = media_scanStorages();
		hasDrives += storageCount;

		if ( isRoot && hasDrives == 1 && storageCount == 1)
		{
#if !(defined STB225)
			int part;
			for (dev=0; 'a'+dev<='z'; dev++)
			{
				sprintf(showPath, "/dev/sd%c", 'a'+dev);
				if (check_file_exsists(showPath))
				{
					for (part=8; part>=0; part--)
					{
						/* Now check if we have such partition on given device */
						if (part == 0)
						{
							/* Workaround for partitionless devices */
							sprintf(showPath, "/dev/sd%c", 'a'+dev);
							sprintf(currentPath, "%sDisk %c/", usbRoot, 'A'+dev);
						} else
						{
							sprintf(showPath, "/dev/sd%c%d", 'a'+dev, part);
							sprintf(currentPath, "%sDisk %c Partition %d/", usbRoot, 'A'+dev, part);
						}
						if (check_file_exsists(showPath))
						{
							//dprintf("%s: New current path='%s'\n", __FUNCTION__,currentPath);
							return media_refreshFileBrowserMenu(pMenu,(void*)MENU_ITEM_LAST);
						}
					}
				}
			}
#else
			//autofs is removed by hotplug with mdev
			// so we can only calculate number of files in /usb
			DIR *usbDir = opendir(usbRoot);
			if( usbDir != NULL )
			{
				struct dirent *item = readdir(usbDir);
				while(item)
				{
//printf("%s[%d]: %s\n", __FILE__, __LINE__, item->d_name);
					if(	(strcmp(item->d_name, ".") != 0) &&
						(strcmp(item->d_name, "..") != 0) )
					{
//printf("%s[%d]: added\n", __FILE__, __LINE__);
						sprintf(currentPath, "%s%s/", usbRoot, item->d_name);
						return media_refreshFileBrowserMenu(pMenu,(void*)MENU_ITEM_LAST);
					}
					item = readdir(usbDir);
				}
			}
			else
				eprintf("%s: Failed to open %s directory\n", __FUNCTION__, usbRoot);
#endif
		}
	} else
	{
		if( isRoot )
		{
			str = _T("NETWORK_BROWSING");
			interface_addMenuEntry((interfaceMenu_t*)&BrowseFilesMenu, str, (menuActionFunction)menuDefaultActionShowMenu,(void*)&SambaMenu, thumbnail_workstation);
			str = _T("NETWORK_PLACES");
			interface_setMenuName((interfaceMenu_t *)&BrowseFilesMenu,str, strlen(str)+1);
			interface_showLoading();
			interface_displayMenu(1);
			if( (f = fopen(SAMBA_CONFIG, "r")) != NULL )
			{
				storageCount = 0;
				while( fgets(showPath, PATH_MAX, f) != NULL )
				{
					str = index( showPath, ';' );
					if( str != NULL )
					{
						*str++ = 0;
						sprintf(str, "%s%s", sambaRoot, showPath);
						time_t start = time(NULL);
						if( helperCheckDirectoryExsists( str ) )
						{
							hasDrives++;
						} else
						{
							storageCount++;
							eprintf("%s: Samba share '%s' check failed [timeout %d]\n", __FUNCTION__, showPath, time(NULL) - start);
						}
					}
				}
				fclose(f);
			}
		} else
		{
			hasDrives = 1;
		}
	}

	if( !helperCheckDirectoryExsists(currentPath) )
	{
		strcpy( currentPath, ROOT );
		return media_refreshFileBrowserMenu(pMenu,(void*)MENU_ITEM_LAST);
	}

	if( hasDrives > 0)
	{
		/* Build directory list */
		media_currentDirCount = scandir(currentPath, &media_currentDirEntries, media_select_dir, alphasort);
		media_currentFileCount = scandir(currentPath, &media_currentFileEntries, media_select_list, alphasort);
		if( media_currentFileCount + media_currentDirCount > 100 ) // displaying lot of items takes more time then scanning
		{
			interface_showLoading();
			interface_displayMenu(1);
		}
		for( i = 0 ; i < media_currentDirCount; ++i )
		{
			if(selectedItem == MENU_ITEM_PREV && strncmp(media_currentDirEntries[i]->d_name, media_previousDir, MENU_ENTRY_INFO_LENGTH)==0)
				selectedItem = interface_getMenuEntryCount((interfaceMenu_t *)&BrowseFilesMenu);
			interface_addMenuEntry((interfaceMenu_t *)&BrowseFilesMenu, media_currentDirEntries[i]->d_name, media_browseFolderMenu, NULL, thumbnail_folder);
		}
		for( i = 0 ; i < media_currentFileCount; ++i )
		{
			//dprintf("%s: adding file: %d (%d)\n", __FUNCTION__,interface_getMenuEntryCount((interfaceMenu_t *)&BrowseFilesMenu),appControlInfo.mediaInfo.typeIndex);
			if(appControlInfo.mediaInfo.typeIndex < 0)
			{
				file_icon = (file_icon = media_getMediaType(media_currentFileEntries[i]->d_name)) >=0 ? media_formats[file_icon].icon : thumbnail_file;
			} else
				file_icon = FORMAT_ICON;

			interface_addMenuEntryCustom((interfaceMenu_t *)&BrowseFilesMenu, interfaceMenuEntryText, media_currentFileEntries[i]->d_name, strlen(media_currentFileEntries[i]->d_name)+1, 1, media_stream_change,
#ifdef ENABLE_AUTOPLAY
				media_stream_selected, media_stream_deselected,
#else
				NULL, NULL,
#endif
				NULL, (void*)interface_getMenuEntryCount((interfaceMenu_t *)&BrowseFilesMenu), file_icon);
		}
	}

	appControlInfo.mediaInfo.maxFile = media_currentFileCount;

	if(selectedItem <= MENU_ITEM_LAST)
		selectedItem = interface_getSelectedItem((interfaceMenu_t *)&BrowseFilesMenu);/*interface_getMenuEntryCount((interfaceMenu_t *)&BrowseFilesMenu) > 0 ? 0 : MENU_ITEM_MAIN;*/
	if( selectedItem >= interface_getMenuEntryCount((interfaceMenu_t *)&BrowseFilesMenu) )
	{
		selectedItem = media_currentDirCount + media_currentFileCount > 0 ? 0 : MENU_ITEM_MAIN;
	}

	switch(appControlInfo.mediaInfo.typeIndex)
	{
		case mediaVideo:
			file_icon = media_browseType == mediaBrowseTypeUSB ? thumbnail_usb_video : thumbnail_workstation_video; break;
		case mediaAudio:
			file_icon = media_browseType == mediaBrowseTypeUSB ? thumbnail_usb_audio : thumbnail_workstation_audio;
			break;
		case mediaImage:
			file_icon = media_browseType == mediaBrowseTypeUSB ? thumbnail_usb_image : thumbnail_workstation_image;
			break;
		default: file_icon = media_browseType == mediaBrowseTypeUSB ? thumbnail_usb : thumbnail_workstation;
	}

	if( hasDrives == 0 )
	{
		if( media_browseType == mediaBrowseTypeUSB )
		{
			str = _T("USB_NOTFOUND");
			file_icon = thumbnail_usb;
			interface_addEvent( media_check_storages, NULL, 3000, 1 );
		} else
		{
			str = _T("SAMBA_NO_SHARES");
			file_icon = thumbnail_network;
		}
		interface_addMenuEntryDisabled((interfaceMenu_t *)&BrowseFilesMenu, str, thumbnail_info);
	} else
	if(!isRoot && media_currentFileCount == 0 && media_currentDirCount == 0)
	{
		str = _T("NO_FILES");
		interface_addMenuEntryDisabled((interfaceMenu_t *)&BrowseFilesMenu, str, thumbnail_info);
	}
	if( media_browseType == mediaBrowseTypeSamba && isRoot && storageCount > 0 )
	{
		/* Display unavailable shares */
		if( (f = fopen(SAMBA_CONFIG, "r")) != NULL )
		{
			while( fgets(showPath, PATH_MAX, f) != NULL )
			{
				str = index( showPath, ';' );
				if( str != NULL )
				{
					*str++ = 0;
					/*sprintf(str, "%s%s", sambaRoot, showPath);
					dprintf("%s: Found samba share: '%s'\n", __FUNCTION__, str);
					if( !helperCheckDirectoryExsists( str ) ) // <- long wait here!
					{
						str = showPath;
						interface_addMenuEntry((interfaceMenu_t *)&BrowseFilesMenu, str, NULL, NULL, thumbnail_error);
					}*/
					for( i = 0; i < media_currentDirCount; i++ )
					{
						if( strcmp( showPath, media_currentDirEntries[i]->d_name ) == 0 )
							break;
					}
					if( i >= media_currentDirCount )
					{
						interface_addMenuEntry((interfaceMenu_t *)&BrowseFilesMenu, showPath, NULL, NULL, thumbnail_error);
					}
				}
			}
			fclose(f);
		}
	}

	interface_setMenuLogo((interfaceMenu_t*)&BrowseFilesMenu, file_icon, -1, 0, 0, 0);
	interface_setSelectedItem((interfaceMenu_t *)&BrowseFilesMenu, selectedItem);

	interface_hideLoading();

	return 0;
}

static int media_fillFileBrowserMenu(interfaceMenu_t *pMenu, void* pArg)
{
	mysem_get(media_semaphore);
	media_refreshFileBrowserMenu(pMenu, pArg);
	mysem_release(media_semaphore);

	return 0;
}

int media_initUSBBrowserMenu(interfaceMenu_t *pMenu, void* pArg)
{
	//appControlInfo.mediaInfo.typeIndex = (int)pArg;
	media_browseType = mediaBrowseTypeUSB;
	BrowseFilesMenu.baseMenu.statusBarIcons[0] = 0;

	if( strncmp(currentPath, usbRoot, strlen(usbRoot)) !=0 || !helperCheckDirectoryExsists(currentPath) )
		strncpy(currentPath, usbRoot, strlen(usbRoot)+1);

	dprintf("%s: %s\n", __FUNCTION__, currentPath);

	return interface_menuActionShowMenu( pMenu, &BrowseFilesMenu );
}

int media_initSambaBrowserMenu(interfaceMenu_t *pMenu, void* pArg)
{
	//appControlInfo.mediaInfo.typeIndex = (int)pArg;
	media_browseType = mediaBrowseTypeSamba;
	BrowseFilesMenu.baseMenu.statusBarIcons[0] = statusbar_f1_delete;

	if( strncmp( currentPath, sambaRoot, strlen(sambaRoot) ) != 0 || !helperCheckDirectoryExsists(currentPath) )
		strcpy( currentPath, sambaRoot );

	dprintf("%s: %s\n", __FUNCTION__,currentPath);

	return interface_menuActionShowMenu( pMenu, &BrowseFilesMenu );
}

static int media_browseFolderMenu(interfaceMenu_t *pMenu, void* pArg)
{
	size_t currentPathLength = strlen(currentPath);
	int hasParent = strcmp(currentPath,usbRoot) == 0 ? 0 : 1; /**< Samba root has virtual parent - Network Browse */
	int deviceNumber = (int)pArg;

	if( media_browseType == mediaBrowseTypeUSB && deviceNumber < 0)
	{
		// CD/DVD Drive
		char driveName[32];
		sprintf(driveName, "Drive %d", -(deviceNumber+1));
		strncpy(currentPath+currentPathLength, driveName, PATH_MAX-currentPathLength);
	} else
	{
		strncpy(currentPath+currentPathLength,
		media_currentDirEntries[ interface_getSelectedItem(pMenu) - hasParent ]->d_name, PATH_MAX-currentPathLength);
	}
	currentPathLength = strlen(currentPath);
	currentPath[currentPathLength]='/';
	currentPath[currentPathLength+1]=0;
	dprintf("%s: entering %s\n", __FUNCTION__,currentPath);

	interface_removeEvent(media_check_storages, NULL);

	if(!helperCheckDirectoryExsists(currentPath))
	{
		strcpy(currentPath,ROOT);
		if( media_browseType == mediaBrowseTypeUSB )
			media_check_storages(NULL);
	}

	media_fillFileBrowserMenu(pMenu,(void*)0);
	interface_displayMenu(1);

	return 0;
}

static int media_upFolderMenu(interfaceMenu_t *pMenu, void* pArg)
{
	*(rindex(currentPath,'/')) = 0;
	char* parentDir = (rindex(currentPath,'/')+1);
	strncpy(media_previousDir,parentDir,sizeof(media_previousDir));
	*parentDir = 0;
	dprintf("%s: media_previousDir = %s\n", __FUNCTION__,media_previousDir);
	if(!helperCheckDirectoryExsists(currentPath))
	{
		media_previousDir[0] = 0;
		strcpy(currentPath, ROOT);
	}

	dprintf("%s: browsing %s\n", __FUNCTION__,currentPath);

	if( media_browseType == mediaBrowseTypeUSB )
	{
		/* If we have browsed up folder and there is only one drive,
		 * we will automatically be brought back by media_refreshFileBrowserMenu.
		 */
		if( strcmp( currentPath, usbRoot ) == 0 )
		{
			int hasDrives = 0;
			int dev;
			char showPath[10];
			for (dev=0; dev < 10; dev++)
			{
				sprintf(showPath, "/dev/sr%d", dev);
				if (check_file_exsists(showPath))
				{
					hasDrives = 1;
					break;
				}
			}
			if( hasDrives == 0 && media_scanStorages() == 1 )
			{
				interface_menuActionShowMenu(pMenu, BrowseFilesMenu.baseMenu.pParentMenu);
				return 0;
			}
		}
	}

	media_fillFileBrowserMenu(pMenu,(void*)MENU_ITEM_PREV);
	interface_displayMenu(1);

	return 0;
}

static int  media_toggleMediaType(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.mediaInfo.typeIndex++;
	if(appControlInfo.mediaInfo.typeIndex >= mediaTypeCount)
	{
		appControlInfo.mediaInfo.typeIndex = mediaAll;
	}
	saveAppSettings();
	media_fillSettingsMenu(pMenu, pArg);
	return 0;
}

static int  media_toggleSlideshowMode(interfaceMenu_t *pMenu, void* pArg)
{
	media_slideshowSetMode(-1);

	media_fillSettingsMenu(pMenu, pArg);

	return 0;
}

static int  media_fillSettingsMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH], *str;
	int icon = thumbnail_usb;

	interface_clearMenuEntries((interfaceMenu_t *)&media_settingsMenu);

	switch( appControlInfo.mediaInfo.playbackMode )
	{
		case playback_looped:     str = _T("LOOPED");    break;
		case playback_sequential: str = _T("SEQUENTAL"); break;
		case playback_random:     str = _T("RANDOM");    break;
		default:                  str = _T("SINGLE");
	}
	sprintf(buf, "%s: %s", _T("PLAYBACK_MODE"), str);
	interface_addMenuEntry((interfaceMenu_t *)&media_settingsMenu, buf, media_setMode, NULL, thumbnail_turnaround);

	switch( appControlInfo.slideshowInfo.defaultState )
	{
		case slideshowShow:       str = _T("ON");     break;
		case slideshowRandom:     str = _T("RANDOM"); break;
		default:                  str = _T("OFF");
	}
	sprintf(buf, "%s: %s", _T("SLIDESHOW"), str);
	interface_addMenuEntry((interfaceMenu_t *)&media_settingsMenu, buf, media_toggleSlideshowMode, NULL, thumbnail_turnaround);

	switch( appControlInfo.mediaInfo.typeIndex )
	{
		case mediaVideo:
			str = _T("VIDEO");
			icon = thumbnail_usb_video;
			break;
		case mediaAudio:
			str = _T("AUDIO");
			icon = thumbnail_usb_audio;
			break;
		case mediaImage:
			str = _T("IMAGES");
			icon = thumbnail_usb_image;
			break;
		default:
			str = _T("ALL_FILES");
			icon = thumbnail_usb;
	}
	sprintf(buf, "%s: %s", _T("RECORDED_FILTER_SETUP"), str);
	interface_addMenuEntry((interfaceMenu_t *)&media_settingsMenu, buf, media_toggleMediaType, NULL, icon);

	if( pArg != NULL )
	{
		interface_menuActionShowMenu(pMenu, (void*)&media_settingsMenu);
	}
	interface_displayMenu(1);

	return 0;
}

static int media_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int selectedIndex;
	char URL[MAX_URL];
	char *str;

	switch( cmd->command )
	{
		case interfaceCommandGreen:
			media_fillSettingsMenu(pMenu,(void*)pMenu);
			return 0;
		case interfaceCommandBlue:
			media_toggleMediaType(pMenu,NULL);
			media_fillFileBrowserMenu(pMenu, (void*)MENU_ITEM_LAST);
			interface_displayMenu(1);
			return 0;
		case interfaceCommandBack:
			if( strcmp(currentPath, ROOT) != 0)
			{
				media_upFolderMenu(pMenu, pArg);
				return 0;
			} else
			return 1;
		default: ;
	}

	selectedIndex = interface_getSelectedItem((interfaceMenu_t*)&BrowseFilesMenu);

	if(selectedIndex < 0 || interface_getMenuEntryCount((interfaceMenu_t*)&BrowseFilesMenu) == 0)
		return 1;

	if ( media_browseType == mediaBrowseTypeSamba && cmd->command == interfaceCommandRed )
	{
		if( selectedIndex == 0 || strcmp(currentPath, sambaRoot ) != 0 )
			return 0;
		selectedIndex--; /* "Network browse" */;
		if( selectedIndex >= media_currentDirCount )
		{
			/* Disabled shares */
			if( interface_getMenuEntryInfo((interfaceMenu_t*)&BrowseFilesMenu, selectedIndex+1, URL, MENU_ENTRY_INFO_LENGTH) == 0)
			{
				str = URL;
			} else
				return 0;
		} else
		{
			str = media_currentDirEntries[ selectedIndex ]->d_name;
		}
		if( (selectedIndex = samba_unmountShare(str)) == 0 )
		{
			media_fillFileBrowserMenu(pMenu, (void*)MENU_ITEM_LAST );
			interface_showMessageBox(_T("SAMBA_UNMOUNTED"), thumbnail_info, 3000);
		}
		else
			eprintf("media: Failed to unmount '%s' (%d)\n", str, selectedIndex);
		return 0;
	} else if ( cmd->command == interfaceCommandYellow)
	{
		if( appControlInfo.mediaInfo.typeIndex == mediaImage )
			return 0;
		selectedIndex = selectedIndex - media_currentDirCount - 1 /* ".." */;
		if(selectedIndex < 0 || selectedIndex >= media_currentFileCount)
			return 0;
		snprintf(URL, MAX_URL, "file://%s%s", currentPath, media_currentFileEntries[ selectedIndex ]->d_name);
		eprintf("media: Add to Playlist '%s'\n",URL);
		playlist_addUrl(URL, media_currentFileEntries[ selectedIndex ]->d_name);
		return 0;
	}

	return 1;
}

static int media_settingsKeyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	switch(cmd->command)
	{
		case interfaceCommandGreen:
			media_setMode(pMenu,pArg);
			return 0;
		case interfaceCommandYellow:
			media_toggleSlideshowMode(pMenu,pArg);
			return 0;
		case interfaceCommandBlue:
			media_toggleMediaType(pMenu,pArg);
			mysem_get(media_semaphore);
			media_refreshFileBrowserMenu( pMenu, (void*)MENU_ITEM_LAST );
			mysem_release(media_semaphore);
			interface_displayMenu(1); // needed when media_refreshFileBrowserMenu toggled showLoading
			return 0;
		default: ;
	}
	return 1;
}
