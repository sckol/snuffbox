
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


Revision: $Id$
*/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <directfb.h>
#include <fcntl.h>

#include "app_info.h"
#include "debug.h"
#include "sem.h"
#include "gfx.h"
#include "interface.h"
#include "l10n.h"
#include "StbMainApp.h"
#include "menu_app.h"
#include "rtp.h"
#include "rtsp.h"
#include "media.h"
#include "off_air.h"
#include "output.h"
#include "playlist.h"
#include "voip.h"
#include "youtube.h"
#include "rutube.h"
#include "samba.h"
#include "smil.h"
#include "pvr.h"
#ifdef ENABLE_STATS
#include "stats.h"
#endif
#ifdef ENABLE_DLNA
#include "dlna.h"
#endif // #ifdef ENABLE_DLNA
#ifdef ENABLE_VIDIMAX
#include "vidimax.h"
#endif

/*******************************************************************
* LOCAL MACROS                                                     *
********************************************************************/

/*******************************************************************
* STATIC DATA                                                      *
********************************************************************/

#ifdef ENABLE_WEB_SERVICES
static interfaceListMenu_t WebServicesMenu;
#endif

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

interfaceListMenu_t interfaceMainMenu;

#ifdef ENABLE_VIDIMAX
extern interfaceVidimaxMenu_t VidimaxMenu;
#endif

/*******************************************************************
* FUNCTION IMPLEMENTATION                                          *
********************************************************************/

int menu_channelsMenuSelected(interfaceMenu_t *pMenu, void *pArg)
{
	interface_menuActionShowMenu(pMenu, (void*)pMenu->pParentMenu);

	return 0;
}

long getone()
{
	return 50;
}

int open_browser(interfaceMenu_t* pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char open_link[MENU_ENTRY_INFO_LENGTH];

	interface_showMessageBox(_T("LOADING_BROWSER"), thumbnail_internet, 0);
	interface_displayMenu(1); // fill second buffer with same frame
	
	getParam(BROWSER_CONFIG_FILE, "HomeURL", "", buf);
	if(buf[0]!=0)
		sprintf(open_link,"/usr/local/webkit/_start.sh %s",buf);
	else
		sprintf(open_link,"/usr/local/webkit/_start.sh");
	helperStartApp(open_link);

	return 0;
}

int open_browser_mw(interfaceMenu_t* pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char open_link[MENU_ENTRY_INFO_LENGTH];

	interface_showMessageBox(_T("LOADING_BROWSER"), thumbnail_internet, 0);
	interface_displayMenu(1); // fill second buffer with same frame

	getParam(BROWSER_CONFIG_FILE, "HomeURL", "", buf);
	if(buf[0]!=0)
		sprintf(open_link,"/usr/local/webkit/_start.sh %s",buf);
	else
		sprintf(open_link,"/usr/local/webkit/_start.sh");
	helperStartApp(open_link);
	return 0;
}

static int menu_confirmShutdown(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		gfx_stopVideoProviders(screenMain);
		gfx_stopVideoProviders(screenPip);
		system("halt");
		//keepCommandLoopAlive = 0;
		return 0;
	}
	return 1;
}

static int power_callback(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showConfirmationBox(_T("POWER_OFF_CONFIRM"), thumbnail_power, menu_confirmShutdown, pArg);

	return 0;
}

static int menu_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{	
#ifdef ENABLE_FAVORITES
	if (cmd->command == interfaceCommandBlue)
	{
		interface_menuActionShowMenu(pMenu, (void*)&playlistMenu);
		return 0;
	}
#endif

	return 1;
}

void menu_buildMainMenu()
{
	char *str;
	int  main_icons[4] = { 0, 0, 0,
#ifdef ENABLE_FAVORITES
#ifdef ENABLE_VIDIMAX
	0
#else
	statusbar_f4_favorites
#endif	
#else
	0
#endif
	};
	
	createListMenu(&interfaceMainMenu, NULL, thumbnail_logo, main_icons, NULL,
				   /* interfaceInfo.clientX, interfaceInfo.clientY,
				   interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuBigThumbnail,//interfaceListMenuIconThumbnail,
				   NULL, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&interfaceMainMenu, menu_keyCallback);
/*
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, 0, NULL, NULL, IMAGE_DIR "splash.png");
*/
#ifndef ENABLE_VIDIMAX
#ifdef ENABLE_DVB
	 
#ifdef HIDE_EXTRA_FUNCTIONS
		if ((appControlInfo.tunerInfo[0].status != tunerNotPresent ||
		    appControlInfo.tunerInfo[1].status != tunerNotPresent)
#endif // #ifdef HIDE_EXTRA_FUNCTIONS
	   )
	{
		offair_buildDVBTMenu((interfaceMenu_t*)&interfaceMainMenu);
		str = _T("DVB_CHANNELS");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&DVBTMenu, thumbnail_dvb);
	}
#endif // #ifdef ENABLE_DVB
#ifdef ENABLE_IPTV
	rtp_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("TV_CHANNELS");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, rtp_initStreamMenu, (void*)screenMain, thumbnail_multicast);
	
#endif // #ifdef ENABLE_IPTV
#ifdef ENABLE_PVR
	pvr_buildPvrMenu((interfaceMenu_t *) &interfaceMainMenu);
#endif
#ifdef ENABLE_VOD
	rtsp_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("MOVIES");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, rtsp_fillStreamMenu, (void*)screenMain, thumbnail_vod);
/*	if (checkStatus() == 0)
	{*/
#ifdef ENABLE_QIWI
 	billed_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
		/*if (appControlInfo.billedInfo.MWALLETPHONE[0] != 0)
		{*/
	str = _T("BILLED");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&billingLoginMenu /*(void*)&billedMenu*/, thumbnail_billed);
/*   		}
}*/
#endif

#endif // #ifdef ENABLE_VOD
#ifdef ENABLE_FAVORITES
	playlist_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("PLAYLIST");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&playlistMenu, thumbnail_favorites);
	
#endif // #ifdef ENABLE_FAVORITES

	media_buildMediaMenu((interfaceMenu_t*)&interfaceMainMenu);
#ifdef ENABLE_USB
	str = _T("RECORDED");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, media_initUSBBrowserMenu, (void*)mediaVideo, thumbnail_usb);
	
#endif // #ifdef ENABLE_USB

#ifdef ENABLE_WEB_SERVICES
	{
		str = _T("WEB_SERVICES");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&WebServicesMenu, thumbnail_internet);
		createListMenu(&WebServicesMenu, _T("WEB_SERVICES"), thumbnail_internet, NULL, (interfaceMenu_t*)&interfaceMainMenu,
					/* interfaceInfo.clientX, interfaceInfo.clientY,
					interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
					NULL, NULL, NULL);
#ifdef ENABLE_RUTUBE
		rutube_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = "RuTube";
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&RutubeCategories, thumbnail_rutube);
	
#endif // #ifdef ENABLE_RUTUBE
#ifdef ENABLE_YOUTUBE
		youtube_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = "YouTube";
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&YoutubeMenu, thumbnail_youtube);
#endif // #ifdef ENABLE_YOUTUBE

#ifdef ENABLE_SAMBA
		samba_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = _T("NETWORK_PLACES");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, media_initSambaBrowserMenu, NULL, thumbnail_network);
		str = _T("NETWORK_BROWSING");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&SambaMenu, thumbnail_workstation);
		
#endif // #ifdef ENABLE_SAMBA
#ifdef ENABLE_DLNA
		dlna_buildDLNAMenu((interfaceMenu_t*)&WebServicesMenu);
		str = _T("MEDIA_SERVERS");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, dlna_initServerBrowserMenu, NULL, thumbnail_movies);
		
#endif // #ifdef ENABLE_DLNA
#ifdef ENABLE_BROWSER		
		str = _T("INTERNET_BROWSING");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, open_browser, NULL, thumbnail_internet);
#ifndef HIDE_EXTRA_FUNCTIONS
		if (helperFileExists("/config/konqueror/konq-embedrc-mw"))
		{
			str = _T("MIDDLEWARE");
			interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, open_browser_mw, NULL, thumbnail_elecardtv);
		}
#endif // #ifndef HIDE_EXTRA_FUNCTIONS
		
#endif // #ifdef ENABLE_BROWSER
	//str = _T("RTMP");
	//interface_addMenuEntry((interfaceMenu_t *)&WebServicesMenu, str, smil_enterURL, (void*)-1, thumbnail_add_url);
	}
#endif // #ifdef ENABLE_WEB_SERVICES

#ifdef ENABLE_VOIP
	voip_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
		//if(appControlInfo.voipInfo.status == 0)
		{
			str = _T("VOIP");
			interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, voip_fillMenu, (void*)1, thumbnail_voip);
		}
	
#endif
#ifdef SHOW_CARD_MENU
	if (checkStatus() == 0)
	{
		card_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
		str = _T("CARD");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, card_fillInfoMenu, (void*)1, thumbnail_rd);
		str = _T("QUIZ");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, card_fillQuizMenu, (void*)&cardMenu, thumbnail_account);
	}
#endif // #ifdef SHOW_CARD_MENU
#ifdef SHOW_CLINIC_MENU
	if (checkStatus() == 0)
	{
		str = _T("MEDICAL_UTILITIES");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&clinicMenu, clinic_box);
		clinic_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	}
#endif // #ifdef SHOW_CLINIC_MENU
#ifdef STBxx
	{
		output_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
#ifdef ENABLE_STATS
		stats_buildMenu((interfaceMenu_t*)&OutputMenu);
#endif
		str = _T("SETTINGS");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&OutputMenu, thumbnail_configure);
	}
#endif // #ifdef STBxx
#else // NOT ENABLE_VIDIMAX
//#ifdef ENABLE_VIDIMAX	
	vidimax_buildCascadedMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("Vidimax");
	interface_addMenuEntry ((interfaceMenu_t*)&interfaceMainMenu, 
							str, 
							//(menuActionFunction)menuDefaultActionShowMenu, 
							vidimax_fillMenu,
							(void*)&VidimaxMenu, 
							thumbnail_vidimax);
	
	
	
	/////////////////////////////////////////////////////////////////////////////////
/*
#ifdef ENABLE_VOD
	rtsp_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("MOVIES");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, rtsp_fillStreamMenu, (void*)screenMain, thumbnail_vod);
	
#endif // #ifdef ENABLE_VOD
*/
#ifdef ENABLE_DVB
#ifdef HIDE_EXTRA_FUNCTIONS
		if ((appControlInfo.tunerInfo[0].status != tunerNotPresent ||
		    appControlInfo.tunerInfo[1].status != tunerNotPresent))
#endif // #ifdef HIDE_EXTRA_FUNCTIONS
	   
	{
		offair_buildDVBTMenu((interfaceMenu_t*)&interfaceMainMenu);
		str = _T("DVB_CHANNELS");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&DVBTMenu, thumbnail_dvb);
	}
#endif // #ifdef ENABLE_DVB

#ifdef ENABLE_IPTV
	rtp_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("TV_CHANNELS");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, rtp_initStreamMenu, (void*)screenMain, thumbnail_multicast);
	
#endif // #ifdef ENABLE_IPTV
#ifdef ENABLE_PVR
	pvr_buildPvrMenu((interfaceMenu_t *) &interfaceMainMenu);
#endif


// todo : insert payment 


#ifdef ENABLE_FAVORITES
	playlist_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
	str = _T("PLAYLIST");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&playlistMenu, thumbnail_favorites);
	
#endif // #ifdef ENABLE_FAVORITES

	media_buildMediaMenu((interfaceMenu_t*)&interfaceMainMenu);
#ifdef ENABLE_USB
	str = _T("RECORDED");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, media_initUSBBrowserMenu, (void*)mediaVideo, thumbnail_usb);
	
#endif // #ifdef ENABLE_USB

#ifdef ENABLE_WEB_SERVICES
	{
		str = _T("WEB_SERVICES");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&WebServicesMenu, thumbnail_internet);
		createListMenu(&WebServicesMenu, _T("WEB_SERVICES"), thumbnail_internet, NULL, (interfaceMenu_t*)&interfaceMainMenu,
					/* interfaceInfo.clientX, interfaceInfo.clientY,
					interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
					NULL, NULL, NULL);
#ifdef ENABLE_RUTUBE
		rutube_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = "RuTube";
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&RutubeCategories, thumbnail_rutube);
	
#endif // #ifdef ENABLE_RUTUBE
#ifdef ENABLE_YOUTUBE
		youtube_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = "YouTube";
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&YoutubeMenu, thumbnail_youtube);
#endif // #ifdef ENABLE_YOUTUBE

#ifdef ENABLE_BROWSER		
		str = _T("INTERNET_BROWSING");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, open_browser, NULL, thumbnail_internet);
#ifndef HIDE_EXTRA_FUNCTIONS
		if (helperFileExists("/config/konqueror/konq-embedrc-mw"))
		{
			str = _T("MIDDLEWARE");
			interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, open_browser_mw, NULL, thumbnail_elecardtv);
		}
#endif // #ifndef HIDE_EXTRA_FUNCTIONS
#endif // #ifdef ENABLE_BROWSER

#ifdef ENABLE_SAMBA
		samba_buildMenu((interfaceMenu_t*)&WebServicesMenu);
		str = _T("NETWORK_PLACES");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, media_initSambaBrowserMenu, NULL, thumbnail_network);
		str = _T("NETWORK_BROWSING");
		interface_addMenuEntry((interfaceMenu_t*)&WebServicesMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&SambaMenu, thumbnail_workstation);
		
#endif // #ifdef ENABLE_SAMBA
	}
#endif // #ifdef ENABLE_WEB_SERVICES

#ifdef ENABLE_VOIP
	voip_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
		//if(appControlInfo.voipInfo.status == 0)
		{
			str = _T("VOIP");
			interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, voip_fillMenu, (void*)1, thumbnail_voip);
		}
#endif
#ifdef STBxx
	{
		output_buildMenu((interfaceMenu_t*)&interfaceMainMenu);
#ifdef ENABLE_STATS
		stats_buildMenu((interfaceMenu_t*)&OutputMenu);
#endif
		str = _T("SETTINGS");
		interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&OutputMenu, thumbnail_configure);
	}
#endif // #ifdef STBxx

#endif // ENABLE_VIDIMAX
	/*
	str = _T("MUSIC");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, NULL, IMAGE_DIR "thumbnail_music.png");
	str = _T("PAUSED_CONTENT");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, NULL, IMAGE_DIR "thumbnail_paused.png");
	str = _T("ACCOUNT");
	interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, (menuActionFunction)menuDefaultActionShowMenu, NULL, IMAGE_DIR "thumbnail_account.png");
	*/
	switch( interface_getMenuEntryCount( (interfaceMenu_t*)&interfaceMainMenu ))
	{
		case 4:case 6:case 9: break;
		default:
			str = _T("SHUTDOWN");
			interface_addMenuEntry((interfaceMenu_t*)&interfaceMainMenu, str, power_callback, NULL, thumbnail_power);
	}
}

void menu_init()
{
	//interface_showSplash(interfaceInfo.clientX, interfaceInfo.clientY, interfaceInfo.clientWidth, interfaceInfo.clientHeight, 0, 0);
	interface_showSplash(0, 0, interfaceInfo.screenWidth, interfaceInfo.screenHeight, 0, 0);

	menu_buildMainMenu();

	interfaceInfo.currentMenu = (interfaceMenu_t*)&interfaceMainMenu;
#ifdef ENABLE_PROVIDER_PROFILES
	output_checkProfile();
#endif

#ifndef DEBUG
	if(appControlInfo.playbackInfo.bResumeAfterStart == 0 || appControlInfo.playbackInfo.streamSource == streamSourceNone )
#endif
	{
		interface_showMenu(1, 0);
#ifndef ENABLE_VIDIMAX		
		interface_showSplash(interfaceInfo.clientX, interfaceInfo.clientY, interfaceInfo.clientWidth, interfaceInfo.clientHeight, 1, 1);
		//interface_showSplash(0, 0, interfaceInfo.screenWidth, interfaceInfo.screenHeight, 1, 1);
#else
		interface_showSplash(0, 0, interfaceInfo.screenWidth, interfaceInfo.screenHeight, 1, 1);
#endif		
	}

	{//release splash, splash entry no need more
		stb810_gfxImageEntry *pSplashEntry = gfx_findImageEntryByName( IMAGE_DIR INTERFACE_SPLASH_IMAGE );
		gfx_releaseImageEntry( pSplashEntry );
	}
}

void menu_cleanup()
{
	rtp_cleanupMenu();
	rtsp_cleanupMenu();
	rutube_cleanupMenu();
	/*if (checkStatus() == 0)
	{*/
	//}
	media_cleanupMenu();
#ifdef ENABLE_DVB
	offair_cleanupMenu();
#endif
}

