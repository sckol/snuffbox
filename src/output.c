
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

//#ifdef STBxx

/***********************************************
* INCLUDE FILES                                *
************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>

#include <directfb.h>
#include <directfb_strings.h>
#include "app_info.h"
#include "interface.h"
#include "l10n.h"
#include "StbMainApp.h"
#include "sem.h"
#include "gfx.h"
#include "backend.h"
#include "menu_app.h"
#include "output.h"
#include "sound.h"
#ifdef ENABLE_STATS
#include "stats.h"
#endif

#include <platform.h>
#include <sdp.h>
#include <service.h>

#include "dvb.h"
#include "off_air.h"
#include "rtp.h"
#include "pvr.h"
#include "debug.h"
#include "md5.h"
#include "voip.h"
#include "messages.h"

#define _FILE_OFFSET_BITS 64 /* for curl_off_t magic */
#include <curl/curl.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define DATA_BUFF_SIZE              (32*1024)

#define MAX_CONFIG_PATH             (64)

#define COLOR_STEP_COUNT            (64)
#define COLOR_STEP_SIZE             (0x10000/COLOR_STEP_COUNT)

#define TEMP_CONFIG_FILE            "/var/tmp/cfg.tmp"

#define OUTPUT_INFO_SET(type,index) (((int)type << 16) | (index))
#define OUTPUT_INFO_GET_TYPE(info)  ((int)info >> 16)
#define OUTPUT_INFO_GET_INDEX(info) ((int)info & 0xFFFF)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

typedef enum
{
	gatewayModeOff = 0,
	gatewayModeBridge,
	gatewayModeNAT,
	gatewayModeFull,
	gatewayModeCount
} gatewayMode_t;

typedef enum
{
	optionRtpEpg,
	optionRtpPlaylist,
	optionVodPlaylist
} outputUrlOption;

typedef enum
{
	optionIP   = 0x01,
	optionGW   = 0x02,
	optionMask = 0x04,
	optionDNS  = 0x08,
	optionMode = 0x10
} outputIPOption;

#ifdef ENABLE_PPP
typedef struct
{
	char login[MENU_ENTRY_INFO_LENGTH];
	char password[MENU_ENTRY_INFO_LENGTH];
	pthread_t check_thread;
} pppInfo_t;
#endif

#ifdef ENABLE_WIFI
typedef enum
{
	wifiModeManaged = 0,
	wifiModeAdHoc,
	wifiModeCount, // Master mode unsupported yet
	wifiModeMaster,
} outputWifiMode_t;

typedef enum
{
	wifiAuthOpen = 0,
	wifiAuthWEP,
	wifiAuthWPAPSK,
	wifiAuthWPA2PSK,
	wifiAuthCount,
} outputWifiAuth_t;

typedef enum
{
	wifiEncTKIP = 0,
	wifiEncAES,
	wifiEncCount
} outputWifiEncryption_t;

typedef struct
{
	int wanMode;
	int dhcp;
	int channelCount;
	int currentChannel;
	outputWifiMode_t       mode;
	outputWifiAuth_t       auth;
	outputWifiEncryption_t encryption;
	char key[64+1];
} outputWifiInfo_t;
#endif

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static void output_fillOutputMenu(void);
static int output_fillNetworkMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_fillVideoMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_fillTimeMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_fillInterfaceMenu(interfaceMenu_t *pMenu, void* pArg);

static int output_fillWANMenu (interfaceMenu_t *pMenu, void* pArg);
#ifdef ENABLE_PPP
static int output_fillPPPMenu (interfaceMenu_t *pMenu, void* pArg);
static int output_leavePPPMenu (interfaceMenu_t *pMenu, void* pArg);
#endif
#ifdef ENABLE_LAN
static int output_fillLANMenu (interfaceMenu_t *pMenu, void* pArg);
static int output_fillGatewayMenu(interfaceMenu_t *pMenu, void* pArg);
#endif
#ifdef ENABLE_WIFI
static int output_fillWifiMenu (interfaceMenu_t *pMenu, void* pArg);
#endif
#ifdef ENABLE_IPTV
static int output_fillIPTVMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_toggleIPTVPlaylist(interfaceMenu_t *pMenu, void* pArg);

#ifdef ENABLE_PROVIDER_PROFILES
static int output_enterProfileMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_leaveProfileMenu(interfaceMenu_t *pMenu, void* pArg);
static int output_setProfile(interfaceMenu_t *pMenu, void* pArg);
#endif

#endif // ENABLE_IPTV
#ifdef ENABLE_VOD
static int output_fillVODMenu (interfaceMenu_t *pMenu, void* pArg);
static int output_toggleVODPlaylist(interfaceMenu_t *pMenu, void* pArg);
#endif
static int output_fillWebMenu (interfaceMenu_t *pMenu, void* pArg);

#ifdef STB82
static int output_toggleInterfaceAnimation(interfaceMenu_t* pMenu, void* pArg);
#endif
static int output_toggleResumeAfterStart(interfaceMenu_t *pMenu, void* pArg);
static int output_toggleAutoPlay(interfaceMenu_t *pMenu, void* pArg);
static int output_toggleHighlightColor(interfaceMenu_t* pMenu, void* pArg);
static int output_togglePlayControlTimeout(interfaceMenu_t* pMenu, void* pArg);
static int output_togglePlayControlShowOnStart(interfaceMenu_t* pMenu, void* pArg);
#ifdef ENABLE_VOIP
static int output_toggleVoipIndication(interfaceMenu_t* pMenu, void* pArg);
static int output_toggleVoipBuzzer(interfaceMenu_t* pMenu, void* pArg);
#endif
#ifdef ENABLE_DVB
static int output_toggleDvbShowScrambled(interfaceMenu_t *pMenu, void* pArg);
static int output_toggleDvbBandwidth(interfaceMenu_t *pMenu, void* pArg);
static int output_clearDvbSettings(interfaceMenu_t *pMenu, void* pArg);
static int output_clearOffairSettings(interfaceMenu_t *pMenu, void* pArg);
static int output_confirmClearDvb(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
static int output_confirmClearOffair(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
#endif
#ifdef ENABLE_LAN
static int output_confirmGatewayMode(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
static int output_toggleGatewayMode(interfaceMenu_t *pMenu, void* pArg);
#endif

// 3D
#ifdef STB225
static interfaceListMenu_t Video3DSubMenu;
static int output_fill3DMenu(interfaceMenu_t *pMenu, void* pArg);
#endif // #ifdef STB225

static void output_colorSliderUpdate(void *pArg);

static char* output_getOption(outputUrlOption option);
static char* output_getURL(int index, void* pArg);
static int output_changeURL(interfaceMenu_t *pMenu, char *value, void* pArg);
static int output_toggleURL(interfaceMenu_t *pMenu, void* pArg);

#ifdef ENABLE_PASSWORD
static int output_askPassword(interfaceMenu_t *pMenu, void* pArg);
static int output_enterPassword(interfaceMenu_t *pMenu, char *value, void* pArg);
#endif
static void output_fillStandardMenu(void);
static void output_fillFormatMenu(void);
static void output_fillBlankingMenu(void);

static void output_fillTimeZoneMenu(void);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static interfaceListMenu_t StandardMenu;
static interfaceListMenu_t FormatMenu;
static interfaceListMenu_t BlankingMenu;
static interfaceListMenu_t TimeZoneMenu;
static interfaceListMenu_t InterfaceMenu;

static interfaceListMenu_t VideoSubMenu;
static interfaceListMenu_t TimeSubMenu;
static interfaceListMenu_t NetworkSubMenu;

static interfaceListMenu_t WANSubMenu;
#ifdef ENABLE_PPP
static interfaceListMenu_t PPPSubMenu;
#endif
#ifdef ENABLE_LAN
static interfaceListMenu_t LANSubMenu;
static interfaceListMenu_t GatewaySubMenu;
#endif
#ifdef ENABLE_WIFI
static interfaceListMenu_t WifiSubMenu;
#endif
#ifdef ENABLE_IPTV
static interfaceListMenu_t IPTVSubMenu;

#ifdef ENABLE_PROVIDER_PROFILES
#define MAX_PROFILE_PATH 64
static struct dirent **output_profiles = NULL;
static int             output_profiles_count = 0;
static char            output_profile[MAX_PROFILE_PATH] = {0};
static interfaceListMenu_t ProfileMenu;
#endif

#endif // ENABLE_IPTV
#ifdef ENABLE_VOD
static interfaceListMenu_t VODSubMenu;
#endif
static interfaceListMenu_t WebSubMenu;

#ifdef ENABLE_WIFI
static outputWifiInfo_t wifiInfo;
#endif

static interfaceEditEntry_t TimeEntry;
static interfaceEditEntry_t DateEntry;

#ifdef ENABLE_LAN
static gatewayMode_t output_gatewayMode = gatewayModeOff;
#endif

static int bDisplayedWarning = 0;

static long info_progress;

static char output_ip[4*4];

#ifdef ENABLE_PPP
static pppInfo_t pppInfo;
#endif

/**
 * @brief Useful DFB macros to have Strings and Values in an array.
 */
static const DirectFBScreenEncoderTVStandardsNames(tv_standards);
static const DirectFBScreenOutputSignalsNames(signals);
static const DirectFBScreenOutputResolutionNames(resolutions);

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceListMenu_t OutputMenu;
#ifdef ENABLE_DVB
interfaceListMenu_t DVBSubMenu;
#endif

stbTimeZoneDesc_t timezones[] = {
	{"Russia/Kaliningrad", "(MSK-1) Калининградское время"},
	{"Russia/Moscow", "(MSK) Московское время"},
	{"Russia/Yekaterinburg", "(MSK+2) Екатеринбургское время"},
	{"Russia/Omsk", "(MSK+3) Омское время"},
	{"Russia/Krasnoyarsk", "(MSK+4) Красноярское время"},
	{"Russia/Irkutsk", "(MSK+5) Иркутское время"},
	{"Russia/Yakutsk", "(MSK+6) Якутское время"},
	{"Russia/Vladivostok", "(MSK+7) Владивостокское время"},
	{"Russia/Magadan", "(MSK+8) Магаданское время"},
	{"Etc/GMT+12", "(GMT -12:00) Eniwetok, Kwajalein"},
	{"Etc/GMT+11", "(GMT -11:00) Midway Island, Samoa"},
	{"Etc/GMT+10", "(GMT -10:00) Hawaii"},
	{"Etc/GMT+9", "(GMT -9:00) Alaska"},
	{"Etc/GMT+8", "(GMT -8:00) Pacific Time (US &amp; Canada)"},
	{"Etc/GMT+7", "(GMT -7:00) Mountain Time (US &amp; Canada)"},
	{"Etc/GMT+6", "(GMT -6:00) Central Time (US &amp; Canada), Mexico City"},
	{"Etc/GMT+5", "(GMT -5:00) Eastern Time (US &amp; Canada), Bogota, Lima"},
	{"Etc/GMT+4", "(GMT -4:00) Atlantic Time (Canada), La Paz, Santiago"},
	{"Etc/GMT+3", "(GMT -3:00) Brazil, Buenos Aires, Georgetown"},
	{"Etc/GMT+2", "(GMT -2:00) Mid-Atlantic"},
	{"Etc/GMT+1", "(GMT -1:00) Azores, Cape Verde Islands"},
	{"Etc/GMT-0", "(GMT) Western Europe Time, London, Lisbon, Casablanca"},
	{"Etc/GMT-1", "(GMT +1:00) Brussels, Copenhagen, Madrid, Paris"},
	{"Etc/GMT-2", "(GMT +2:00) Kaliningrad, South Africa"},
	{"Etc/GMT-3", "(GMT +3:00) Baghdad, Riyadh, Moscow, St. Petersburg"},
	{"Etc/GMT-4", "(GMT +4:00) Abu Dhabi, Muscat, Baku, Tbilisi"},
	{"Etc/GMT-5", "(GMT +5:00) Ekaterinburg, Islamabad, Karachi, Tashkent"},
	{"Etc/GMT-6", "(GMT +6:00) Almaty, Dhaka, Colombo"},
	{"Etc/GMT-7", "(GMT +7:00) Bangkok, Hanoi, Krasnoyarsk"},
	{"Etc/GMT-8", "(GMT +8:00) Beijing, Perth, Singapore, Hong Kong"},
	{"Etc/GMT-9", "(GMT +9:00) Tokyo, Seoul, Osaka, Sapporo, Yakutsk"},
	{"Etc/GMT-10", "(GMT +10:00) Eastern Australia, Guam, Vladivostok"},
	{"Etc/GMT-11", "(GMT +11:00) Magadan, Solomon Islands, New Caledonia"},
	{"Etc/GMT-12", "(GMT +12:00) Auckland, Wellington, Fiji, Kamchatka"}
};

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

char* inet_addr_prepare( char *value)
{
	char *str1, *str2;
	str1 = value;
	while( str1[0] == '0' && !(str1[1] == 0 || str1[1] == '.'))
	{
		str2 = str1;
		while( (*str2++ = str2[1]) );
	}
	for( str1 = value; *str1; str1++)
	{
		while( str1[0] == '.' && str1[1] == '0' && !(str1[2] == 0 || str1[2] == '.'))
		{
			str2 = &str1[1];
			while( (*str2++ = str2[1]) );
		}
	}
	return value;
}

/* -------------------------- OUTPUT SETTING --------------------------- */

static int output_setStandard(interfaceMenu_t *pMenu, void* pArg)
{
#ifdef STB6x8x
    DFBScreenEncoderTVStandards tv_standard;
    tv_standard = (int)pArg;

    /* Check to see if the standard actually needs changing */
    /*if (tv_standard==appControlInfo.outputInfo.encConfig[0].tv_standard)
    {
        return 0;
    }*/

    appControlInfo.outputInfo.encConfig[0].tv_standard = tv_standard;
    appControlInfo.outputInfo.encConfig[0].flags = DSECONF_TV_STANDARD;

    /* What if we have 2 encoders like PNX8510 + TDA9982 */
    if(appControlInfo.outputInfo.numberOfEncoders == 2)
    {
        appControlInfo.outputInfo.encConfig[1].tv_standard = tv_standard;
        appControlInfo.outputInfo.encConfig[1].flags = DSECONF_TV_STANDARD;
    }

    switch(tv_standard)
    {
        case DSETV_NTSC:
#ifdef STBPNX
        case DSETV_NTSC_M_JPN:
#endif
			interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, 0);
            //menuInfra_setSelected(&StandardMenu, 0);
			system("/config.templates/scripts/dispmode ntsc");
            break;

        case DSETV_SECAM:
			interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, 1);
            //menuInfra_setSelected(&StandardMenu, 1);
			system("/config.templates/scripts/dispmode secam");
            break;

        case DSETV_PAL:
#ifdef STBPNX
        case DSETV_PAL_BG:
        case DSETV_PAL_I:
        case DSETV_PAL_N:
        case DSETV_PAL_NC:
#endif
			interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, 2);
            //menuInfra_setSelected(&StandardMenu, 2);
			system("/config.templates/scripts/dispmode pal");
            break;

#ifdef STB82
#if RELEASE == 7
        case DSETV_PAL_60:
#else
		case DSETV_PAL60:
#endif
#endif //#ifdef STB82
#ifdef STBPNX
        case DSETV_PAL_M:
			interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, 3);
            //menuInfra_setSelected(&StandardMenu, 3);
			system("/config.templates/scripts/dispmode pal60");
            break;
        case DSETV_DIGITAL:
            appControlInfo.outputInfo.encConfig[0].out_signals = DSOS_YCBCR;
            appControlInfo.outputInfo.encConfig[0].out_connectors = DSOC_COMPONENT;
            appControlInfo.outputInfo.encConfig[0].flags = (DSECONF_OUT_SIGNALS | DSECONF_CONNECTORS );
			interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, 0);
            //menuInfra_setSelected(&StandardMenu, 0);
            break;
#endif
        default:
            break;
    }

    //gfx_setOutputFormat(1);

	appControlInfo.outputInfo.standart = tv_standard;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
#else //#ifdef STB6x8x
//	system("mount -o rw,remount /");
//	setParam("/etc/init.d/S35pnxcore.sh", "resolution", (int)pArg == 720 ? "1280x720x60p" : "1920x1080x60i");
//	system("mount -o ro,remount /");
#endif //#ifdef STB6x8x

	system("sync");
	system("reboot");

    output_fillStandardMenu();
    //menuInfra_display((void*)&StandardMenu);
	interface_displayMenu(1);

#ifdef ENABLE_DVB
    /* Re-start DVB - if possible */
    if (( appControlInfo.tunerInfo[0].status != tunerNotPresent ||
          appControlInfo.tunerInfo[1].status != tunerNotPresent ) &&
        dvb_getNumberOfServices())
    {
        offair_startVideo(screenMain);
    }
#endif

    return 0;
}

/**
 * This function now uses the Encoder API to set the output format instead of the Output API.
 */
static int output_setFormat(interfaceMenu_t *pMenu, void* pArg)
{
	int format;

	format = (int)pArg;

	gfx_changeOutputFormat(format);

	appControlInfo.outputInfo.format = format;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

    /*switch(format)
    {
        case(DSOS_YCBCR) :
			interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, 3);
            //menuInfra_setSelected(&FormatMenu, 0);
            break;
        case(DSOS_YC) :
			interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, 0);
            //menuInfra_setSelected(&FormatMenu, 0);
            break;
        case(DSOS_CVBS) :
			interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, 1);
            //menuInfra_setSelected(&FormatMenu, 1);
            break;
        case(DSOS_RGB) :
			interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, 2);
            //menuInfra_setSelected(&FormatMenu, 2);
            break;
        default:
            break;
    }*/

    output_fillFormatMenu();
    //menuInfra_display((void*)&FormatMenu);
	interface_displayMenu(1);

    return 0;
}

/**
 * This function now uses the Encoder API to set the slow blanking instead of the Output API.
 */
static int output_setBlanking(interfaceMenu_t *pMenu, void* pArg)
{
    int blanking;

    blanking = (int)pArg;

    appControlInfo.outputInfo.encConfig[0].slow_blanking = blanking;
    appControlInfo.outputInfo.encConfig[0].flags = DSECONF_SLOW_BLANKING;

    switch(blanking)
    {
        case(DSOSB_4x3) :
			interface_setSelectedItem((interfaceMenu_t*)&BlankingMenu, 0);
            //menuInfra_setSelected(&BlankingMenu, 0);
            break;
        case(DSOSB_16x9) :
			interface_setSelectedItem((interfaceMenu_t*)&BlankingMenu, 1);
            //menuInfra_setSelected(&BlankingMenu, 1);
            break;
        case(DSOSB_OFF) :
			interface_setSelectedItem((interfaceMenu_t*)&BlankingMenu, 2);
            //menuInfra_setSelected(&BlankingMenu, 2);
            break;
        default:
            break;
    }

    gfx_setOutputFormat(0);
    //output_fillBlankingMenu();
    //menuInfra_display((void*)&BlankingMenu);
	interface_displayMenu(1);

    return 0;
}

#ifndef HIDE_EXTRA_FUNCTIONS
static int output_toggleAudio(interfaceMenu_t *pMenu, void* pArg)
{
	if (appControlInfo.soundInfo.rcaOutput == 1)
	{
		appControlInfo.soundInfo.rcaOutput = 0;
	} else
	{
		appControlInfo.soundInfo.rcaOutput = 1;
	}

	/* Just force rcaOutput for now */
	appControlInfo.soundInfo.rcaOutput = 1;

	sound_restart();
	output_fillVideoMenu();

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

static int output_togglePCR(interfaceMenu_t *pMenu, void* pArg)
{
	if (appControlInfo.bProcessPCR == 1)
	{
		appControlInfo.bProcessPCR = 0;
	} else
	{
		appControlInfo.bProcessPCR = 1;
	}

	output_fillVideoMenu(pMenu, pArg);

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	interface_displayMenu(1);

	return 0;
}

static int output_toggleRSync(interfaceMenu_t *pMenu, void* pArg)
{
	if (appControlInfo.bRendererDisableSync == 1)
	{
		appControlInfo.bRendererDisableSync = 0;
	} else
	{
		appControlInfo.bRendererDisableSync = 1;
	}

	output_fillVideoMenu(pMenu, pArg);

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	interface_displayMenu(1);

	return 0;
}


static int output_toggleBufferTracking(interfaceMenu_t *pMenu, void* pArg)
{
	if (appControlInfo.bUseBufferModel == 1)
	{
		appControlInfo.bUseBufferModel = 0;
	} else
	{
		appControlInfo.bUseBufferModel = 1;
	}

	output_fillVideoMenu(pMenu, pArg);

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	interface_displayMenu(1);

	return 0;
}
#endif

#ifdef STB82
static int output_toggleInterfaceAnimation(interfaceMenu_t *pMenu, void* pArg)
{
	interfaceInfo.animation = (interfaceInfo.animation + 1) % interfaceAnimationCount;
	if( interfaceInfo.animation > 0 )
	{
		interfaceInfo.currentMenu = (interfaceMenu_t*)&interfaceMainMenu; // toggles animation
		interface_displayMenu(1);
	}

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}
#endif

static int output_toggleHighlightColor(interfaceMenu_t *pMenu, void* pArg)
{
	interfaceInfo.highlightColor = (interfaceInfo.highlightColor + 1) % PREDEFINED_COLORS;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

static int output_toggleResumeAfterStart(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.playbackInfo.bResumeAfterStart = (appControlInfo.playbackInfo.bResumeAfterStart + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

static int output_toggleAutoPlay(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.playbackInfo.bAutoPlay = (appControlInfo.playbackInfo.bAutoPlay + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

static int output_togglePlayControlTimeout(interfaceMenu_t *pMenu, void* pArg)
{
	interfacePlayControl.showTimeout = interfacePlayControl.showTimeout % PLAYCONTROL_TIMEOUT_MAX + 1;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

static int output_togglePlayControlShowOnStart(interfaceMenu_t *pMenu, void* pArg)
{
	interfacePlayControl.showOnStart = !interfacePlayControl.showOnStart;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

#ifdef ENABLE_VOIP
static int output_toggleVoipIndication(interfaceMenu_t *pMenu, void* pArg)
{
	interfaceInfo.enableVoipIndication = (interfaceInfo.enableVoipIndication + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}

static int output_toggleVoipBuzzer(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.voipInfo.buzzer = (appControlInfo.voipInfo.buzzer + 1) % 2;
	voip_setBuzzer();
	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	return output_fillInterfaceMenu(pMenu, pArg);
}
#endif

int getParam(const char *path, const char *param, const char *defaultValue, char *output)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	FILE *fd;
	int found = 0;
	int plen, vlen;

	fd = fopen(path, "r");
	if (fd != NULL)
	{
		while (fgets(buf, MENU_ENTRY_INFO_LENGTH, fd) != NULL)
		{
			plen = strlen(param);
			vlen = strlen(buf)-1;
			if (strncmp(buf, param, plen) == 0 && buf[plen] == '=')
			{
				while (buf[vlen] == '\r' || buf[vlen] == '\n' || buf[vlen] == ' ')
				{
					buf[vlen] = 0;
					vlen--;
				}
				if (vlen-plen > 0)
				{
					if (output != NULL)
					{
						strcpy(output, &buf[plen+1]);
					}
					found = 1;
				}
				break;
			}
		}
		fclose(fd);
	}

	if (!found && defaultValue != NULL && output != NULL)
	{
		strcpy(output, defaultValue);
	}
	return found;
}

int setParam(const char *path, const char *param, const char *value)
{
	FILE *fdi, *fdo;
	char buf[MENU_ENTRY_INFO_LENGTH];
	int found = 0;

	//dprintf("%s: %s: %s -> %s\n", __FUNCTION__, path, param, value);

#ifdef STB225
//	system("mount -o rw,remount /");
#endif

	fdo = fopen(TEMP_CONFIG_FILE, "w");

	if (fdo == NULL)
	{
		eprintf("output: Failed to open out file '%s'\n", TEMP_CONFIG_FILE);
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
#ifdef STB225
//		system("mount -o ro,remount /");
#endif
		return -1;
	}

	fdi = fopen(path, "r");

	if (fdi != NULL)
	{
		while (fgets(buf, MENU_ENTRY_INFO_LENGTH, fdi) != NULL)
		{
			//dprintf("%s: line %s\n", __FUNCTION__, buf);
			if (strncasecmp(param, buf, strlen(param)) == 0 ||
				(buf[0] == '#' && strncasecmp(param, &buf[1], strlen(param)) == 0))
			{
				//dprintf("%s: line matched param %s\n", __FUNCTION__, param);
				if (value != NULL)
				{
					fprintf(fdo, "%s=%s\n", param, value);
				} else
				{
					fprintf(fdo, "#%s=\n", param);
				}
				found = 1;
			} else
			{
				fwrite(buf, strlen(buf), 1, fdo);
			}
		}
		fclose(fdi);
	}

	if (found == 0)
	{
		if (value != NULL)
		{
			fprintf(fdo, "%s=%s\n", param, value);
		} else
		{
			fprintf(fdo, "#%s=\n", param);
		}
	}

	fclose(fdo);

	//dprintf("%s: replace!\n", __FUNCTION__);
	sprintf(buf, "mv -f '%s' '%s'", TEMP_CONFIG_FILE, path);
	system(buf);

#ifdef STB225
//	system("mount -o ro,remount /");
#endif

	return 0;
}

#ifdef ENABLE_WIFI
static int output_changeESSID(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	int i = OUTPUT_INFO_GET_INDEX(pArg);

	if( value == NULL || value[0] == 0 )
		return 1;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	if( setParam(path, "ESSID", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

char* output_getESSID(int index, void* pArg)
{
	if( index == 0 )
	{
		char path[MAX_CONFIG_PATH];
		static char essid[MENU_ENTRY_INFO_LENGTH];
		int i = OUTPUT_INFO_GET_INDEX(pArg);

		sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

		getParam(path, "ESSID", "itelec", essid);

		return essid;
	} else
		return NULL;
}

static int output_toggleESSID(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_ESSID"), "\\w+", output_changeESSID, output_getESSID, inputModeABC, pArg );

	return 0;
}

static int output_changeWifiChannel(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	int i = OUTPUT_INFO_GET_INDEX(pArg);

	if( value == NULL || value[0] == 0 )
		return 1;

	wifiInfo.currentChannel = strtol( value, NULL, 10 );
	if( wifiInfo.currentChannel < 1 || wifiInfo.currentChannel > wifiInfo.channelCount )
		return 0;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	if( setParam(path, "CHANNEL", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

char* output_getWifiChannel(int index, void* pArg)
{
	if( index == 0 )
	{
		static char temp[8];
		sprintf(temp, "%02d", wifiInfo.currentChannel);
		return temp;
	} else
		return NULL;
}

static int output_toggleWifiChannel(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_WIFI_CHANNEL"), "\\d{2}", output_changeWifiChannel, output_getWifiChannel, inputModeDirect, pArg );

	return 0;
}

static int output_toggleAuthMode(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	int i = OUTPUT_INFO_GET_INDEX(pArg);
	char *value;
	outputWifiAuth_t maxAuth = wifiInfo.mode == wifiModeAdHoc ? wifiAuthWEP+1 : wifiAuthCount;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	wifiInfo.auth = (wifiInfo.auth+1)%maxAuth;
	switch(wifiInfo.auth)
	{
		case wifiAuthOpen:   value = "OPEN";
			setParam(path, "ENCRYPTION", "NONE");
			break;
		case wifiAuthWEP:    value = "SHARED";
			setParam(path, "ENCRYPTION", "WEP");
			break;
		case wifiAuthWPAPSK: value = "WPAPSK";
			setParam(path, "ENCRYPTION", "TKIP");
			break;
		case wifiAuthWPA2PSK:value = "WPA2PSK";
			setParam(path, "ENCRYPTION", "TKIP");
			break;
		default:
			return 1;
	}

	if( setParam(path, "AUTH", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleWifiEncryption(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	int i = OUTPUT_INFO_GET_INDEX(pArg);
	char *value;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	wifiInfo.encryption = (wifiInfo.encryption+1)%wifiEncCount;
	switch(wifiInfo.encryption)
	{
		case wifiEncAES:  value = "AES";  break;
		case wifiEncTKIP: value = "TKIP"; break;
		default: return 1;
	}

	if( setParam(path, "ENCRYPTION", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_changeWifiKey(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	int i = OUTPUT_INFO_GET_INDEX(pArg);

	if( value == NULL || value[0] == 0 )
		return 1;

	if( wifiInfo.auth == wifiAuthWEP )
	{
		if( strlen(value) != 10 )
		{
			interface_showMessageBox(_T("WIRELESS_PASSWORD_INCORRECT"), thumbnail_error, 0);
			return 1;
		} else
		{
			int j;
			for( j = 0; j < 10; j++ )
				if( value[j] < '0' || value[j] > '9' )
				{
					interface_showMessageBox(_T("WIRELESS_PASSWORD_INCORRECT"), thumbnail_error, 0);
					return 1;
				}
		}
	} else if( strlen(value) < 8 )
	{
		interface_showMessageBox(_T("WIRELESS_PASSWORD_TOO_SHORT"), thumbnail_error, 0);
		return 1;
	}

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	if( setParam(path, "KEY", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

char* output_getWifiKey(int index, void* pArg)
{
	return index == 0 ? wifiInfo.key : NULL;
}

static int output_toggleWifiKey(interfaceMenu_t *pMenu, void* pArg)
{
	if( wifiInfo.auth == wifiAuthWEP )
		interface_getText(pMenu, _T("ENTER_PASSWORD"), "\\d{10}", output_changeWifiKey, output_getWifiKey, inputModeDirect, pArg );
	else
		interface_getText(pMenu, _T("ENTER_PASSWORD"), "\\w+", output_changeWifiKey, output_getWifiKey, inputModeABC, pArg );

	return 0;
}

static int output_toggleWifiWAN(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char value[16];
	int i = OUTPUT_INFO_GET_INDEX(pArg);

	wifiInfo.wanMode = (wifiInfo.wanMode+1)%2;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));
	sprintf(value,"%d",wifiInfo.wanMode);

	if( !wifiInfo.wanMode )
	{
		setParam(path, "BOOTPROTO",      "static");
		setParam(path, "MODE",           "ad-hoc");
		if( wifiInfo.auth > wifiAuthWEP )
		{
			setParam(path, "AUTH",       "SHARED");
			setParam(path, "ENCRYPTION", "WEP");
			setParam(path, "KEY",        "0102030405");
		}
	}
	if( setParam(path, "WAN_MODE", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleWifiMode(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char *value = NULL;
	int i = OUTPUT_INFO_GET_INDEX(pArg);

	wifiInfo.mode = (wifiInfo.mode+1)%wifiModeCount;
	switch(wifiInfo.mode)
	{
		case wifiModeCount:
		case wifiModeAdHoc:   value = "ad-hoc"; break;
		case wifiModeMaster:  value = "master"; break;
		case wifiModeManaged: value = "managed"; break;
	}

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	if( setParam(path, "MODE", value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillWifiMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}
#endif

static void output_parseIP(char *value)
{
	int i;
	int ip_index, j;
	ip_index = 0;
	j = 0;
	memset( &output_ip, 0, sizeof(output_ip) );
	for( i = 0; ip_index < 4 && j < 4 && value[i]; i++ )
	{
		if( value[i] >= '0' && value[i] <= '9' )
			output_ip[(ip_index*4) + j++] = value[i];
		else if( value[i] == '.' )
		{
			ip_index++;
			j = 0;
		}
	}
}

static void output_getIP(void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char value[MENU_ENTRY_INFO_LENGTH];
	char *key;
	int i = OUTPUT_INFO_GET_INDEX(pArg);
	outputIPOption type = (outputIPOption)OUTPUT_INFO_GET_TYPE(pArg);

	switch(type)
	{
		case optionIP:   key = "IPADDR";          break;
		case optionGW:   key = "DEFAULT_GATEWAY"; break;
		case optionMask: key = "NETMASK";         break;
		case optionDNS:  key = "NAMESERVERS";     break;
		default:
			return;
	}

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	if( getParam(path, key, "0.0.0.0", value) )
	{
		output_parseIP(value);
	}
}

static char* output_getIPfield(int field, void* pArg)
{
	return field < 4 ? &output_ip[field*4] : NULL;
}

static int output_changeIP(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char *key = "";
	int i = OUTPUT_INFO_GET_INDEX(pArg);
	outputIPOption type = (outputIPOption)OUTPUT_INFO_GET_TYPE(pArg);

	if( value == NULL )
		return 1;

	if (type != optionMode)
	{
		in_addr_t ip = inet_addr( inet_addr_prepare(value) );
		if(ip == INADDR_NONE || ip == INADDR_ANY)
		{
			interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
			return -1;
		}
	}

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

	switch(type)
	{
		case optionIP:   key = "IPADDR";          break;
		case optionGW:   key = "DEFAULT_GATEWAY"; break;
		case optionMask: key = "NETMASK";         break;
		case optionDNS:  key = "NAMESERVERS";     break;
		case optionMode: key = "BOOTPROTO";       break;
	}

	if( setParam(path, key, value) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	switch( i )
	{
		case ifaceWAN: output_fillWANMenu(pMenu, (void*)type); break;
#ifdef ENABLE_LAN
		case ifaceLAN: output_fillLANMenu(pMenu, (void*)type); break;
#endif
#ifdef ENABLE_WIFI
		case ifaceWireless: output_fillWifiMenu(pMenu, (void*)type); break;
#endif
	}
	interface_displayMenu(1);

	return 0;
}

static int output_toggleIP(interfaceMenu_t *pMenu, void* pArg)
{

	output_getIP((void*)OUTPUT_INFO_SET(optionIP,(int)pArg));
	interface_getText(pMenu, _T("ENTER_DEFAULT_IP"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeIP, output_getIPfield, inputModeDirect, (void*)OUTPUT_INFO_SET(optionIP,(int)pArg) );

	return 0;
}

static int output_toggleGw(interfaceMenu_t *pMenu, void* pArg)
{

	output_getIP((void*)OUTPUT_INFO_SET(optionGW,(int)pArg));
	interface_getText(pMenu, _T("ENTER_GATEWAY"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeIP, output_getIPfield, inputModeDirect, (void*)OUTPUT_INFO_SET(optionGW,(int)pArg) );

	return 0;
}

static int output_toggleDNSIP(interfaceMenu_t *pMenu, void* pArg)
{

	output_getIP((void*)OUTPUT_INFO_SET(optionDNS,(int)pArg));
	interface_getText(pMenu, _T("ENTER_DNS_ADDRESS"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeIP, output_getIPfield, inputModeDirect, (void*)OUTPUT_INFO_SET(optionDNS,(int)pArg) );

	return 0;
}

static int output_toggleNetmask(interfaceMenu_t *pMenu, void* pArg)
{

	output_getIP((void*)OUTPUT_INFO_SET(optionMask,(int)pArg));
	interface_getText(pMenu, _T("ENTER_NETMASK"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeIP, output_getIPfield, inputModeDirect, (void*)OUTPUT_INFO_SET(optionMask,(int)pArg) );

	return 0;
}

#ifdef ENABLE_LAN
char *output_BWField(int field, void* pArg)
{
	if( field == 0 )
	{
		static char BWValue[MENU_ENTRY_INFO_LENGTH];
		getParam(STB_CONFIG_FILE, "CONFIG_TRAFFIC_SHAPE", "0", BWValue);
		return BWValue;
	} else
		return NULL;
}

char *output_MACField(int field, void* pArg)
{
	int i;
	char *ptr;
	static char MACValue[MENU_ENTRY_INFO_LENGTH];
	getParam(STB_CONFIG_FILE, "CONFIG_GATEWAY_CLIENT_MAC", "", MACValue);

	ptr = MACValue;
	for (i=0;i<field;i++)
	{
		ptr = strchr(ptr, ':');
		if (ptr == NULL)
		{
			return NULL;
		}
		ptr++;
	}

	ptr[2] = 0;

	return ptr;
}

static int output_changeBW(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ivalue;
	char buf[32];

	if( value == NULL )
		return 1;

	if (helperFileExists(STB_CONFIG_OVERRIDE_FILE))
	{
		return 0;
	}

	interface_showMessageBox(_T("GATEWAY_IN_PROGRESS"), settings_renew, 0);

	ivalue = atoi(value);

	if (value[0] == 0 || ivalue <= 0)
	{
		strcpy(buf, "");
	} else
	{
		sprintf(buf, "%d", ivalue);
	}

	// Stop network interfaces
	system("/usr/local/etc/init.d/S90dhcpd stop");
	// Update settings
	setParam(STB_CONFIG_FILE, "CONFIG_TRAFFIC_SHAPE", buf);
	// Start network interfaces
	system("/usr/local/etc/init.d/S90dhcpd start");

	output_fillLANMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleGatewayBW(interfaceMenu_t *pMenu, void* pArg)
{

	interface_getText(pMenu, _T("GATEWAY_BANDWIDTH_INPUT"), "\\d*", output_changeBW, output_BWField, inputModeDirect, pArg);

	return 0;
}

static int output_confirmGatewayMode(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	char *str;
	gatewayMode_t mode = (gatewayMode_t)pArg;

	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		if (helperFileExists(STB_CONFIG_OVERRIDE_FILE))
		{
			return 0;
		}
		if (mode >= gatewayModeCount )
		{
			return 0;
		}

		interface_showMessageBox(_T("GATEWAY_IN_PROGRESS"), settings_renew, 0);

		switch( mode ) {
			case gatewayModeBridge: str = "BRIDGE"; break;
			case gatewayModeNAT:    str = "NAT"; break;
			case gatewayModeFull:   str = "FULL"; break;
			default:                str = "OFF"; break;
		}

		output_gatewayMode = mode;

		// Stop network interfaces
#ifdef ENABLE_WIFI
		system("/usr/local/etc/init.d/S80wifi stop");
#endif
		system("/usr/local/etc/init.d/S90dhcpd stop");
		system("/etc/init.d/S70servers stop");
#ifdef ENABLE_PPP
		system("/etc/init.d/S65ppp stop");
#endif
		system("/etc/init.d/S19network stop");
		// Update settings
		setParam(STB_CONFIG_FILE, "CONFIG_GATEWAY_MODE", str);
		// Start network interfaces
		system("/etc/init.d/S19network start");
#ifdef ENABLE_PPP
		system("/etc/init.d/S65ppp start");
#endif
		system("/etc/init.d/S70servers start");
		system("/usr/local/etc/init.d/S90dhcpd start");
#ifdef ENABLE_WIFI
		system("/usr/local/etc/init.d/S80wifi start");
#endif

		output_fillGatewayMenu(pMenu, (void*)0);

		interface_hideMessageBox();

		return 0;
	}
	return 1;
}

static int output_toggleGatewayMode(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showConfirmationBox(_T("GATEWAY_MODE_CONFIRM"), thumbnail_question, output_confirmGatewayMode, pArg);
	return 0;
}
#endif

static int output_toggleReset(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[PATH_MAX];
	int i = (int)pArg;

    interface_showMessageBox(_T("RENEW_IN_PROGRESS"), settings_renew, 0);

	switch(i)
	{
		case ifaceWAN:
#if !(defined STB225)
			sprintf(buf, "/usr/sbin/ifdown %s", helperEthDevice(i));
			system(buf);

			sleep(1);

			sprintf(buf, "/usr/sbin/ifup %s", helperEthDevice(i));
#else
			strcpy(buf, "/etc/init.d/additional/dhcp.sh");
#endif
			system(buf);
			output_fillWANMenu( (interfaceMenu_t*)&WANSubMenu, NULL );
			break;
#ifdef ENABLE_LAN
		case ifaceLAN:
			sprintf(buf, "/usr/local/etc/init.d/S90dhcpd stop");
			system(buf);

			sleep(1);

			sprintf(buf, "/usr/local/etc/init.d/S90dhcpd start");
			system(buf);
			break;
#endif
#ifdef ENABLE_WIFI
		case ifaceWireless:
			gfx_stopVideoProviders(screenMain);
#if (defined STB225)
			sprintf(buf, "/etc/init.d/additional/wifi.sh stop");
#else
			sprintf(buf, "/usr/local/etc/init.d/S80wifi stop");
#endif
			system(buf);

			sleep(1);

#if (defined STB225)
			sprintf(buf, "/etc/init.d/additional/wifi.sh start");
#else
			sprintf(buf, "/usr/local/etc/init.d/S80wifi start");
#endif
			system(buf);
			break;
#endif
		default:
			eprintf("%s: unknown iface %d\n", __FUNCTION__, i);
	}

    interface_hideMessageBox();

	return 0;
}

static int output_changeProxyAddr(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char *ptr1, *ptr2;
	char buf[MENU_ENTRY_INFO_LENGTH];
	int ret, port, i;

	if( value == NULL )
		return 1;

	strcpy(buf, value);

	ptr2 = buf;
	ptr1 = strchr(buf, ':');
	*ptr1 = 0;
	ptr1++;

	if (strlen(ptr1) == 0 && strlen(ptr2) == 3 /* dots */)
	{
		ret = setParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", NULL);
		unsetenv("http_proxy");
	}
	else
	{
		ptr2 = inet_addr_prepare(ptr2);
		if (inet_addr(ptr2) == INADDR_NONE || inet_addr(ptr2) == INADDR_ANY)
		{
			interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
			return -1;
		}

		if ( (port = atoi(ptr1)) <= 0)
		{
			interface_showMessageBox(_T("ERR_INCORRECT_PORT"), thumbnail_error, 0);
			return -1;
		}

		ptr2[strlen(ptr2) + 7] = 0;
		for ( i = strlen(ptr2) - 1 + 7; i > 6; --i)
		{
			ptr2[i] = ptr2[i-7];
		}
		strncpy(ptr2,"http://",7);
		sprintf(&ptr2[strlen(ptr2)], ":%d", port);
		dprintf("%s: HTTPProxyServer=%s\n", __FUNCTION__,ptr2);

		ret = setParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", ptr2);
		setenv("http_proxy", ptr2, 1);
	}

	if( ret == 0 )
	{
		output_fillWebMenu(pMenu, 0);
		interface_displayMenu(1);
	}
	return ret;
}

static char* output_getOption(outputUrlOption option)
{
	switch(option)
	{
#ifdef ENABLE_IPTV
		case optionRtpEpg:
			return appControlInfo.rtpMenuInfo.epg;
		case optionRtpPlaylist:
			return appControlInfo.rtpMenuInfo.playlist;
#endif
#ifdef ENABLE_VOD
		case optionVodPlaylist:
			return appControlInfo.rtspInfo[screenMain].streamInfoUrl;
#endif
		default:;
	}
	eprintf("%s: unknown option %d\n", __FUNCTION__, option);
	return NULL;
}

static int output_changeURL(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ret;
	char *dest;

	if( value == NULL )
	{
		if( (outputUrlOption)pArg == optionRtpPlaylist && appControlInfo.rtpMenuInfo.playlist[0] == 0 )
		{
			appControlInfo.rtpMenuInfo.usePlaylistURL = 0;
		}
		return 1;
	}

	dest = output_getOption((outputUrlOption)pArg);
	if( dest == NULL )
		return 1;

	if( value[0] == 0 )
	{
		dest[0] = 0;
		if( (outputUrlOption)pArg == optionRtpPlaylist )
		{
			appControlInfo.rtpMenuInfo.usePlaylistURL = 0;
		}
	} else
	{
		if( strlen( value ) < 8 )
		{
			interface_showMessageBox(_T("ERR_INCORRECT_URL"), thumbnail_warning, 3000);
			return 1;
		}
		if( strncasecmp( value, "http://", 7 ) == 0 || strncasecmp( value, "https://", 8 ) == 0 )
		{
			strcpy(dest, value);
		} else
		{
			sprintf(dest, "http://%s",value);
		}
	}
	if ((ret = saveAppSettings() != 0) && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	if (ret == 0)
	{
		switch((outputUrlOption)pArg)
		{
#ifdef ENABLE_IPTV
			case optionRtpEpg:
				rtp_cleanupEPG();
				output_fillIPTVMenu(pMenu, NULL);
				break;
			case optionRtpPlaylist:
				rtp_cleanupPlaylist(screenMain);
				output_fillIPTVMenu(pMenu, NULL);
				break;
#endif
#ifdef ENABLE_VOD
			case optionVodPlaylist:
				output_fillVODMenu(pMenu, NULL);
#endif
			default:;
		}
		interface_displayMenu(1);
	}

	return ret;
}

static int output_changeProxyLogin(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ret;

	if( value == NULL )
		return 1;

	ret = setParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", value);
	if (ret == 0)
	{
		setenv("http_proxy_login", value, 1);
		output_fillWebMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static int output_changeProxyPasswd(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ret;

	if( value == NULL )
		return 1;

	ret = setParam(BROWSER_CONFIG_FILE, "HTTPProxyPasswd", value);
	if (ret == 0)
	{
		setenv("http_proxy_passwd", value, 1);
		output_fillWebMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static int output_confirmReset(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
#ifdef STB225
		system("mount -o rw,remount /");
#endif
		system("rm -R /config/*");
#ifdef STB225
		system("mount -o ro,remount /");
#endif
		system("sync");
		system("reboot");
		return 0;
	}

	return 1;
}

static int output_resetSettings(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showConfirmationBox(_T("RESET_SETTINGS_CONFIRM"), thumbnail_question, output_confirmReset, pArg);
	return 1; // when called from askPassword, should return non-0 to leave getText message box opened
}

static char *output_getURL(int index, void* pArg)
{
	return index == 0 ? output_getOption((outputUrlOption)pArg) : NULL;
}

static int output_toggleURL(interfaceMenu_t *pMenu, void* pArg)
{
	char *str = "";
	switch( (outputUrlOption)pArg )
	{
		case optionRtpEpg:      str = _T("ENTER_IPTV_EPG_ADDRESS");  break;
		case optionRtpPlaylist: str = _T("ENTER_IPTV_LIST_ADDRESS"); break;
		case optionVodPlaylist: str = _T("ENTER_VOD_LIST_ADDRESS");  break;
	}
	interface_getText(pMenu, str, "\\w+", output_changeURL, output_getURL, inputModeABC, pArg);
	return 0;
}

#ifdef ENABLE_IPTV
static int output_toggleIPTVPlaylist(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.rtpMenuInfo.usePlaylistURL = (appControlInfo.rtpMenuInfo.usePlaylistURL+1)%2;
	if( appControlInfo.rtpMenuInfo.usePlaylistURL && appControlInfo.rtpMenuInfo.playlist[0] == 0 )
	{
		output_toggleURL( pMenu, (void*)optionRtpPlaylist );
	} else
	{
		if (saveAppSettings() != 0 && bDisplayedWarning == 0)
		{
			bDisplayedWarning = 1;
			interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
		}
		output_fillIPTVMenu( pMenu, pArg );
		interface_displayMenu(1);
	}
	return 0;
}
#endif

#ifdef ENABLE_VOD
static int output_toggleVODPlaylist(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.rtspInfo[screenMain].usePlaylistURL = (appControlInfo.rtspInfo[screenMain].usePlaylistURL+1)%2;
	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	output_fillVODMenu( pMenu, pArg );
	interface_displayMenu(1);
	return 0;
}
#endif

static int output_toggleProxyAddr(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_PROXY_ADDR"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}:\\d+", output_changeProxyAddr, NULL, inputModeDirect, pArg);
	return 0;
}

static int output_toggleProxyLogin(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_PROXY_LOGIN"), "\\w+", output_changeProxyLogin, NULL, inputModeABC, pArg);
	return 0;
}

static int output_toggleProxyPasswd(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_PROXY_PASSWD"), "\\w+", output_changeProxyPasswd, NULL, inputModeABC, pArg);
	return 0;
}

#ifdef ENABLE_BROWSER
char *output_getMWAddr(int field, void* pArg)
{
	if( field == 0 )
	{
		static char MWaddr[MENU_ENTRY_INFO_LENGTH];
		getParam(BROWSER_CONFIG_FILE, "HomeURL", "", MWaddr);
		return MWaddr;
	} else
		return NULL;
}

static int output_changeMWAddr(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	int ret;

	if( value == NULL )
		return 1;

	strcpy(buf, value);

	if(strncmp(value,"http",4) != 0)
	{
		sprintf(buf, "http://%s/", value);
	} else
	{
		strcpy(buf,value);
	}
	ret = setParam(BROWSER_CONFIG_FILE, "HomeURL", buf);

	if (ret == 0)
	{
		output_fillWebMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static int output_toggleMWAddr(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_MW_ADDR"), "\\w+", output_changeMWAddr, output_getMWAddr, inputModeABC, pArg);
	return 0;
}

static int output_toggleMWAutoLoading(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
	char temp[256];

	getParam(BROWSER_CONFIG_FILE, "AutoLoadingMW", "OFF", temp);
	dprintf("%s: %s \n", __FUNCTION__,temp);
	if (strcmp(temp,"OFF"))
	{
		dprintf("%s: Set OFF \n", __FUNCTION__);
		str = "OFF";
	} else
	{
		dprintf("%s: Set ON  \n", __FUNCTION__);
		str = "ON";
	}
	setParam(BROWSER_CONFIG_FILE, "AutoLoadingMW",str);
	output_fillWebMenu(pMenu, 0);
	interface_displayMenu(1);
	return 0;
}
#endif

static int output_toggleMode(interfaceMenu_t *pMenu, void* pArg)
{
	char value[MENU_ENTRY_INFO_LENGTH];
	char path[MAX_CONFIG_PATH];
	int i = (int)pArg;

	sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));
	getParam(path, "BOOTPROTO", "static", value);

	if (strcmp("dhcp+dns", value) == 0)
	{
		strcpy(value, "static");
	} else
	{
		strcpy(value, "dhcp+dns");
	}

	output_changeIP(pMenu, value, (void*)OUTPUT_INFO_SET( optionMode,i));

	switch( i )
	{
		case ifaceWAN: output_fillWANMenu(pMenu, (void*)i); break;
#ifdef ENABLE_LAN
		case ifaceLAN: output_fillLANMenu(pMenu, (void*)i); break;
#endif
#ifdef ENABLE_WIFI
		case ifaceWireless: output_fillWifiMenu(pMenu, (void*)i); break;
#endif
	}
	interface_displayMenu(1);

	return 0;
}

#ifdef ENABLE_VOD
static int output_changeVODIP(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if( value == NULL )
		return 1;

	value = inet_addr_prepare(value);
	if (inet_addr(value) == INADDR_NONE || inet_addr(value) == INADDR_ANY)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
		return -1;
	}

	strcpy(appControlInfo.rtspInfo[0].streamIP, value);

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillVODMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_changeVODINFOIP(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if( value == NULL )
		return 1;

	value = inet_addr_prepare(value);
	if (inet_addr(value) == INADDR_NONE || inet_addr(value) == INADDR_ANY)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
		return -1;
	}

	strcpy(appControlInfo.rtspInfo[0].streamInfoIP, value);

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillVODMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleVODIP(interfaceMenu_t *pMenu, void* pArg)
{
	output_parseIP( appControlInfo.rtspInfo[0].streamIP );
	interface_getText(pMenu, _T("ENTER_VOD_IP"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeVODIP, output_getIPfield, inputModeDirect, NULL);

	return 0;
}

static int output_toggleVODINFOIP(interfaceMenu_t *pMenu, void* pArg)
{
	output_parseIP( appControlInfo.rtspInfo[0].streamInfoIP );
	interface_getText(pMenu, _T("ENTER_VOD_INFO_IP"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeVODINFOIP, output_getIPfield, inputModeDirect, NULL);

	return 0;
}
#endif /* ENABLE_VOD */

#ifdef ENABLE_DVB
static int output_clearDvbSettings(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
#ifdef ENABLE_PVR
	if( pvr_hasDVBRecords() )
		str = _T("DVB_CLEAR_SERVICES_CONFIRM_CANCEL_PVR");
	else
#endif
	str = _T("DVB_CLEAR_SERVICES_CONFIRM");
	interface_showConfirmationBox(str, thumbnail_question, output_confirmClearDvb, pArg);

	return 0;
}

static int output_clearOffairSettings(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showConfirmationBox(_T("DVB_CLEAR_CHANNELS_CONFIRM"), thumbnail_question, output_confirmClearOffair, pArg);

	return 0;
}

static int output_confirmClearDvb(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		offair_clearServiceList(1);
		output_fillDVBMenu(pMenu, pArg);
		return 0;
	}

	return 1;
}

static int output_confirmClearOffair(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		offair_clearServices();
		offair_initServices();
		offair_fillDVBTOutputMenu(screenMain);
		return 0;
	}

	return 1;
}

#ifdef ENABLE_DVB_DIAG
static int output_toggleDvbDiagnosticsOnStart(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.offairInfo.diagnosticsMode = appControlInfo.offairInfo.diagnosticsMode != DIAG_ON ? DIAG_ON : DIAG_FORCED_OFF;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}
#endif

static int output_toggleDvbShowScrambled(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.offairInfo.dvbShowScrambled = (appControlInfo.offairInfo.dvbShowScrambled + 1) % 3;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	offair_initServices();
	offair_fillDVBTMenu();
	offair_fillDVBTOutputMenu(screenMain);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

static int output_toggleDvbNetworkSearch(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.dvbCommonInfo.networkScan = !appControlInfo.dvbCommonInfo.networkScan;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

static int output_toggleDvbInversion(interfaceMenu_t *pMenu, void* pArg)
{
	stb810_dvbfeInfo *fe;
	switch( dvb_getType(0) )
	{
		case FE_OFDM:
			fe = &appControlInfo.dvbtInfo.fe;
			break;
		case FE_QAM:
			fe = &appControlInfo.dvbcInfo.fe;
			break;
		default:
			return 0;
	}
	fe->inversion = fe->inversion == 0 ? 1 : 0;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

static int output_toggleDvbBandwidth(interfaceMenu_t *pMenu, void* pArg)
{
	switch (appControlInfo.dvbtInfo.bandwidth)
	{
		case BANDWIDTH_8_MHZ: appControlInfo.dvbtInfo.bandwidth = BANDWIDTH_7_MHZ; break;
		/*
		case BANDWIDTH_7_MHZ: appControlInfo.dvbtInfo.bandwidth = BANDWIDTH_6_MHZ; break;
		case BANDWIDTH_6_MHZ: appControlInfo.dvbtInfo.bandwidth = BANDWIDTH_AUTO; break;*/
		default:
			appControlInfo.dvbtInfo.bandwidth = BANDWIDTH_8_MHZ;
	}

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

static int output_toggleDvbSpeed(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.dvbCommonInfo.adapterSpeed = (appControlInfo.dvbCommonInfo.adapterSpeed+1) % 11;

    if (saveAppSettings() != 0 && bDisplayedWarning == 0)
    {
    	bDisplayedWarning = 1;
    	interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
    }

    output_fillDVBMenu(pMenu, pArg);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static int output_toggleDvbExtScan(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.dvbCommonInfo.extendedScan ^= 1;

    if (saveAppSettings() != 0 && bDisplayedWarning == 0)
    {
    	bDisplayedWarning = 1;
    	interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
    }

    output_fillDVBMenu(pMenu, pArg);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static char *output_getDvbRange(int field, void* pArg)
{
	if( field == 0 )
	{
		static char buffer[32];
		stb810_dvbfeInfo *fe;
		int id = (int)pArg;
		buffer[0] = 0;
		switch( dvb_getType(0) )
		{
			case FE_OFDM:
				fe = &appControlInfo.dvbtInfo.fe;
				break;
			case FE_QAM:
				fe = &appControlInfo.dvbcInfo.fe;
				break;
			default:
				return buffer;
		}
		switch (id)
		{
			case 0: sprintf(buffer, "%ld", fe->lowFrequency); break;
			case 1: sprintf(buffer, "%ld", fe->highFrequency); break;
			case 2: sprintf(buffer, "%ld", fe->frequencyStep); break;
			case 3: sprintf(buffer, "%ld", appControlInfo.dvbcInfo.symbolRate); break;
		}

		return buffer;
	} else
		return NULL;
}

static int output_changeDvbRange(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int id = (int)pArg;
	long val;
	stb810_dvbfeInfo *fe;

	if( value == NULL )
		return 1;

	val = strtol(value, NULL, 10);
	switch( dvb_getType(0) )
	{
		case FE_OFDM:
			fe = &appControlInfo.dvbtInfo.fe;
			break;
		case FE_QAM:
			fe = &appControlInfo.dvbcInfo.fe;
			break;
		default:
			return 0;
	}

	if ( (id < 3 && (val < 1000 || val > 860000)) ||
		 (id == 3 && (val < 1 || val > 50000)) )
	{
		interface_showMessageBox(_T("ERR_INCORRECT_FREQUENCY"), thumbnail_error, 0);
		return -1;
	}

	switch (id)
	{
		case 0: fe->lowFrequency = val; break;
		case 1: fe->highFrequency = val; break;
		case 2: fe->frequencyStep = val; break;
		case 3: appControlInfo.dvbcInfo.symbolRate = val; break;
		default: return 0;
	}

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleDvbModulation(interfaceMenu_t *pMenu, void* pArg)
{
	switch (appControlInfo.dvbcInfo.modulation)
	{
		case QAM_16: appControlInfo.dvbcInfo.modulation = QAM_32; break;
		case QAM_32: appControlInfo.dvbcInfo.modulation = QAM_64; break;
		case QAM_64: appControlInfo.dvbcInfo.modulation = QAM_128; break;
		case QAM_128: appControlInfo.dvbcInfo.modulation = QAM_256; break;
		default: appControlInfo.dvbcInfo.modulation = QAM_16;
	}

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillDVBMenu(pMenu, pArg);
	interface_displayMenu(1);

	return 0;
}

static int output_toggleDvbRange(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	int id = (int)pArg;

	switch (id)
	{
		case 0: sprintf(buf, "%s, %s: ", _T("DVB_LOW_FREQ"), _T("KHZ")); break;
		case 1: sprintf(buf, "%s, %s: ", _T("DVB_HIGH_FREQ"), _T("KHZ")); break;
		case 2: sprintf(buf, "%s, %s: ", _T("DVB_STEP_FREQ"), _T("KHZ")); break;
		case 3: sprintf(buf, "%s, %s: ", _T("DVB_SYMBOL_RATE"), _T("KHZ")); break;
		default: return -1;
	}

	interface_getText(pMenu, buf, "\\d+", output_changeDvbRange, output_getDvbRange, inputModeDirect, pArg);

	return 0;
}

int output_fillDVBMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char *str;
	stb810_dvbfeInfo *fe;

	interface_clearMenuEntries((interfaceMenu_t*)&DVBSubMenu);

	switch( dvb_getType(0) )
	{
		case FE_OFDM:
			fe = &appControlInfo.dvbtInfo.fe;
			break;
		case FE_QAM:
			fe = &appControlInfo.dvbcInfo.fe;
			break;
		default:
			return interface_menuActionShowMenu(pMenu, (void*)&DVBSubMenu);
	}

	sprintf(buf, PROFILE_LOCATIONS_PATH "/%s", appControlInfo.offairInfo.profileLocation);
	if (getParam(buf, "LOCATION_NAME", NULL, NULL))
	{
		str = _T("SETTINGS_WIZARD");
		interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, str, offair_wizardStart, NULL, thumbnail_scan);
	}

	str = _T("DVB_MONITOR");
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, str, offair_frequencyMonitor, NULL, thumbnail_info);

	str = _T("DVB_INSTALL");
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, str, offair_serviceScan, NULL, thumbnail_scan);

	str = _T("DVB_SCAN_FREQUENCY");
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, str, offair_frequencyScan, screenMain, thumbnail_scan);

	sprintf(buf, "%s (%d)", _T("DVB_CLEAR_SERVICES"), dvb_getNumberOfServices());
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_clearDvbSettings, screenMain, thumbnail_scan);

	str = _T("DVB_CLEAR_CHANNELS");
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, str, output_clearOffairSettings, screenMain, thumbnail_scan);

#ifdef ENABLE_DVB_DIAG
	sprintf(buf, "%s: %s", _T("DVB_START_WITH_DIAGNOSTICS"), _T( appControlInfo.offairInfo.diagnosticsMode == DIAG_FORCED_OFF ? "OFF" : "ON" ) );
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbDiagnosticsOnStart, NULL, thumbnail_configure);
#endif

	sprintf(buf, "%s: %s", _T("DVB_SHOW_SCRAMBLED"), _T( appControlInfo.offairInfo.dvbShowScrambled == SCRAMBLED_PLAY ? "PLAY" : (appControlInfo.offairInfo.dvbShowScrambled ? "ON" : "OFF") ) );
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbShowScrambled, NULL, thumbnail_configure);

	sprintf(buf, "%s: %s", _T("DVB_NETWORK_SEARCH"), _T( appControlInfo.dvbCommonInfo.networkScan ? "ON" : "OFF" ) );
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbNetworkSearch, NULL, thumbnail_configure);

	sprintf(buf, "%s: %s", _T("DVB_INVERSION"), _T( fe->inversion ? "ON" : "OFF" ) );
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbInversion, NULL, thumbnail_configure);

	if (dvb_getType(0) == FE_OFDM)
	{
		switch (appControlInfo.dvbtInfo.bandwidth)
		{
			case BANDWIDTH_8_MHZ: sprintf(buf, "%s: 8 %s", _T("DVB_BANDWIDTH"), _T( "MHZ" ) ); break;
			case BANDWIDTH_7_MHZ: sprintf(buf, "%s: 7 %s", _T("DVB_BANDWIDTH"), _T( "MHZ" ) ); break;
			case BANDWIDTH_6_MHZ: sprintf(buf, "%s: 6 %s", _T("DVB_BANDWIDTH"), _T( "MHZ" ) ); break;
			default:
				sprintf(buf, "%s: Auto", _T("DVB_BANDWIDTH") );
		}
		interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbBandwidth, NULL, thumbnail_configure);
	} else if (dvb_getType(0) == FE_QAM)
	{
		char *mod;

		sprintf(buf, "%s: %ld %s", _T("DVB_SYMBOL_RATE"),appControlInfo.dvbcInfo.symbolRate, _T("KHZ"));
		interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbRange, (void*)3, thumbnail_configure);

		switch (appControlInfo.dvbcInfo.modulation)
		{
			case QAM_16: mod = "QAM16"; break;
			case QAM_32: mod = "QAM32"; break;
			case QAM_64: mod = "QAM64"; break;
			case QAM_128: mod = "QAM128"; break;
			case QAM_256: mod = "QAM256"; break;
			default: mod = _T("NOT_AVAILABLE_SHORT");
		}

		sprintf(buf, "%s: %s", _T("DVB_QAM_MODULATION"), mod);
		interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbModulation, NULL, thumbnail_configure);
	}

	if (appControlInfo.dvbCommonInfo.adapterSpeed > 0)
	{
		sprintf(buf, "%s: %d%%", _T("DVB_SPEED"), 100-100*appControlInfo.dvbCommonInfo.adapterSpeed/10);
	} else
	{
		sprintf(buf, "%s: 1", _T("DVB_SPEED"));
	}
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbSpeed, NULL, thumbnail_configure);

	sprintf(buf,"%s: %s", _T("DVB_EXT_SCAN") , _T( appControlInfo.dvbCommonInfo.extendedScan == 0 ? "OFF" : "ON" ));
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbExtScan, NULL, thumbnail_configure);

	sprintf(buf, "%s: %ld %s", _T("DVB_LOW_FREQ"), fe->lowFrequency, _T("KHZ"));
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbRange, (void*)0, thumbnail_configure);

	sprintf(buf, "%s: %ld %s", _T("DVB_HIGH_FREQ"),fe->highFrequency, _T("KHZ"));
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbRange, (void*)1, thumbnail_configure);

	sprintf(buf, "%s: %ld %s", _T("DVB_STEP_FREQ"), fe->frequencyStep, _T("KHZ"));
	interface_addMenuEntry((interfaceMenu_t*)&DVBSubMenu, buf, output_toggleDvbRange, (void*)2, thumbnail_configure);

	interface_menuActionShowMenu(pMenu, (void*)&DVBSubMenu);

	return 0;
}
#endif /* ENABLE_DVB */

#ifdef ENABLE_VERIMATRIX
static int output_toggleVMEnable(interfaceMenu_t *pMenu, void* pArg)
{
    if (appControlInfo.useVerimatrix == 0)
    {
    	appControlInfo.useVerimatrix = 1;
    }
    else
    {
    	appControlInfo.useVerimatrix = 0;
    }

    if (saveAppSettings() != 0 && bDisplayedWarning == 0)
    {
    	bDisplayedWarning = 1;
    	interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
    }

    output_fillNetworkMenu(pMenu, 0);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static int output_changeVMAddress(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ret;

	if( value == NULL )
		return 1;

	value = inet_addr_prepare(value);
	if (inet_addr(value) == INADDR_NONE || inet_addr(value) == INADDR_ANY)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
		return -1;
	}

	ret = setParam(VERIMATRIX_INI_FILE, "SERVERADDRESS", value);
	if (ret == 0)
	{
		char buf[64];

		sprintf(buf, "%s/%d", value, VERIMATRIX_VKS_PORT);
		ret = setParam(VERIMATRIX_INI_FILE, "PREFERRED_VKS", buf);
	}

	if (ret == 0)
	{
		output_fillNetworkMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int len = strlen((char*)userp);

	if (len+size*nmemb >= DATA_BUFF_SIZE)
	{
		size = 1;
		nmemb = DATA_BUFF_SIZE-len-1;
	}

	memcpy(&((char*)userp)[len], buffer, size*nmemb);
	((char*)userp)[len+size*nmemb] = 0;
	return size*nmemb;
}

static int output_getVMRootCert(interfaceMenu_t *pMenu, void* pArg)
{
	char info_url[MAX_URL];
	int fd, code;
	char rootcert[DATA_BUFF_SIZE];
	char errbuff[CURL_ERROR_SIZE];
	CURLcode ret;
	CURL *hnd;

	hnd = curl_easy_init();

	memset(rootcert, 0, sizeof(rootcert));

	sprintf(info_url, "http://%s/%s", appControlInfo.rtspInfo[0].streamInfoIP, "rootcert.pem");

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, rootcert);
	curl_easy_setopt(hnd, CURLOPT_URL, info_url);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuff);
	curl_easy_setopt(hnd, CURLOPT_CONNECTTIMEOUT, 5);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 15);

	ret = curl_easy_perform(hnd);

	if (ret == 0)
	{
		curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &code);
		if (code != 200)
		{
			ret = -1;
		} else
		{
			dprintf("%s: rootcert data %d:\n%s\n", __FUNCTION__, strlen(rootcert), rootcert);
		}
	}

	curl_easy_cleanup(hnd);

	if (ret == 0)
	{
		getParam(VERIMATRIX_INI_FILE, "ROOTCERT", VERIMATRIX_ROOTCERT_FILE, errbuff);
		dprintf("%s: Will write to: %s\n", __FUNCTION__, errbuff);
		fd = open(errbuff, O_CREAT|O_WRONLY|O_TRUNC);
		if (fd > 0)
		{
			write(fd, rootcert, strlen(rootcert));
			close(fd);
		} else
		{
			ret = -1;
		}
	}

	if (ret == 0)
	{
		interface_showMessageBox(_T("GOT_ROOTCERT"), thumbnail_yes, 0);
		return ret;
	} else
	{
		interface_showMessageBox(_T("ERR_FAILED_TO_GET_ROOTCERT"), thumbnail_error, 0);
		return ret;
	}

	return ret;
}

static int output_changeVMCompany(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int ret;

	if( value == NULL )
		return 1;

	ret = setParam(VERIMATRIX_INI_FILE, "COMPANY", value);

	if (ret == 0)
	{
		output_fillNetworkMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static int output_toggleVMAddress(interfaceMenu_t *pMenu, void* pArg)
{

	interface_getText(pMenu, _T("VERIMATRIX_ADDRESS"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeVMAddress, NULL, inputModeDirect, pArg);

	return 0;
}

static int output_toggleVMCompany(interfaceMenu_t *pMenu, void* pArg)
{

	interface_getText(pMenu, _T("VERIMATRIX_COMPANY"), "\\w+", output_changeVMCompany, NULL, inputModeABC, pArg);

	return 0;
}
#endif // #ifdef ENABLE_VERIMATRIX

#ifdef ENABLE_SECUREMEDIA
static int output_toggleSMEnable(interfaceMenu_t *pMenu, void* pArg)
{
    if (appControlInfo.useSecureMedia == 0)
    {
    	appControlInfo.useSecureMedia = 1;
    }
    else
    {
    	appControlInfo.useSecureMedia = 0;
    }

    if (saveAppSettings() != 0 && bDisplayedWarning == 0)
    {
    	bDisplayedWarning = 1;
    	interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
    }

    output_fillNetworkMenu(pMenu, 0);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static int output_changeSMAddress(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int type = (int)pArg;
	int ret;

	if( value == NULL )
		return 1;

	value = inet_addr_prepare(value);
	if (inet_addr(value) == INADDR_NONE || inet_addr(value) == INADDR_ANY)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_IP"), thumbnail_error, 0);
		return -1;
	}

	if (type == 1)
	{
		ret = setParam(SECUREMEDIA_CONFIG_FILE, "SECUREMEDIA_ESAM_HOST", value);
	} else
	{
		ret = setParam(SECUREMEDIA_CONFIG_FILE, "SECUREMEDIA_RANDOM_HOST", value);
	}

	if (ret == 0)
	{
		/* Restart smdaemon */
		system("killall smdaemon");
		output_fillNetworkMenu(pMenu, 0);
		interface_displayMenu(1);
	}

	return ret;
}

static int output_toggleSMAddress(interfaceMenu_t *pMenu, void* pArg)
{
	int type = (int)pArg;

	if (type == 1)
	{
		interface_getText(pMenu, _T("SECUREMEDIA_ESAM_HOST"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeSMAddress, NULL, inputModeDirect, pArg);
	} else
	{
		interface_getText(pMenu, _T("SECUREMEDIA_RANDOM_HOST"), "\\d{3}.\\d{3}.\\d{3}.\\d{3}", output_changeSMAddress, NULL, inputModeDirect, pArg);
	}

	return 0;
}
#endif // #ifdef ENABLE_SECUREMEDIA

static int output_toggleAspectRatio(interfaceMenu_t *pMenu, void* pArg)
{
	if (appControlInfo.outputInfo.aspectRatio == aspectRatio_4x3)
	{
		appControlInfo.outputInfo.aspectRatio = aspectRatio_16x9;
	} else
	{
		appControlInfo.outputInfo.aspectRatio = aspectRatio_4x3;
	}

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

    output_fillVideoMenu(pMenu, pArg);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static int output_toggleAutoScale(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.outputInfo.autoScale = appControlInfo.outputInfo.autoScale == videoMode_scale ? videoMode_stretch : videoMode_scale;

#ifdef STB225
	gfxUseScaleParams = 0;
    (void)event_send(gfxDimensionsEvent);
#endif

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

    output_fillVideoMenu(pMenu, pArg);
    //menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
    return 0;
}

static int output_toggleScreenFiltration(interfaceMenu_t *pMenu, void* pArg)
{
	appControlInfo.outputInfo.bScreenFiltration = (appControlInfo.outputInfo.bScreenFiltration + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillVideoMenu(pMenu, pArg);
	//menuInfra_display(&OutputMenu);
	interface_displayMenu(1);
	return 0;
}

/* -------------------------- MENU DEFINITION --------------------------- */


static void output_fillStandardMenu(void)
{
    int selected = MENU_ITEM_BACK;
	char *str;

    //StandardMenu.prev = &OutputMenu;
	interface_clearMenuEntries((interfaceMenu_t*)&StandardMenu);
#ifdef STB6x8x
    /*Add menu items automatically*/
    if (appControlInfo.outputInfo.encDesc[0].caps & DSECAPS_TV_STANDARDS)
    {
        int n;
        int position = 0;
        for (n=0; tv_standards[n].standard; n++)
        {
            /* the following will only work if the supported resolutions is only one value when you have a DIGITAL (HD) output.*/
            if (appControlInfo.outputInfo.encDesc[0].tv_standards & tv_standards[n].standard)
            {
                if (tv_standards[n].standard == appControlInfo.outputInfo.encConfig[0].tv_standard)
                {
                    selected = position;
                }
#ifdef STBPNX
				str = (tv_standards[n].standard == DSETV_DIGITAL) ? (char*)resolutions[0].name : (char*) tv_standards[n].name;
#else
				str = (char*) tv_standards[n].name;
#endif
				interface_addMenuEntry((interfaceMenu_t*)&StandardMenu, str, output_setStandard, (void*) tv_standards[n].standard, tv_standards[n].standard == appControlInfo.outputInfo.encConfig[0].tv_standard ? thumbnail_selected : thumbnail_tvstandard);
				position++;
/*                menuInfra_setEntry(&StandardMenu,
                                   position++,
                                   menuEntryText,
                                   (tv_standards[n].standard == DSETV_DIGITAL) ?  (char*)resolutions[0].name : (char*) tv_standards[n].name,
                                   output_setStandard,
                                   (void*) tv_standards[n].standard);*/
            }
        }
    }
#else
	{
		char buf[128];
		getParam("/etc/init.d/S35pnxcore.sh", "resolution", "1280x720x60p", buf);

		str = "1280x720x60p";
		interface_addMenuEntry((interfaceMenu_t*)&StandardMenu, str, output_setStandard, (void*) 720, strstr(buf, str) != 0 ? thumbnail_selected : thumbnail_tvstandard);
		str = "1920x1080x60i";
		interface_addMenuEntry((interfaceMenu_t*)&StandardMenu, str, output_setStandard, (void*) 1080, strstr(buf, str) != 0 ? thumbnail_selected : thumbnail_tvstandard);
		selected = strstr(buf, str) != 0;
	}
#endif
    /* Ensure that the correct entry is selected */
    //menuInfra_setSelected(&StandardMenu, selected);
    //menuInfra_setHighlight(&StandardMenu, selected);
	interface_setSelectedItem((interfaceMenu_t*)&StandardMenu, selected);
}


static char *output_getDateValue(int field, void* pArg)
{
	static char buf[8];
	struct tm *lt;
	time_t t;

	time(&t);
	lt = localtime(&t);

	switch (field)
	{
		case 0:
			sprintf(buf, "%02d", lt->tm_mday);
			break;
		case 1:
			sprintf(buf, "%02d", lt->tm_mon+1);
			break;
		case 2:
			sprintf(buf, "%04d", lt->tm_year+1900);
			break;
		default:
			sprintf(buf, "%02d", 0);
	}

	return buf;
}

static int output_changeDateValue(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	struct tm newtime;
	struct tm *lt;
	time_t t;

	if( value == NULL )
		return 1;

	time(&t);
	lt = localtime(&t);

	memcpy(&newtime, lt, sizeof(struct tm));

	dprintf("%s: set time: %s\n", __FUNCTION__, value);

	if (sscanf(value, "%d.%d.%d", &newtime.tm_mday, &newtime.tm_mon, &newtime.tm_year) == 3 &&
		newtime.tm_mday >= 1 && newtime.tm_mday < 32 &&
		newtime.tm_mon >= 1 && newtime.tm_mon < 13 &&
		newtime.tm_year >= 1900)
	{
		struct timeval tv;

		newtime.tm_year -= 1900;
		newtime.tm_mon -= 1;

		tv.tv_usec = 0;
		tv.tv_sec = mktime(&newtime);

		settimeofday(&tv, NULL);

		system("hwclock -w -u");
	} else
	{
		interface_showMessageBox(_T("ERR_INCORRECT_DATE"), thumbnail_error, 0);
	}

	output_fillTimeMenu(pMenu, pArg);
	interface_displayMenu(1);

	return 0;
}

static int output_setDate(interfaceMenu_t *pMenu, void* pArg)
{
	struct tm newtime;
	struct tm *lt;
	time_t t;
	char temp[5];
	if( pArg != &DateEntry )
	{
		eprintf("%s: wrong edit entry %p (should be %p)\n", __FUNCTION__, pArg, &DateEntry);
		return 1;
	}

	time(&t);
	lt = localtime(&t);
	memcpy(&newtime, lt, sizeof(struct tm));

	temp[2] = 0;
	temp[0] = DateEntry.info.date.value[0];
	temp[1] = DateEntry.info.date.value[1];
	newtime.tm_mday = atoi(temp);
	temp[0] = DateEntry.info.date.value[2];
	temp[1] = DateEntry.info.date.value[3];
	newtime.tm_mon = atoi(temp) - 1;
	temp[4] = 0;
	memcpy(temp, &DateEntry.info.date.value[4], 4);
	newtime.tm_year = atoi(temp) - 1900;
	if( newtime.tm_year < 0 || newtime.tm_mon < 0 || newtime.tm_mon > 11 || newtime.tm_mday < 0 || newtime.tm_mday > 31 )
	{
		interface_showMessageBox(_T("ERR_INCORRECT_DATE"), thumbnail_error, 0);
		return 2;
	}

	if( lt->tm_mday != newtime.tm_mday ||
	    lt->tm_mon  != newtime.tm_mon  ||
	    lt->tm_year != newtime.tm_year )
	{
		struct timeval tv;
		interfaceEditEntry_t *pEditEntry = (interfaceEditEntry_t *)pArg;

		tv.tv_usec = 0;
		tv.tv_sec = mktime(&newtime);

		settimeofday(&tv, NULL);
		system("hwclock -w -u");

		output_fillTimeMenu(pMenu, pEditEntry->pArg);
		interface_displayMenu(1);
	}
	return 0;
}

static char *output_getTimeValue(int field, void* pArg)
{
	static char buf[8];
	struct tm *lt;
	time_t t;

	time(&t);
	lt = localtime(&t);

	switch (field)
	{
		case 0:
			sprintf(buf, "%02d", lt->tm_hour);
			break;
		case 1:
			sprintf(buf, "%02d", lt->tm_min);
			break;
		case 2:
			sprintf(buf, "%02d", lt->tm_sec);
			break;
		default:
			sprintf(buf, "%02d", 0);
	}

	return buf;
}

static int output_changeTimeValue(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	struct tm newtime;
	struct tm *lt;
	time_t t;

	if( value == NULL )
		return 1;

	time(&t);
	lt = localtime(&t);

	memcpy(&newtime, lt, sizeof(struct tm));

	dprintf("%s: set time: %s\n", __FUNCTION__, value);

	if (sscanf(value, "%d:%d:%d", &newtime.tm_hour, &newtime.tm_min, &newtime.tm_sec) == 3 &&
		newtime.tm_hour >= 0 && newtime.tm_hour < 24 &&
		newtime.tm_min >= 0 && newtime.tm_min < 60 &&
		newtime.tm_sec >= 0 && newtime.tm_sec < 60)
	{
		struct timeval tv;

		tv.tv_usec = 0;
		tv.tv_sec = mktime(&newtime);

		settimeofday(&tv, NULL);

		system("hwclock -w -u");
	} else
	{
		interface_showMessageBox(_T("ERR_INCORRECT_TIME"), thumbnail_error, 0);
	}

	output_fillTimeMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}

static int output_setTime(interfaceMenu_t *pMenu, void* pArg)
{
	char temp[3];
	struct tm newtime;
	struct tm *lt;
	time_t t;

	time(&t);
	lt = localtime(&t);

	memcpy(&newtime, lt, sizeof(struct tm));

	temp[2] = 0;
	temp[0] = TimeEntry.info.time.value[0];
	temp[1] = TimeEntry.info.time.value[1];
	newtime.tm_hour = atoi(temp);
	temp[0] = TimeEntry.info.time.value[2];
	temp[1] = TimeEntry.info.time.value[3];
	newtime.tm_min = atoi(temp);
	if( newtime.tm_hour < 0 || newtime.tm_hour > 23 || newtime.tm_min  < 0 || newtime.tm_min  > 59 )
	{
		interface_showMessageBox(_T("ERR_INCORRECT_TIME"), thumbnail_error, 0);
		return 2;
	}
	if( lt->tm_hour != newtime.tm_hour ||
	    lt->tm_min  != newtime.tm_min)
	{
		struct timeval tv;
		interfaceEditEntry_t *pEditEntry = (interfaceEditEntry_t *)pArg;

		tv.tv_usec = 0;
		tv.tv_sec = mktime(&newtime);

		settimeofday(&tv, NULL);

		system("hwclock -w -u");

		output_fillTimeMenu(pMenu, pEditEntry->pArg);
		interface_displayMenu(1);
	}
	return 0;
}

static int output_changeTime(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;

	str = _T("SET_TIME");
	interface_getText(pMenu, str, "\\d{2}:\\d{2}:\\d{2}", output_changeTimeValue, output_getTimeValue, inputModeDirect, pArg);

	return 0;
}

static int output_changeDate(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;

	str = _T("SET_DATE_VALUE");
	interface_getText(pMenu, str, "\\d{2}.\\d{2}.\\d{4}", output_changeDateValue, output_getDateValue, inputModeDirect, pArg);

	return 0;
}

static int output_setTimeZone(interfaceMenu_t *pMenu, void* pArg)
{
	FILE *f;

	f = fopen("/config/timezone", "w");
	if (f != NULL)
	{
		fprintf(f, "/usr/share/zoneinfo/%s", (char*)pArg);
		fclose(f);

		system("/etc/init.d/S18timezone restart");

	} else
	{
		if (bDisplayedWarning == 0)
		{
			bDisplayedWarning = 1;
			interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
		}
	}

	tzset();

	output_fillTimeMenu(pMenu, pArg);

	return 0;
}

static char* output_getNTP(int field, void* pArg)
{
	if( field == 0 )
	{
		static char ntp[MENU_ENTRY_INFO_LENGTH];
		getParam(STB_CONFIG_FILE, "NTPSERVER", "", ntp);
		return ntp;
	} else
		return NULL;
}

static int output_changeNTPValue(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if( value == NULL )
		return 1;

	if( setParam(STB_CONFIG_FILE, "NTPSERVER", value) != 0 )
	{
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	} else
	{
		system("/usr/sbin/ntpupdater");
		output_fillTimeMenu(pMenu, pArg);
	}

	return 0;
}

static int output_setNTP(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_NTP_ADDRESS"), "\\w+", output_changeNTPValue, output_getNTP, inputModeABC, pArg);
	return 0;
}

static void output_fillTimeZoneMenu(void)
{
	int i;
	char *str;
	char buf[BUFFER_SIZE];
	int found = 12;

	interface_clearMenuEntries((interfaceMenu_t*)&TimeZoneMenu);

	if (!helperParseLine(INFO_TEMP_FILE, "cat /config/timezone", "zoneinfo/", buf, '0'))
	{
		buf[0] = 0;
	}

	for (i=0; i<(int)(sizeof(timezones)/sizeof(timezones[0])); i++)
	{
		str = timezones[i].desc;
		interface_addMenuEntry((interfaceMenu_t*)&TimeZoneMenu, str, output_setTimeZone, (void*)timezones[i].file, thumbnail_log);
		if (strcmp(timezones[i].file, buf) == 0)
		{
			found = i;
		}
	}

	interface_setSelectedItem((interfaceMenu_t*)&TimeZoneMenu, found);
}

/**
 * This function now uses the Encoder description to get the supported outout formats instead of the Output API.
 */
static void output_fillFormatMenu(void)
{
    //int position = 0;
    int n;
	int position = 0;
	int selected = MENU_ITEM_BACK;
	char *signalName;
	int signalEnable;

    //FormatMenu.prev = &OutputMenu;
	interface_clearMenuEntries((interfaceMenu_t*)&FormatMenu);

	/*Add menu items automatically*/
	if (appControlInfo.outputInfo.encDesc[0].caps & DSECAPS_OUT_SIGNALS)
	{
		for (n=0; signals[n].signal; n++)
		{
			if (appControlInfo.outputInfo.encDesc[0].out_signals & signals[n].signal)
			{
				switch( signals[n].signal )
				{
					case DSOS_YC:
						signalName = "S-Video";
						signalEnable = 1;
						break;
					case DSOS_RGB:
#ifndef HIDE_EXTRA_FUNCTIONS
						signalName = "SCART-RGB";
						signalEnable = 0;
						break;
#else
						continue;
#endif
					case DSOS_YCBCR:
#ifndef HIDE_EXTRA_FUNCTIONS
						signalName = "SCART-YUYV";
						signalEnable = 0;
						break;
#else
						continue;
#endif
					case DSOS_CVBS:
						signalName = "CVBS";
						signalEnable = 1;
						break;
					default:
						signalName = (char*)signals[n].name;
						signalEnable = 0;
				}
				if (signals[n].signal == appControlInfo.outputInfo.encConfig[0].out_signals)
				{
					selected = position;
				}
				interface_addMenuEntryCustom((interfaceMenu_t*)&FormatMenu, interfaceMenuEntryText, signalName, strlen(signalName)+1, signalEnable, output_setFormat, NULL, NULL, NULL, (void*) signals[n].signal, signals[n].signal == appControlInfo.outputInfo.encConfig[0].out_signals ? thumbnail_selected : thumbnail_channels);
				position++;
				/*menuInfra_setEntry(&FormatMenu,
				position++,
				menuEntryText,
				(char*) signals[n].name,
				output_setFormat,
				(void*) signals[n].signal);*/
			}
		}
	}
	interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, selected);
}

static void output_fillBlankingMenu(void)
{
	//int position = 0;
	char *str;
	interface_clearMenuEntries((interfaceMenu_t*)&BlankingMenu);
	//BlankingMenu.prev = &OutputMenu;
	str = "4 x 3";
	interface_addMenuEntry((interfaceMenu_t*)&BlankingMenu, str, output_setBlanking, (void*)DSOSB_4x3, thumbnail_configure);
	//menuInfra_setEntry(&BlankingMenu, position++, menuEntryText, "4 x 3", output_setBlanking, (void*)DSOSB_4x3);
	str = "16 x 9";
	interface_addMenuEntry((interfaceMenu_t*)&BlankingMenu, str, output_setBlanking, (void*)DSOSB_16x9, thumbnail_configure);
	//menuInfra_setEntry(&BlankingMenu, position++, menuEntryText, "16 x 9", output_setBlanking, (void*)DSOSB_16x9);
	str = "None";
	interface_addMenuEntry((interfaceMenu_t*)&BlankingMenu, str, output_setBlanking, (void*)DSOSB_OFF, thumbnail_configure);
	//menuInfra_setEntry(&BlankingMenu, position++, menuEntryText, "None", output_setBlanking, (void*)DSOSB_OFF);
}

long get_info_progress()
{
	return info_progress;
}

int show_info(interfaceMenu_t* pMenu, void* pArg)
{
	char buf[256];
	char temp[256];
	char info_text[4096];
	char *iface_name;
	int i;

	info_text[0] = 0;

	info_progress = 0;

	eprintf("output: Start collecting info...\n");

	interface_sliderSetText( _T("COLLECTING_INFO"));
	interface_sliderSetMinValue(0);
	interface_sliderSetMaxValue(9);
	interface_sliderSetCallbacks(get_info_progress, NULL, NULL);
	interface_sliderSetDivisions(100);
	interface_sliderShow(1, 1);

#ifdef STBxx
	int fd;
	char *vendor;

	fd = open("/proc/nxp/drivers/pnx8550/video/renderer0/output_resolution", O_RDONLY);
	if (fd > 0)
	{
		vendor = "nxp";
		close(fd);
	} else
	{
		vendor = "philips";
	}

#ifdef STB82
	systemId_t sysid;
	systemSerial_t serial;
	unsigned long stmfw;

	if (helperParseLine(INFO_TEMP_FILE, "cat /dev/sysid", "SERNO: ", buf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
	{
		serial.SerialFull = strtoul(buf, NULL, 16);
	} else {
		serial.SerialFull = 0;
	}

	if (helperParseLine(INFO_TEMP_FILE, NULL, "SYSID: ", buf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
	{
		sysid.IDFull = strtoul(buf, NULL, 16);
	} else {
		sysid.IDFull = 0;
	}

	if (helperParseLine(INFO_TEMP_FILE, NULL, "VER: ", buf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
	{
		stmfw = strtoul(buf, NULL, 16);
	} else {
		stmfw = 0;
	}

	get_composite_serial(sysid, serial, temp);
	sprintf(info_text,"%s: %s\n",_T("SERIAL_NUMBER"),temp);
	if (stmfw > 0x0106 && helperParseLine(INFO_TEMP_FILE, "stmclient 7", "MAC: ", buf, ' ')) // MAC: 02:EC:D0:00:00:39
	{
		char mac[6];
		sscanf(buf, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
		sprintf(temp, "%s 1: %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n", _T("MAC_ADDRESS"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	} else {
		sprintf(temp, "%s 1: %s\n", _T("MAC_ADDRESS"), _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, temp);

	if (stmfw > 0x0106 && helperParseLine(INFO_TEMP_FILE, "stmclient 8", "MAC: ", buf, ' ')) // MAC: 02:EC:D0:00:00:39
	{
		char mac[6];
		sscanf(buf, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);
		sprintf(temp, "%s 2: %02hhX%02hhX%02hhX%02hhX%02hhX%02hhX\n", _T("MAC_ADDRESS"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	} else {
		sprintf(temp, "%s 2: %s\n", _T("MAC_ADDRESS"), _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, temp);

	strcat(info_text,_T("STM_VERSION"));
	strcat(info_text,": ");
	if (stmfw != 0)
	{
		sprintf(temp, "%lu.%lu", (stmfw >> 8)&0xFF, (stmfw)&0xFF);
		strcat(info_text, temp);
	} else
	{
		strcat(info_text, _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, "\n");

	strcat(info_text, _T("SERIAL_NUMBER_OLD"));
	strcat(info_text,": ");
	if (serial.SerialFull != 0)
	{
		sprintf(temp, "%u\n", serial.SerialFull);
	} else
	{
		sprintf(temp, "%s\n", _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, temp);

	strcat(info_text, _T("SYSID"));
	strcat(info_text,": ");
	if (sysid.IDFull != 0)
	{
		get_system_id(sysid, temp);
		strcat(temp,"\n");
	} else
	{
		sprintf(temp, "%s\n", _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, temp);
	info_progress++;
	interface_displayMenu(1);
#endif // STB82
	sprintf(temp, "%s: %s\n%s: %s\n%s: %s\n", _T("APP_RELEASE"), RELEASE_TYPE, _T("APP_VERSION"), REVISION, _T("COMPILE_TIME"), COMPILE_TIME);
	strcat(info_text,temp);

#ifdef STB82
	sprintf(buf, "%s: %d MB\n", _T("MEMORY_SIZE"), appControlInfo.memSize);
	strcat(info_text, buf);
#endif

	if (helperParseLine(INFO_TEMP_FILE, "date -R", "", buf, '\r'))
	{
  	sprintf(temp, "%s: %s\n",_T("CURRENT_TIME"), buf);
	} else
	{
		sprintf(temp, "%s: %s\n",_T("CURRENT_TIME"), _T("NOT_AVAILABLE_SHORT"));
	}
	strcat(info_text, temp);

	info_progress++;
	interface_displayMenu(1);

	for (i=0;;i++)
	{
		sprintf(temp, "/sys/class/net/%s", helperEthDevice(i));
		if (helperCheckDirectoryExsists(temp))
		{
			switch( i )
			{
				case ifaceWAN: iface_name = "WAN"; break;
#ifdef ENABLE_LAN
				case ifaceLAN: iface_name = "LAN"; break;
#endif
#ifdef ENABLE_PPP
				case ifacePPP: iface_name = "PPP"; break;
#endif
#ifdef ENABLE_WIFI
				case ifaceWireless: iface_name = _T("WIRELESS"); break;
#endif
				default:
					eprintf("%s: unknown network interface %s\n", temp);
					iface_name = "";
			}
			strcat(info_text, "\n");
			sprintf(temp, "ifconfig %s | grep HWaddr", helperEthDevice(i));
			if (helperParseLine(INFO_TEMP_FILE, temp, "HWaddr ", buf, ' '))			 // eth0      Link encap:Ethernet  HWaddr 76:60:37:02:24:02
			{
				sprintf(temp, "%s %s: ", iface_name, _T("MAC_ADDRESS"));
				strcat(info_text, temp);
				strcat(info_text, buf);
				strcat(info_text, "\n");
			} else
			{
				sprintf(temp, "%s %s: %s\n", iface_name, _T("MAC_ADDRESS"), _T("NOT_AVAILABLE_SHORT"));
				strcat(info_text, temp);
			}

			sprintf(temp, "ifconfig %s | grep \"inet addr\"", helperEthDevice(i));
			if (helperParseLine(INFO_TEMP_FILE, temp, "inet addr:", buf, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				sprintf(temp, "%s %s: ", iface_name, _T("IP_ADDRESS"));
				strcat(info_text, temp);
				strcat(info_text, buf);
				strcat(info_text, "\n");
			} else {
				sprintf(temp, "%s %s: %s\n", iface_name, _T("IP_ADDRESS"), _T("NOT_AVAILABLE_SHORT"));
				strcat(info_text, temp);
			}

			sprintf(temp, "ifconfig %s | grep \"Mask:\"", helperEthDevice(i));
			if (helperParseLine(INFO_TEMP_FILE, temp, "Mask:", buf, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				sprintf(temp, "%s %s: ", iface_name, _T("NETMASK"));
				strcat(info_text, temp);
				strcat(info_text, buf);
				strcat(info_text, "\n");
			} else {
				sprintf(temp, "%s %s: %s\n", iface_name, _T("NETMASK"), _T("NOT_AVAILABLE_SHORT"));
				strcat(info_text, temp);
			}

			sprintf(temp, "route -n | grep -e \"0\\.0\\.0\\.0 .* 0\\.0\\.0\\.0 *UG .* %s\"", helperEthDevice(i));
			if (helperParseLine(INFO_TEMP_FILE, temp, "0.0.0.0", buf, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				sprintf(temp, "%s %s: ", iface_name, _T("GATEWAY"));
				strcat(info_text, temp);
				strcat(info_text, buf);
				strcat(info_text, "\n");
			} else {
				sprintf(temp, "%s %s: %s\n", iface_name, _T("GATEWAY"), _T("NOT_AVAILABLE_SHORT"));
				strcat(info_text, temp);
			}
		} else
		{
			break;
		}
	}
	strcat(info_text, "\n");
	i = -1;
	fd = open( "/etc/resolv.conf", O_RDONLY );
	if( fd > 0 )
	{
		char *ptr;
		while( helperReadLine( fd, buf ) == 0 && buf[0] )
		{
			if( (ptr = strstr( buf, "nameserver " )) != NULL )
			{
				ptr += 11;
				++i;
				sprintf(temp, "%s %d: %s\n", _T("DNS_SERVER"), i+1, ptr);
				strcat(info_text, temp);
			}
		}
		close(fd);
	}
	if( i < 0 )
	{
		sprintf(temp, "%s: %s\n", _T("DNS_SERVER"), _T("NOT_AVAILABLE_SHORT"));
		strcat(info_text, temp);
	}

	info_progress++;
	interface_displayMenu(1);

#if 0
	strcat(info_text, "\n");

	/* prefetch messages for speedup */
	system("cat /var/log/messages | grep -e \"ate wi\" -e \"TM . n\" > /tmp/tmmsg");
	system("mkdir -p /tmp/dp");
	sprintf(buf, "cat /proc/%s/dp0 >> /tmp/dp/dp0", vendor);
	system(buf);
	sprintf(buf, "cat /proc/%s/dp1 >> /tmp/dp/dp1", vendor);
	system(buf);
	system("cat /tmp/dp/dp0 | grep -e \"\\$Build: \" -e \"CPU RSE\" > /tmp/tm0dp");
	system("cat /tmp/dp/dp1 | grep -e \"\\$Build: \" -e \"CPU RSE\" > /tmp/tm1dp");

	info_progress++;
	interface_displayMenu(1);

	if (helperParseLine(INFO_TEMP_FILE, "cat /tmp/dp/dp0init | grep \"\\$Build: \"", "$Build: ", buf, '$'))
	{
		strcat(info_text, LANG_TM0_IMAGE ": ");
		strcat(info_text, buf);
		strcat(info_text, "\n");
	} else
	{
		strcat(info_text, LANG_TM0_IMAGE ": " LANG_NOT_AVAILABLE_SHORT "\n");
	}

	info_progress++;
	interface_displayMenu(1);

	if (helperParseLine(INFO_TEMP_FILE, "cat /tmp/tm0dp | grep \"CPU RSE\"", NULL, NULL, ' ') ||
		helperParseLine(INFO_TEMP_FILE, "cat /tmp/tmmsg | grep \"unable to communicate with TriMedia 0\"", NULL, NULL, ' ') ||
		helperParseLine(INFO_TEMP_FILE, "cat /tmp/tmmsg | grep \"TM 0 not ready\"", NULL, NULL, ' '))
	{
		strcat(info_text, LANG_TM0_STATUS ": " LANG_FAIL "\n");
	} else
	{
		strcat(info_text, LANG_TM0_STATUS ": " LANG_OK "\n");
	}

	info_progress++;
	interface_displayMenu(1);

	if (helperParseLine(INFO_TEMP_FILE, "cat /tmp/dp/dp1init | grep \"\\$Build: \"", "$Build: ", buf, '$'))
	{
		strcat(info_text, LANG_TM1_IMAGE ": ");
		strcat(info_text, buf);
		strcat(info_text, "\n");
	} else
	{
		strcat(info_text, LANG_TM1_IMAGE ": " LANG_NOT_AVAILABLE_SHORT "\n");
	}

	info_progress++;
	interface_displayMenu(1);

	if (helperParseLine(INFO_TEMP_FILE, "cat /tmp/tm1dp | grep \"CPU RSE\"", NULL, NULL, ' ') ||
		helperParseLine(INFO_TEMP_FILE, "cat /tmp/tmmsg | grep \"unable to communicate with TriMedia 1\"", NULL, NULL, ' ') ||
		helperParseLine(INFO_TEMP_FILE, "cat /tmp/tmmsg | grep \"TM 1 not ready\"", NULL, NULL, ' '))
	{
		strcat(info_text, LANG_TM1_STATUS ": " LANG_FAIL "\n");
	} else
	{
		strcat(info_text, LANG_TM1_STATUS ": " LANG_OK "\n");
	}

	info_progress++;
	interface_displayMenu(1);

	sprintf(buf, "cat /proc/%s/drivers/pnx8550/video/renderer0/output_resolution", vendor);
	if (helperParseLine(INFO_TEMP_FILE, buf, NULL, buf, ' '))
	{
		strcat(info_text, LANG_RESOLUTION ": ");
		strcat(info_text, buf);
	} else
	{
		strcat(info_text, LANG_RESOLUTION ": " LANG_NOT_AVAILABLE_SHORT);
	}
#endif // #if 0

#else
	sprintf(info_text, _T("NOT_AVAILABLE"));
#endif // #ifdef STBxx

	info_progress++;
	interface_displayMenu(1);

	interface_sliderShow(0, 0);

	eprintf("output: Done collecting info.\n---------------------------------------------------\n%s\n---------------------------------------------------\n", info_text);

	helperFlushEvents();

	interface_showScrollingBox( info_text, thumbnail_info, NULL, NULL );
	//interface_showMessageBox(info_text, thumbnail_info, 0);

	return 0;
}

long output_getColorValue(void *pArg)
{
	int iarg = (int)pArg;
	DFBColorAdjustment adj;
	IDirectFBDisplayLayer *layer = gfx_getLayer(gfx_getMainVideoLayer());

	adj.flags = DCAF_ALL;
	layer->GetColorAdjustment(layer, &adj);

	switch (iarg)
	{
		case colorSettingContrast:
			return adj.contrast;
		case colorSettingBrightness:
			return adj.brightness;
		case colorSettingHue:
			return adj.hue;
		default:
			return adj.saturation;
	}

	return 0;
}


void output_setColorValue(long value, void *pArg)
{
	int iarg = (int)pArg;
	DFBColorAdjustment adj;
	IDirectFBDisplayLayer *layer = gfx_getLayer(gfx_getMainVideoLayer());

	/*adj.flags = DCAF_ALL;
	layer->GetColorAdjustment(layer, &adj);*/

	switch (iarg)
	{
		case colorSettingContrast:
			adj.flags = DCAF_CONTRAST;
			adj.contrast = value;
			appControlInfo.pictureInfo.contrast = value;
			break;
		case colorSettingBrightness:
			adj.flags = DCAF_BRIGHTNESS;
			adj.brightness = value;
			appControlInfo.pictureInfo.brightness = value;
			break;
		case colorSettingHue:
			adj.flags = DCAF_HUE;
			adj.hue = value;
			break;
		default:
			adj.flags = DCAF_SATURATION;
			adj.saturation = value;
			appControlInfo.pictureInfo.saturation = value;
	}

	layer->SetColorAdjustment(layer, &adj);
}

int output_showColorSlider(interfaceMenu_t *pMenu, void* pArg)
{
	if(interfacePlayControl.activeButton == interfacePlayControlStop && appControlInfo.slideshowInfo.state == slideshowDisabled && appControlInfo.slideshowInfo.showingCover == 0)
	{
		gfx_decode_and_render_Image_to_layer(IMAGE_DIR "wallpaper_test.png", screenMain);
		gfx_showImage(screenMain);
	}
	interface_playControlDisable(0);

	interface_showMenu(0, 0);
	output_colorSliderUpdate(pArg);

	return 0;
}

int output_colorCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int iarg = (int)pArg;

	switch( cmd->command )
	{
		case interfaceCommandOk:
		case interfaceCommandEnter:
		case interfaceCommandUp:
		case interfaceCommandYellow:
		case interfaceCommandBlue:
			iarg = (iarg + 1) % colorSettingCount;
			output_colorSliderUpdate((void*)iarg);
			return 0;
		case interfaceCommandDown:
			iarg = (iarg + colorSettingCount - 1) % colorSettingCount;
			output_colorSliderUpdate((void*)iarg);
			return 0;
		case interfaceCommandBack:
		case interfaceCommandRed:
		case interfaceCommandGreen:
			cmd->command = interfaceCommandExit;
			interface_processCommand(cmd);
			return 1;
		case interfaceCommandExit:
			interfacePlayControl.enabled = 1;
			if (saveAppSettings() != 0 && bDisplayedWarning == 0)
			{
				bDisplayedWarning = 1;
				interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
			}
			//dprintf("%s: b=%d h=%d s=%d\n", __FUNCTION__, appCon);
			return 0;
		default:
			return 0;
	}
}

static void output_colorSliderUpdate(void *pArg)
{
	int iarg = (int)pArg;
	char *param;
	char installationString[2048];

	switch (iarg)
	{
		case colorSettingContrast:
			param = _T("CONTRAST");
			break;
		case colorSettingBrightness:
			param = _T("BRIGHTNESS");
			break;
		case colorSettingHue:
			param = _T("HUE");
			break;
		default:
			iarg = colorSettingSaturation;
			param = _T("SATURATION");
	}

	sprintf(installationString, "%s", param);

	interface_sliderSetText(installationString);
	interface_sliderSetMinValue(0);
	interface_sliderSetMaxValue(0xFFFF);
	interface_sliderSetCallbacks(output_getColorValue, output_setColorValue, pArg);
	interface_sliderSetDivisions(COLOR_STEP_COUNT);
	interface_sliderSetHideDelay(10);
	interface_sliderSetKeyCallback(output_colorCallback);
	interface_sliderShow(2, 1);
}

#ifdef ENABLE_PASSWORD
static int output_askPassword(interfaceMenu_t *pMenu, void* pArg)
{
	char passwd[MAX_PASSWORD_LENGTH];
	FILE *file = NULL;
	menuActionFunction pAction = (menuActionFunction)pArg;

	file = popen("hwconfigManager a -1 PASSWORD 2>/dev/null | grep \"VALUE:\" | sed 's/VALUE: \\(.*\\)/\\1/'","r");
	if( file != NULL )
	{
		if ( fgets(passwd, MAX_PASSWORD_LENGTH, file) != NULL && passwd[0] != 0 && passwd[0] != '\n' )
		{
			fclose(file);
			interface_getText(pMenu, _T("ENTER_PASSWORD"), "\\w+", output_enterPassword, NULL, inputModeDirect, pArg);
			return 0;
		}
	}

	return pAction(pMenu, NULL);
}

static int output_enterPassword(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char passwd[MAX_PASSWORD_LENGTH];
	unsigned char passwdsum[16];
	char inpasswd[MAX_PASSWORD_LENGTH];
	FILE *file;
	unsigned long id, i, intpwd;
	menuActionFunction pAction = (menuActionFunction)pArg;

	if( value == NULL )
		return 1;

	if( pArg != NULL)
	{
		file = popen("hwconfigManager a -1 PASSWORD 2>/dev/null | grep \"VALUE:\" | sed 's/VALUE: \\(.*\\)/\\1/'","r");
		if( file != NULL && fgets(passwd, MAX_PASSWORD_LENGTH, file) != NULL && passwd[0] != 0 && passwd[0] != '\n' )
		{
			fclose(file);
			if( passwd[strlen(passwd)-1] == '\n')
			{
				passwd[strlen(passwd)-1] = 0;
			}
			/* Get MD5 sum of input and convert it to hex string */
			md5((unsigned char*)value, strlen(value), passwdsum);
			for (i=0;i<16;i++)
			{
				sprintf(&inpasswd[i*2], "%02hhx", passwdsum[i]);
			}
			dprintf("%s: Passwd #1: %s/%s\n", __FUNCTION__,passwd, inpasswd);
			if(strcmp( passwd, inpasswd ) != 0)
			{
				interface_showMessageBox(_T("ERR_WRONG_PASSWORD"), thumbnail_error, 3000);
				return 1;
			}
		} else
		{
			if( file != NULL)
			{
				fclose(file);
			}
			file = popen("cat /dev/sysid | sed 's/.*SERNO: \\(.*\\), .*/\\1/'","r");
			if( file != NULL && fgets(passwd, MAX_PASSWORD_LENGTH, file) != NULL && passwd[0] != 0 && passwd[0] != '\n' )
			{
				fclose(file);
				if( passwd[strlen(passwd)-1] == '\n')
				{
					passwd[strlen(passwd)-1] = 0;
				}
				id = strtoul(passwd, NULL, 16);
				intpwd = strtoul(value, NULL, 10);
				dprintf("%s: Passwd #2: %lu/%lu\n", __FUNCTION__, id, intpwd);

				if( intpwd != id )
				{
					interface_showMessageBox(_T("ERR_WRONG_PASSWORD"), thumbnail_error, 3000);
					return 1;
				}
			} else
			{
				if( file != NULL)
				{
					fclose(file);
				}
				dprintf("%s: Warning: can't determine system password!\n", __FUNCTION__);
			}
		}
		return pAction(pMenu, NULL);
	}
	return 1;
}
#endif

int output_fillVideoMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
	char buf[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&VideoSubMenu);

	if (gfx_tvStandardSelectable())
    {
		str = _T("TV_STANDARD");
		interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&StandardMenu, thumbnail_tvstandard);
        //menuInfra_setEntry(&VideoSubMenu, position++, menuEntryText, "TV Standard", menuInfra_display, (void*)&StandardMenu);
    }
    /* We only enable this menu when we are outputting SD and we do not only have the HD denc. (HDMI is not denc[0])*/
    if(!(gfx_isHDoutput()) && !(appControlInfo.outputInfo.encDesc[0].all_connectors & DSOC_HDMI))
    {
		str = _T("TV_FORMAT");
		interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&FormatMenu, thumbnail_channels);
        //menuInfra_setEntry(&VideoSubMenu, position++, menuEntryText, "Format", menuInfra_display, (void*)&FormatMenu);
        /*Only add slow blanking if we have the capability*/
        if(appControlInfo.outputInfo.encDesc[0].caps & DSOCAPS_SLOW_BLANKING)
        {
			str = _T("TV_BLANKING");
			interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&BlankingMenu, thumbnail_configure);
            //menuInfra_setEntry(&VideoSubMenu, position++, menuEntryText, "Blanking", menuInfra_display, (void*)&BlankingMenu);
            output_fillBlankingMenu();
        }
    }

	str = appControlInfo.outputInfo.aspectRatio == aspectRatio_16x9 ? "16:9" : "4:3";
	sprintf(buf, "%s: %s", _T("ASPECT_RATIO"), str);
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, buf, output_toggleAspectRatio, NULL, thumbnail_channels);

	str = appControlInfo.outputInfo.autoScale == videoMode_scale ? _T("AUTO_SCALE_ENABLED") :
		_T("AUTO_STRETCH_ENABLED");
	sprintf(buf, "%s: %s", _T("SCALE_MODE"), str);
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, buf, output_toggleAutoScale, NULL, settings_size);
    /*menuInfra_setEntry(&VideoSubMenu, position++, menuEntryText,
                       appControlInfo.outputInfo.autoScale == videoMode_native ? "Native Resolution" :
                       appControlInfo.outputInfo.autoScale == videoMode_scale ? "Auto Scale Enabled" :
                       "Auto Stretch Enabled", output_toggleAutoScale, NULL);
    menuInfra_setSelected(&VideoSubMenu, NONE_SELECTED);*/
#ifndef HIDE_EXTRA_FUNCTIONS
	sprintf(buf, "%s: %s", _T("AUDIO_OUTPUT"), appControlInfo.soundInfo.rcaOutput == 0 ? "SCART" : "RCA");
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, buf, output_toggleAudio, NULL, thumbnail_sound);
#endif
	sprintf(buf, "%s: %s", _T("SCREEN_FILTRATION"), _T( appControlInfo.outputInfo.bScreenFiltration ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, buf, output_toggleScreenFiltration, NULL, thumbnail_channels);

	str = _T("COLOR_SETTINGS");
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, str, output_showColorSlider, NULL, thumbnail_tvstandard);

#ifdef STB225
	str = _T("3D_SETTINGS");
	interface_addMenuEntry((interfaceMenu_t*)&VideoSubMenu, str, output_fill3DMenu, NULL, thumbnail_channels);
#endif // #ifdef STB225
	interface_menuActionShowMenu(pMenu, (void*)&VideoSubMenu);

	return 0;
}



#ifdef STB225
static int output_toggle3DMonitor(interfaceMenu_t *pMenu, void* pArg) {
	appControlInfo.outputInfo.has_3D_TV = (appControlInfo.outputInfo.has_3D_TV+1) & 1;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	if(interfaceInfo.mode3D==0 || appControlInfo.outputInfo.has_3D_TV==0) {
		Stb225ChangeDestRect("/dev/fb0", 0, 0, 1920, 1080);
	} else	{
		Stb225ChangeDestRect("/dev/fb0", 0, 0, 960, 1080);
	}

	output_fill3DMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}
static int output_toggle3DContent(interfaceMenu_t *pMenu, void* pArg) {
	appControlInfo.outputInfo.content3d = (appControlInfo.outputInfo.content3d + 1) % 3;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	output_fill3DMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}
static int output_toggle3DFormat(interfaceMenu_t *pMenu, void* pArg) {
	appControlInfo.outputInfo.format3d = (appControlInfo.outputInfo.format3d + 1) % 3;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	output_fill3DMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}
static int output_toggleUseFactor(interfaceMenu_t *pMenu, void* pArg) {
	appControlInfo.outputInfo.use_factor = (appControlInfo.outputInfo.use_factor + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	output_fill3DMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}
static int output_toggleUseOffset(interfaceMenu_t *pMenu, void* pArg) {
	appControlInfo.outputInfo.use_offset= (appControlInfo.outputInfo.use_offset + 1) % 2;

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}
	output_fill3DMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}

char *output_get3DFactor(int index, void* pArg) {
	if( index == 0 ) {
		static char temp[8];
		sprintf(temp, "%03d", appControlInfo.outputInfo.factor);
		return temp;
	} else	return NULL;
}
static int output_change3DFactor(interfaceMenu_t *pMenu, char *value, void* pArg) {
	if( value != NULL && value[0] != 0)  {
		int ivalue = atoi(value);
		if (ivalue < 0 || ivalue > 255) ivalue = 255;

		appControlInfo.outputInfo.factor = ivalue;
	}
	output_fill3DMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}
char *output_get3DOffset(int index, void* pArg) {
	if( index == 0 ) {
		static char temp[8];
		sprintf(temp, "%03d", appControlInfo.outputInfo.offset);
		return temp;
	} else	return NULL;
}
static int output_change3DOffset(interfaceMenu_t *pMenu, char *value, void* pArg) {
	if( value != NULL && value[0] != 0)  {
		int ivalue = atoi(value);
		if (ivalue < 0 || ivalue > 255) ivalue = 255;

		appControlInfo.outputInfo.offset = ivalue;
	}
	output_fill3DMenu(pMenu, 0);
	interface_displayMenu(1);

	return 0;
}

static int output_toggle3DFactor(interfaceMenu_t *pMenu, void* pArg) {
	return interface_getText(pMenu, _T("3D_FACTOR"), "\\d{3}", output_change3DFactor, output_get3DFactor, inputModeDirect, pArg);
}
static int output_toggle3DOffset(interfaceMenu_t *pMenu, void* pArg) {
	return interface_getText(pMenu, _T("3D_OFFSET"), "\\d{3}", output_change3DOffset, output_get3DOffset, inputModeDirect, pArg);
}



int output_fill3DMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
	char buf[MENU_ENTRY_INFO_LENGTH];
	
	char *chContent[] = {_T("VIDEO"), _T("3D_SIGNAGE"), _T("3D_STILLS")};
	char *chFormat [] = {_T("3D_2dZ"), _T("3D_DECLIPSE_RD"), _T("3D_DECLIPSE_FULL")};

	interface_clearMenuEntries((interfaceMenu_t*)&Video3DSubMenu);

	sprintf(buf, "%s: %s", _T("3D_MONITOR"), _T( appControlInfo.outputInfo.has_3D_TV ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggle3DMonitor, NULL, thumbnail_channels);

	str = chContent[appControlInfo.outputInfo.content3d];
	sprintf(buf, "%s: %s", _T("3D_CONTENT"), str);
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggle3DContent, NULL, thumbnail_channels);

	str = chFormat[appControlInfo.outputInfo.format3d];
	sprintf(buf, "%s: %s", _T("3D_FORMAT"), str);
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggle3DFormat, NULL, thumbnail_channels);

	sprintf(buf, "%s: %s", _T("3D_FACTOR_FLAG"), _T( appControlInfo.outputInfo.use_factor ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggleUseFactor, NULL, thumbnail_channels);

	sprintf(buf, "%s: %s", _T("3D_OFFSET_FLAG"), _T( appControlInfo.outputInfo.use_offset ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggleUseOffset, NULL, thumbnail_channels);

	sprintf(buf, "%s: %d", _T("3D_FACTOR"), appControlInfo.outputInfo.factor);
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggle3DFactor, NULL, thumbnail_channels);

	sprintf(buf, "%s: %d", _T("3D_OFFSET"), appControlInfo.outputInfo.offset);
	interface_addMenuEntry((interfaceMenu_t*)&Video3DSubMenu, buf, output_toggle3DOffset, NULL, thumbnail_channels);

	interface_menuActionShowMenu(pMenu, (void*)&Video3DSubMenu);

	return 0;
}
#endif // #ifdef STB225

static int output_resetTimeEdit(interfaceMenu_t *pMenu, void* pArg)
{
	interfaceEditEntry_t *pEditEntry = (interfaceEditEntry_t *)pArg;
	return output_fillTimeMenu( pMenu, pEditEntry->pArg );
}

int output_fillTimeMenu(interfaceMenu_t *pMenu, void* pArg)
{
	struct tm *t;
	time_t now;
	char   buf[BUFFER_SIZE], *str;

	interface_clearMenuEntries((interfaceMenu_t*)&TimeSubMenu);

	time(&now);
	t = localtime(&now);
	strftime( TimeEntry.info.time.value, sizeof(TimeEntry.info.time.value), "%H%M", t);
	interface_addEditEntryTime((interfaceMenu_t *)&TimeSubMenu, _T("SET_TIME"), output_setTime, output_resetTimeEdit, pArg, thumbnail_log, &TimeEntry);
	strftime( DateEntry.info.date.value, sizeof(DateEntry.info.date.value), "%d%m%Y", t);
	interface_addEditEntryDate((interfaceMenu_t *)&TimeSubMenu, _T("SET_DATE"), output_setDate, output_resetTimeEdit, pArg, thumbnail_log, &DateEntry);

	//interface_addMenuEntry((interfaceMenu_t*)&TimeSubMenu, _T("SET_TIME"), output_changeTime, NULL, thumbnail_log);
	interface_addMenuEntry((interfaceMenu_t*)&TimeSubMenu, _T("SET_TIME_ZONE"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&TimeZoneMenu, thumbnail_log);
	//interface_addMenuEntry((interfaceMenu_t*)&TimeSubMenu, _T("SET_DATE"), output_changeDate, NULL, thumbnail_log);

	sprintf(buf, "%s: ", _T("NTP_SERVER"));
	str = &buf[strlen(buf)];
	strcpy( str, output_getNTP( 0, NULL ) );
	if ( *str == 0 )
		strcpy(str, _T("NOT_AVAILABLE_SHORT") );
	interface_addMenuEntry((interfaceMenu_t*)&TimeSubMenu, buf, output_setNTP, NULL, thumbnail_enterurl);

	interface_menuActionShowMenu(pMenu, (void*)&TimeSubMenu);

	return 0;
}

int output_fillNetworkMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char temp[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&NetworkSubMenu);

	sprintf(temp, "/sys/class/net/%s", helperEthDevice(ifaceWAN));
	if( helperCheckDirectoryExsists(temp) )
	{
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, "WAN", (menuActionFunction)menuDefaultActionShowMenu, (void*)&WANSubMenu, settings_network);
		sprintf(path, "/config/ifcfg-%s", helperEthDevice(ifaceWAN));
#ifdef ENABLE_LAN
		if (!helperFileExists(STB_CONFIG_OVERRIDE_FILE))
		{
			getParam(STB_CONFIG_FILE, "CONFIG_GATEWAY_MODE", "OFF", temp);
			if (strcmp("NAT", temp) == 0)
			{
				output_gatewayMode = gatewayModeNAT;
			} else if (strcmp("FULL", temp) == 0)
			{
				output_gatewayMode = gatewayModeFull;
			} else if (strcmp("BRIDGE", temp) == 0)
			{
				output_gatewayMode = gatewayModeBridge;
			} else
			{
				output_gatewayMode = gatewayModeOff;
			}
		}
#endif
	} else
	{
		interface_addMenuEntryDisabled((interfaceMenu_t*)&NetworkSubMenu, "WAN", settings_network);
	}
#ifdef ENABLE_PPP
	interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("PPP"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&PPPSubMenu, settings_network);
#endif
#ifdef ENABLE_LAN
	sprintf(temp, "/sys/class/net/%s", helperEthDevice(ifaceLAN));
	if( helperCheckDirectoryExsists(temp) )
	{
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, "LAN", (menuActionFunction)menuDefaultActionShowMenu, (void*)&LANSubMenu, settings_network);
	} else
	{
		interface_addMenuEntryDisabled((interfaceMenu_t*)&NetworkSubMenu, "LAN", settings_network);
	}
#endif
#ifdef ENABLE_WIFI
#if !(defined STB225)
	sprintf(temp, "/sys/class/net/%s", helperEthDevice(ifaceWireless));
	if( helperCheckDirectoryExsists(temp) )
	{
#endif
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("WIRELESS"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&WifiSubMenu, settings_network);
#if !(defined STB225)
	} else
	{
		interface_addMenuEntryDisabled((interfaceMenu_t*)&NetworkSubMenu, _T("WIRELESS"), settings_network);
	}
#endif
#endif
#ifdef ENABLE_IPTV
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("TV_CHANNELS"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&IPTVSubMenu, thumbnail_multicast);
#endif
#ifdef ENABLE_VOD
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("MOVIES"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&VODSubMenu, thumbnail_vod);
#endif

	if (helperFileExists(BROWSER_CONFIG_FILE))
	{
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("INTERNET_BROWSING"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&WebSubMenu, thumbnail_internet);
	}
#ifdef ENABLE_VERIMATRIX
	if (helperFileExists(VERIMATRIX_INI_FILE))
	{
		sprintf(path,"%s: %s", _T("VERIMATRIX_ENABLE"), appControlInfo.useVerimatrix == 0 ? _T("OFF") : _T("ON"));
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleVMEnable, NULL, thumbnail_configure);
		if (appControlInfo.useVerimatrix != 0)
		{
			getParam(VERIMATRIX_INI_FILE, "COMPANY", "", temp);
			if (temp[0] != 0)
			{
				sprintf(path, "%s: %s", _T("VERIMATRIX_COMPANY"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleVMCompany, pArg, thumbnail_enterurl);
			}
			getParam(VERIMATRIX_INI_FILE, "SERVERADDRESS", "", temp);
			if (temp[0] != 0)
			{
				sprintf(path, "%s: %s", _T("VERIMATRIX_ADDRESS"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleVMAddress, pArg, thumbnail_enterurl);
			}
			interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, _T("VERIMATRIX_GET_ROOTCERT"), output_getVMRootCert, NULL, thumbnail_turnaround);
		}
	}
#endif
#ifdef ENABLE_SECUREMEDIA
	if (helperFileExists(SECUREMEDIA_CONFIG_FILE))
	{
		sprintf(path,"%s: %s", _T("SECUREMEDIA_ENABLE"), appControlInfo.useSecureMedia == 0 ? _T("OFF") : _T("ON"));
		interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleSMEnable, NULL, thumbnail_configure);
		if (appControlInfo.useSecureMedia != 0)
		{
			getParam(SECUREMEDIA_CONFIG_FILE, "SECUREMEDIA_ESAM_HOST", "", temp);
			if (temp[0] != 0)
			{
				sprintf(path, "%s: %s", _T("SECUREMEDIA_ESAM_HOST"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleSMAddress, (void*)1, thumbnail_enterurl);
			}
			getParam(SECUREMEDIA_CONFIG_FILE, "SECUREMEDIA_RANDOM_HOST", "", temp);
			if (temp[0] != 0)
			{
				sprintf(path, "%s: %s", _T("SECUREMEDIA_RANDOM_HOST"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, path, output_toggleSMAddress, (void*)2, thumbnail_enterurl);
			}
		}
	}
#endif
//HTTPProxyServer=http://192.168.1.2:3128
//http://192.168.1.57:8080/media/stb/home.html
#ifndef HIDE_EXTRA_FUNCTIONS
	str = _T("PROCESS_PCR");
	interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, str, output_togglePCR, NULL, appControlInfo.bProcessPCR ? thumbnail_yes : thumbnail_no);
	str = _T( appControlInfo.bUseBufferModel ? "BUFFER_TRACKING" : "PCR_TRACKING");
	interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, str, output_toggleBufferTracking, NULL, thumbnail_configure);
	str = _T("RENDERER_SYNC");
	interface_addMenuEntry((interfaceMenu_t*)&NetworkSubMenu, str, output_toggleRSync, NULL, appControlInfo.bRendererDisableSync ? thumbnail_no : thumbnail_yes);
#endif

	interface_menuActionShowMenu(pMenu, (void*)&NetworkSubMenu);

	return 0;
}

static int output_fillWANMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char buf[MENU_ENTRY_INFO_LENGTH];
	char temp[MENU_ENTRY_INFO_LENGTH];
	int dhcp;

	interface_clearMenuEntries((interfaceMenu_t*)&WANSubMenu);

	const int i = ifaceWAN;
	sprintf(temp, "/sys/class/net/%s", helperEthDevice(i));
	if( helperCheckDirectoryExsists(temp) )
	{
		sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

		getParam(path, "BOOTPROTO", _T("NOT_AVAILABLE_SHORT"), temp);
		if (strcmp("dhcp+dns", temp) == 0)
		{
			strcpy(temp, _T("ADDR_MODE_DHCP"));
			dhcp = 1;
		} else
		{
			strcpy(temp, _T("ADDR_MODE_STATIC"));
			dhcp = 0;
		}
		sprintf(buf, "%s: %s", _T("ADDR_MODE"), temp);
		interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleMode, (void*)i, thumbnail_configure);

		if (dhcp == 0)
		{
			getParam(path, "IPADDR", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s: %s",  _T("IP_ADDRESS"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleIP, (void*)i, thumbnail_configure);

			getParam(path, "NETMASK", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s: %s", _T("NETMASK"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleNetmask, (void*)i, thumbnail_configure);

			getParam(path, "DEFAULT_GATEWAY", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s: %s", _T("GATEWAY"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleGw, (void*)i, thumbnail_configure);

			getParam(path, "NAMESERVERS", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s: %s", _T("DNS_SERVER"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleDNSIP, (void*)i, thumbnail_configure);
		} else
		{
			sprintf(path, "ifconfig %s | grep \"inet addr\"", helperEthDevice(i));
			if (!helperParseLine(INFO_TEMP_FILE, path, "inet addr:", temp, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				strcpy(temp, _T("NOT_AVAILABLE_SHORT"));
			}
			sprintf(buf, "%s: %s", _T("IP_ADDRESS"), temp);
			interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);

			sprintf(path, "ifconfig %s | grep \"Mask:\"", helperEthDevice(i));
			if (!helperParseLine(INFO_TEMP_FILE, path, "Mask:", temp, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				strcpy(temp, _T("NOT_AVAILABLE_SHORT"));
			}
			sprintf(buf, "%s: %s", _T("NETMASK"), temp);
			interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);

			sprintf(path, "route -n | grep -e \"0\\.0\\.0\\.0 .* 0\\.0\\.0\\.0 *UG .* %s\"", helperEthDevice(i));
			if (!helperParseLine(INFO_TEMP_FILE, path, "0.0.0.0", temp, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
			{
				strcpy(temp, _T("NOT_AVAILABLE_SHORT"));
			}
			sprintf(buf, "%s: %s", _T("GATEWAY"), temp);
			interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);

			int found = -1;
			int fd = open( "/etc/resolv.conf", O_RDONLY );
			if( fd > 0 )
			{
				char *ptr;
				while( helperReadLine( fd, temp ) == 0 && temp[0] )
				{
					if( (ptr = strstr( temp, "nameserver " )) != NULL )
					{
						ptr += 11;
						found++;
						sprintf(buf, "%s %d: %s", _T("DNS_SERVER"), found+1, ptr);
						interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);
					}
				}
				close(fd);
			}
			if( found < 0 )
			{
				sprintf(buf, "%s: %s", _T("DNS_SERVER"), _T("NOT_AVAILABLE_SHORT"));
				interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);
			}
		}
		/*sprintf(path, "ifconfig %s | grep HWaddr", helperEthDevice(i));
		if (!helperParseLine(INFO_TEMP_FILE, path, "HWaddr ", temp, ' '))			 // eth0      Link encap:Ethernet  HWaddr 76:60:37:02:24:02
		{
			strcpy(temp, _T("NOT_AVAILABLE_SHORT"));
		}
		sprintf(buf, "%s: %s",  _T("MAC_ADDRESS"), temp);
		interface_addMenuEntryDisabled((interfaceMenu_t*)&WANSubMenu, buf, thumbnail_configure);
		*/

		sprintf(buf, "%s", _T("NET_RESET"));
		interface_addMenuEntry((interfaceMenu_t*)&WANSubMenu, buf, output_toggleReset, (void*)i, settings_renew);

		return 0;
	}

	return 1;
}

#ifdef ENABLE_PPP
static char* output_getPPPPassword(int field, void* pArg)
{
	if( field == 0 )
		return pppInfo.password;

	return NULL;
}

static char* output_getPPPLogin(int field, void* pArg)
{
	if( field == 0 )
		return pppInfo.login;

	return NULL;
}

static int output_setPPPPassword(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if (value == NULL )
		return 1;

	FILE *f;

	strcpy( pppInfo.password, value );

	f = fopen("/etc/ppp/chap-secrets", "w");
	if( f != NULL )
	{
		fprintf( f, "%s pppoe %s *\n", pppInfo.login, pppInfo.password );
		fclose(f);
	} else
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
		return 0;
	}

	output_fillPPPMenu(pMenu, pArg);

	return 0;
}

static int output_togglePPPPassword(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText( pMenu, _T("PASSWORD"), "\\w+", output_setPPPPassword, output_getPPPPassword, inputModeABC, NULL );

	return 0;
}

static int output_setPPPLogin(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if (value == NULL )
		return 1;

	if( value[0] == 0 )
	{
		system("rm -f /etc/ppp/chap-secrets");
		output_fillPPPMenu(pMenu, pArg);
		return 0;
	}

	strcpy(pppInfo.login, value);

	output_togglePPPPassword(pMenu, pArg);

	return 1;
}

static int output_togglePPPLogin(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText( pMenu, _T("LOGIN"), "\\w+", output_setPPPLogin, output_getPPPLogin, inputModeABC, NULL );
	return 0;
}

static void* output_checkPPPThread(void *pArg)
{
	for(;;)
	{
		sleep(2);
		output_fillPPPMenu(interfaceInfo.currentMenu, pArg);
		interface_displayMenu(1);
	}
	pthread_exit(NULL);
}

static int output_togglePPP(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showMessageBox(_T("RENEW_IN_PROGRESS"), settings_renew, 0);

	system("/etc/init.d/S65ppp stop");
	system("/etc/init.d/S65ppp start");

	output_fillPPPMenu( pMenu, pArg );

	interface_hideMessageBox();

	if( pppInfo.check_thread == 0 )
		pthread_create( &pppInfo.check_thread, NULL, output_checkPPPThread, NULL );

	return 0;
}

static int output_leavePPPMenu(interfaceMenu_t *pMenu, void* pArg)
{
	if( pppInfo.check_thread != 0 )
	{
		pthread_cancel( pppInfo.check_thread );
		pthread_join( pppInfo.check_thread, NULL );
		pppInfo.check_thread = 0;
	}
	return 0;
}

static int output_fillPPPMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char *str;
	pMenu = (interfaceMenu_t*)&PPPSubMenu;

	interface_clearMenuEntries(pMenu);

	FILE *f;
	f = fopen("/etc/ppp/chap-secrets", "r");
	if( f != NULL )
	{
		fgets( buf, sizeof(buf), f );
		if( sscanf( buf, "%s pppoe %s *", pppInfo.login, pppInfo.password ) != 2 )
		{
			pppInfo.login[0] = 0;
			pppInfo.password[0] = 0;
		}
		fclose(f);
	} else
	{
		pppInfo.login[0] = 0;
		pppInfo.password[0] = 0;
	}

	snprintf(buf, sizeof(buf), "%s: %s", _T("PPP_TYPE"), "PPPoE");
	interface_addMenuEntryDisabled(pMenu, buf, thumbnail_configure);

	snprintf(buf, sizeof(buf), "%s: %s", _T("LOGIN"), pppInfo.login[0] ? pppInfo.login : _T("OFF") );
	interface_addMenuEntry(pMenu, buf, output_togglePPPLogin, NULL, thumbnail_enterurl);

	if( pppInfo.login[0] != 0 )
	{
		snprintf(buf, sizeof(buf), "%s: ***", _T("PASSWORD"));
		interface_addMenuEntry(pMenu, buf, output_togglePPPPassword, NULL, thumbnail_enterurl);
	}

	interface_addMenuEntry(pMenu, _T("NET_RESET"), output_togglePPP, NULL, settings_renew);

	if( helperCheckDirectoryExsists( "/sys/class/net/ppp0" ) )
	{
		str = _T("ON");
	} else
	{
		int res;
		res = system("killall -0 pppd 2> /dev/null");
		if( WIFEXITED(res) == 1 && WEXITSTATUS(res) == 0 )
		{
			str = _T("CONNECTING");
			if( pppInfo.check_thread == 0 )
				pthread_create( &pppInfo.check_thread, NULL, output_checkPPPThread, NULL );
		}
		else
			str = _T("OFF");
	}
	snprintf(buf, sizeof(buf), "%s: %s", _T("PPP"), str);
	interface_addMenuEntryDisabled(pMenu, buf, thumbnail_info);

	return 0;
}
#endif

#ifdef ENABLE_LAN
static int output_fillLANMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char buf[MENU_ENTRY_INFO_LENGTH];
	char temp[MENU_ENTRY_INFO_LENGTH];
	char *str;

	const int i = ifaceLAN;
	sprintf(path, "/sys/class/net/%s", helperEthDevice(i));
	if( helperCheckDirectoryExsists(path) )
	{
		interface_clearMenuEntries((interfaceMenu_t*)&LANSubMenu);
		sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

		getParam(path, "IPADDR", _T("NOT_AVAILABLE_SHORT"), temp);
		sprintf(buf, "%s: %s", _T("IP_ADDRESS"), temp);
		interface_addMenuEntry((interfaceMenu_t*)&LANSubMenu, buf, output_toggleIP, (void*)i, thumbnail_configure);

		sprintf(buf, "%s", _T("NET_RESET"));
		interface_addMenuEntry((interfaceMenu_t*)&LANSubMenu, buf, output_toggleReset, (void*)i, settings_renew);

		if (!helperFileExists(STB_CONFIG_OVERRIDE_FILE))
		{
			switch( output_gatewayMode )
			{
				case gatewayModeNAT:    str = _T("GATEWAY_NAT_ONLY"); break;
				case gatewayModeFull:   str = _T("GATEWAY_FULL"); break;
				case gatewayModeBridge: str = _T("GATEWAY_BRIDGE"); break;
				default:                str = _T("OFF");
			}
			sprintf(buf,"%s: %s", _T("GATEWAY_MODE"), str);
			interface_addMenuEntry((interfaceMenu_t*)&LANSubMenu, buf, output_fillGatewayMenu, (void*)0, thumbnail_configure);
			if (output_gatewayMode != gatewayModeOff)
			{
				getParam(STB_CONFIG_FILE, "CONFIG_TRAFFIC_SHAPE", "0", temp);
				sprintf(buf,"%s: %s %s", _T("GATEWAY_BANDWIDTH"), atoi(temp) <= 0 ? _T("NONE") : temp, atoi(temp) <= 0 ? "" : _T("KBPS"));
				interface_addMenuEntry((interfaceMenu_t*)&LANSubMenu, buf, output_toggleGatewayBW, (void*)0, thumbnail_configure);
			}
		}

		return 0;
	}

	return 1;
}
#endif

#ifdef ENABLE_WIFI
static int output_fillWifiMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char path[MAX_CONFIG_PATH];
	char buf[MENU_ENTRY_INFO_LENGTH];
	char temp[MENU_ENTRY_INFO_LENGTH];
	char *iface_name;
	int exists;
	char *str;

	const int i = ifaceWireless;
	iface_name  = _T("WIRELESS");
#if !(defined STB225)
	sprintf(temp, "/sys/class/net/%s", helperEthDevice(i));
	exists = helperCheckDirectoryExsists(temp);
#else
	exists = 1; // no check
#endif
	if( exists )
	{
		interface_clearMenuEntries((interfaceMenu_t*)&WifiSubMenu);

		sprintf(path, "/config/ifcfg-%s", helperEthDevice(i));

		getParam(path, "WAN_MODE", "0", temp);
		wifiInfo.wanMode = strtol( temp, NULL, 10 );

		sprintf(buf, "%s: %s", iface_name, wifiInfo.wanMode ? "WAN" : "LAN" );
		interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleWifiWAN, (void*)i, thumbnail_configure);

		getParam(path, "MODE", "ad-hoc", temp);
		if( strcmp(temp, "managed") == 0 )
		{
			wifiInfo.mode = wifiModeManaged;
			str = "Infrastructure";
		}
		/*else if( strcmp(temp, "master") == 0 )
		{
			wifiInfo.mode = wifiModeMaster;
			str = "AP";
		}*/
		else
		{
			wifiInfo.mode = wifiModeAdHoc;
			str = "Ad-Hoc";
		}
		sprintf(buf, "%s %s: %s", iface_name, _T("MODE"), str);
		interface_addMenuEntryCustom((interfaceMenu_t*)&WifiSubMenu, interfaceMenuEntryText, buf, strlen(buf)+1, wifiInfo.wanMode, output_toggleWifiMode, NULL, NULL, NULL, (void*)i, thumbnail_configure);

		getParam(path, "BOOTPROTO", "static", temp);
		if (strcmp("dhcp+dns", temp) == 0)
		{
			strcpy(temp, _T("ADDR_MODE_DHCP"));
			wifiInfo.dhcp = 1;
		} else
		{
			strcpy(temp, _T("ADDR_MODE_STATIC"));
			wifiInfo.dhcp = 0;
		}
		sprintf(buf, "%s %s: %s", iface_name, _T("ADDR_MODE"), temp);
		interface_addMenuEntryCustom((interfaceMenu_t*)&WifiSubMenu, interfaceMenuEntryText, buf, strlen(buf)+1, wifiInfo.wanMode, output_toggleMode, NULL, NULL, NULL, (void*)i, thumbnail_configure);

		if( wifiInfo.dhcp == 0 || wifiInfo.wanMode == 0 )
		{
			getParam(path, "IPADDR", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s %s: %s", iface_name, _T("IP_ADDRESS"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleIP, (void*)i, thumbnail_configure);

			getParam(path, "NETMASK", _T("NOT_AVAILABLE_SHORT"), temp);
			sprintf(buf, "%s %s: %s", iface_name, _T("NETMASK"), temp);
			interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleNetmask, (void*)i, thumbnail_configure);
			
			if( wifiInfo.wanMode )
			{
				getParam(path, "DEFAULT_GATEWAY", _T("NOT_AVAILABLE_SHORT"), temp);
				sprintf(buf, "%s %s: %s", iface_name, _T("GATEWAY"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleGw, (void*)i, thumbnail_configure);

				getParam(path, "NAMESERVERS", _T("NOT_AVAILABLE_SHORT"), temp);
				sprintf(buf, "%s %s: %s", iface_name, _T("DNS_SERVER"), temp);
				interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleDNSIP, (void*)i, thumbnail_configure);
			}
		}

		getParam(path, "ESSID", _T("NOT_AVAILABLE_SHORT"), temp);
		sprintf(buf, "%s %s: %s", iface_name, _T("ESSID"), temp);
		interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleESSID, (void*)i, thumbnail_enterurl);

		getParam(path, "CHANNEL", "1", temp);
		wifiInfo.currentChannel = strtol( temp, NULL, 10 );

		wifiInfo.auth       = wifiAuthOpen;
		wifiInfo.encryption = wifiEncTKIP;

		getParam(path, "AUTH", "SHARED", temp);
		if( strcasecmp( temp, "WPAPSK" ) == 0 )
			wifiInfo.auth = wifiAuthWPAPSK;
		else if( strcasecmp( temp, "WPA2PSK" ) == 0 )
			wifiInfo.auth = wifiAuthWPA2PSK;

		getParam(path, "ENCRYPTION", "WEP", temp);
		if( strcasecmp( temp, "WEP" ) == 0 )
			wifiInfo.auth = wifiAuthWEP;
		else if ( strcasecmp( temp, "AES" ) == 0 )
			wifiInfo.encryption = wifiEncAES;

		getParam(path, "KEY", "", temp);
		memcpy( wifiInfo.key, temp, sizeof(wifiInfo.key)-1 );
		wifiInfo.key[sizeof(wifiInfo.key)-1] = 0;

#if (defined STB225)
		sprintf(buf, "/usr/sbin/iwlist %s channel > %s", helperEthDevice(i), INFO_TEMP_FILE);
#else
		sprintf(buf, "/usr/local/sbin/iwlist %s channel > %s", helperEthDevice(i), INFO_TEMP_FILE);
#endif
		system(buf);
		FILE* f = fopen( INFO_TEMP_FILE, "r" );
		if( f )
		{
			char *ptr;
			while( fgets(buf, sizeof(buf), f ) != NULL )
			{
				if( strncmp( buf, helperEthDevice(i), 5 ) == 0 )
				{
					// sample: 'wlan0     14 channels in total; available frequencies :'
					str = index(buf, ' ');
					while( str && *str == ' ' )
						str++;
					if( str )
					{
						ptr = index(str+1, ' ');
						if( ptr )
							*ptr++ = 0;

						wifiInfo.channelCount = strtol( str, NULL, 10 );
						if( wifiInfo.channelCount > 12 )
						{
							wifiInfo.channelCount = 12; // 13 and 14 are disallowed in Russia
						}
					}
				}
				/*else if( strstr( buf, "Current Frequency:" ) != NULL )
				{
					// sample: 'Current Frequency:2.412 GHz (Channel 1)'
					str = index(buf, '(');
					if( str )
					{
						str += 9;
						ptr = index(str, ')');
						if(ptr)
							*ptr = 0;
						wifiInfo.currentChannel = strtol( str, NULL, 10 );
					}
				}*/
			}
			fclose(f);
		} else
			eprintf("%s: failed to open %s\n", __FUNCTION__, INFO_TEMP_FILE);
		if( !wifiInfo.wanMode && wifiInfo.channelCount > 0 )
		{
			sprintf(buf, "%s %s: %d", iface_name, _T("CHANNEL_NUMBER"), wifiInfo.currentChannel);
			interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleWifiChannel, (void*)i, thumbnail_configure);
		}

		switch( wifiInfo.auth )
		{
			case wifiAuthOpen:    str = _T("NONE"); break;
			case wifiAuthWEP:     str = "WEP";      break;
			case wifiAuthWPAPSK:  str = "WPA-PSK";  break;
			case wifiAuthWPA2PSK: str = "WPA2-PSK"; break;
			default: str = _T("NONE_AVAILABLE_SHORT"); break;
		}
		sprintf(buf, "%s %s: %s", iface_name, _T("AUTHENTICATION"), str);
		interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleAuthMode, (void*)i, thumbnail_configure);
		if( wifiInfo.auth == wifiAuthWPAPSK || wifiInfo.auth == wifiAuthWPA2PSK )
		{
			switch( wifiInfo.encryption )
			{
				case wifiEncTKIP: str = "TKIP"; break;
				case wifiEncAES:  str = "AES";  break;
				default: str = _T("NONE_AVAILABLE_SHORT");
			}
			sprintf(buf, "%s %s: %s", iface_name, _T("ENCRYPTION"), str);
			interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleWifiEncryption, (void*)i, thumbnail_configure);
		}
		if( wifiInfo.auth != wifiAuthOpen && wifiInfo.auth < wifiAuthCount )
		{
			sprintf(buf, "%s %s: %s", iface_name, _T("PASSWORD"), wifiInfo.key );
			interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleWifiKey, (void*)i, thumbnail_enterurl);
		}

		sprintf(buf, "%s: %s", iface_name, _T("NET_RESET"));
		interface_addMenuEntry((interfaceMenu_t*)&WifiSubMenu, buf, output_toggleReset, (void*)i, settings_renew);

		return 0;
	}

	return 1;
}
#endif

#if (defined ENABLE_IPTV) && (defined ENABLE_XWORKS)
static int output_togglexWorks(interfaceMenu_t *pMenu, void* pArg)
{
	if ( setParam( STB_CONFIG_FILE, "XWORKS", (int)pArg ? "ON" : "OFF" ) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
		return 1;
	}
	output_fillIPTVMenu( pMenu, NULL );
	interface_displayMenu(1);
	return 0;
}

static int output_togglexWorksProto(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
	media_proto proto = (media_proto)pArg;

	switch(proto) // choose next proto
	{
		case mediaProtoHTTP: proto = mediaProtoRTSP; break;
		default:             proto = mediaProtoHTTP;
	}

	switch(proto) // choose right name for setting
	{
		case mediaProtoRTSP: str = "rtsp"; break;
		default:             str = "http";
	}

	if ( setParam( STB_CONFIG_FILE, "XWORKS_PROTO", str ) != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
		return 1;
	}
	system("/usr/local/etc/init.d/S94xworks config");
	output_fillIPTVMenu( pMenu, NULL );
	interface_displayMenu(1);
	return 0;
}

static int output_restartxWorks(interfaceMenu_t *pMenu, void* pArg)
{
	interface_showMessageBox(_T("LOADING"), settings_renew, 0);

	system("/usr/local/etc/init.d/S94xworks restart");

	output_fillIPTVMenu( pMenu, NULL );
	interface_hideMessageBox();
	return 0;
}
#endif

#ifdef ENABLE_IPTV
int output_toggleIPTVtimeout(interfaceMenu_t *pMenu, void* pArg)
{
	static const time_t timeouts[] = { 3, 5, 7, 10, 15, 30 };
	static const int    timeouts_count = sizeof(timeouts)/sizeof(timeouts[0]);
	int i;
	for( i = 0; i < timeouts_count; i++ )
		if( timeouts[i] >= appControlInfo.rtpMenuInfo.pidTimeout )
			break;
	if( i >= timeouts_count )
		appControlInfo.rtpMenuInfo.pidTimeout = timeouts[0];
	else
		appControlInfo.rtpMenuInfo.pidTimeout = timeouts[ (i+1)%timeouts_count ];

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	output_fillIPTVMenu(pMenu, pArg);
	interface_displayMenu(1);
	return 0;
}

static int output_fillIPTVMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&IPTVSubMenu);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("IPTV_PLAYLIST"), appControlInfo.rtpMenuInfo.usePlaylistURL ? "URL" : "SAP");
	interface_addMenuEntry((interfaceMenu_t*)&IPTVSubMenu, buf, output_toggleIPTVPlaylist, NULL, thumbnail_configure);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("IPTV_PLAYLIST"), appControlInfo.rtpMenuInfo.playlist[0] != 0 ? appControlInfo.rtpMenuInfo.playlist : _T("NONE"));
	interface_addMenuEntryCustom((interfaceMenu_t*)&IPTVSubMenu, interfaceMenuEntryText, buf, strlen(buf)+1, appControlInfo.rtpMenuInfo.usePlaylistURL, output_toggleURL, NULL, NULL, NULL, (void*)optionRtpPlaylist, thumbnail_enterurl);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("IPTV_EPG"), appControlInfo.rtpMenuInfo.epg[0] != 0 ? appControlInfo.rtpMenuInfo.epg : _T("NONE"));
	interface_addMenuEntryCustom((interfaceMenu_t*)&IPTVSubMenu, interfaceMenuEntryText, buf, strlen(buf)+1, appControlInfo.rtpMenuInfo.usePlaylistURL, output_toggleURL, NULL, NULL, NULL, (void*)optionRtpEpg, thumbnail_enterurl);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %ld", _T("IPTV_WAIT_TIMEOUT"), appControlInfo.rtpMenuInfo.pidTimeout);
	interface_addMenuEntry((interfaceMenu_t*)&IPTVSubMenu, buf, output_toggleIPTVtimeout, pArg, thumbnail_configure);

#ifdef ENABLE_XWORKS
	int xworks_enabled;
	media_proto proto;
	char *str;

	getParam( STB_CONFIG_FILE, "XWORKS", "OFF", buf );
	xworks_enabled = strcasecmp( "ON", buf ) == 0;
	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("XWORKS"), xworks_enabled ? _T("ON") : _T("OFF"));
	interface_addMenuEntry((interfaceMenu_t*)&IPTVSubMenu, buf, output_togglexWorks, (void*)!xworks_enabled, thumbnail_configure);

	getParam( STB_CONFIG_FILE, "XWORKS_PROTO", "http", buf );
	if( strcasecmp( buf, "rtsp" ) == 0 )
	{
		proto = mediaProtoRTSP;
		str = _T("MOVIES");
	} else
	{
		proto = mediaProtoHTTP;
		str = "HTTP";
	}

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("USE_PROTOCOL"), str);
	if( xworks_enabled )
		interface_addMenuEntry((interfaceMenu_t*)&IPTVSubMenu, buf, output_togglexWorksProto, (void*)proto, thumbnail_configure);
	else
		interface_addMenuEntryDisabled((interfaceMenu_t*)&IPTVSubMenu, buf, thumbnail_configure);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("XWORKS"), _T("RESTART"));
	interface_addMenuEntry((interfaceMenu_t*)&IPTVSubMenu, buf, output_restartxWorks, NULL, settings_renew);

	if( xworks_enabled && appControlInfo.rtpMenuInfo.usePlaylistURL )
	{
		char temp[256];
		sprintf(buf, "ifconfig %s | grep \"inet addr\"", helperEthDevice(ifaceWAN));
		if (helperParseLine(INFO_TEMP_FILE, buf, "inet addr:", temp, ' ')) //           inet addr:192.168.200.15  Bcast:192.168.200.255  Mask:255.255.255.0
		{
			sprintf(buf, "http://%s:1080/xworks.xspf", temp );
			interface_addMenuEntryDisabled((interfaceMenu_t*)&IPTVSubMenu, buf, thumbnail_enterurl);
		}
	}

	if( interface_getSelectedItem( (interfaceMenu_t*)&IPTVSubMenu ) >= interface_getMenuEntryCount( (interfaceMenu_t*)&IPTVSubMenu ) )
	{
		interface_setSelectedItem( (interfaceMenu_t*)&IPTVSubMenu, 0 );
	}
#endif
#ifdef ENABLE_PROVIDER_PROFILES
	interface_addMenuEntry( (interfaceMenu_t*)&IPTVSubMenu, _T("PROFILE"), (menuActionFunction)menuDefaultActionShowMenu, (void*)&ProfileMenu, thumbnail_account );
#endif

	return 0;
}
#endif

#ifdef ENABLE_VOD
static int output_fillVODMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&VODSubMenu);

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("VOD_PLAYLIST"), appControlInfo.rtspInfo[screenMain].usePlaylistURL ? "URL" : _T("IP_ADDRESS") );
	interface_addMenuEntry((interfaceMenu_t*)&VODSubMenu, buf, output_toggleVODPlaylist, NULL, thumbnail_configure);

	if( appControlInfo.rtspInfo[screenMain].usePlaylistURL )
	{
		snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("VOD_PLAYLIST"), appControlInfo.rtspInfo[screenMain].streamInfoUrl != 0 ? appControlInfo.rtspInfo[screenMain].streamInfoUrl : _T("NONE"));
		interface_addMenuEntry((interfaceMenu_t*)&VODSubMenu, buf, output_toggleURL, (void*)optionVodPlaylist, thumbnail_enterurl);
	}
	else
	{
		snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("VOD_INFO_IP_ADDRESS"), appControlInfo.rtspInfo[0].streamInfoIP);
		interface_addMenuEntry((interfaceMenu_t*)&VODSubMenu, buf, output_toggleVODINFOIP, NULL, thumbnail_enterurl);
	}

	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("VOD_IP_ADDRESS"), appControlInfo.rtspInfo[0].streamIP);
	interface_addMenuEntry((interfaceMenu_t*)&VODSubMenu, buf, output_toggleVODIP, NULL, thumbnail_enterurl);

	return 0;
}
#endif

static int output_fillWebMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char temp[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&WebSubMenu);

	getParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", "", temp);
	sprintf(buf, "%s: %s", _T("PROXY_ADDR"), temp[0] != 0 ? temp : _T("NONE"));
	interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleProxyAddr, pArg, thumbnail_enterurl);

	getParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", "", temp);
	sprintf(buf, "%s: %s", _T("PROXY_LOGIN"), temp[0] != 0 ? temp : _T("NONE"));
	interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleProxyLogin, pArg, thumbnail_enterurl);

	sprintf(buf, "%s: ***", _T("PROXY_PASSWD"));
	interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleProxyPasswd, pArg, thumbnail_enterurl);

#ifdef ENABLE_BROWSER
		getParam(BROWSER_CONFIG_FILE, "HomeURL", "", temp);
		sprintf(buf, "%s: %s", _T("MW_ADDR"), temp);
		interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleMWAddr, pArg, thumbnail_enterurl);

		getParam(BROWSER_CONFIG_FILE, "AutoLoadingMW", "", temp);
		if (temp[0] != 0)
		{
			sprintf(buf, "%s: %s", _T("MW_AUTO_MODE"), strcmp(temp,"ON")==0 ? _T("ON") : _T("OFF"));
			interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleMWAutoLoading, pArg, thumbnail_configure);
		}else
		{
			setParam(BROWSER_CONFIG_FILE, "AutoLoadingMW","OFF");
			sprintf(buf, "%s: %s", _T("MW_AUTO_MODE"), _T("OFF"));
			interface_addMenuEntry((interfaceMenu_t*)&WebSubMenu, buf, output_toggleMWAutoLoading, pArg, thumbnail_configure);
		}
	
#endif
	return 0;
}

#ifdef ENABLE_LAN
int output_fillGatewayMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char *str;
	gatewayMode_t mode;

	interface_clearMenuEntries((interfaceMenu_t*)&GatewaySubMenu);

	for( mode = gatewayModeOff; mode < gatewayModeCount; mode++ )
	{
		switch( mode ) {
			case gatewayModeBridge: str = _T("GATEWAY_BRIDGE"); break;
			case gatewayModeNAT:    str = _T("GATEWAY_NAT_ONLY"); break;
			case gatewayModeFull:   str = _T("GATEWAY_FULL"); break;
			default:                str = _T("OFF"); break;
		}
		interface_addMenuEntry((interfaceMenu_t*)&GatewaySubMenu, str, mode == output_gatewayMode ? NULL : output_toggleGatewayMode, (void*)mode,  mode == output_gatewayMode ? radiobtn_filled : radiobtn_empty);
	}

	interface_menuActionShowMenu(pMenu, (void*)&GatewaySubMenu);

	return 0;
}
#endif

int output_fillInterfaceMenu(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[MENU_ENTRY_INFO_LENGTH];

	interface_clearMenuEntries((interfaceMenu_t*)&InterfaceMenu);

	sprintf( buf, "%s: %s", _T("RESUME_AFTER_START"), _T( appControlInfo.playbackInfo.bResumeAfterStart ? "ON" : "OFF" ) );
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_toggleResumeAfterStart, NULL, settings_interface);

	sprintf( buf, "%s: %s", _T("AUTOPLAY"), _T( appControlInfo.playbackInfo.bAutoPlay ? "ON" : "OFF" ) );
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_toggleAutoPlay, NULL, settings_interface);

#ifdef STB82
	char *str;
	switch( interfaceInfo.animation )
	{
		case interfaceAnimationVerticalCinema:     str = _T("VERTICAL_CINEMA");     break;
		case interfaceAnimationVerticalPanorama:   str = _T("VERTICAL_PANORAMA");   break;
		case interfaceAnimationHorizontalPanorama: str = _T("HORIZONTAL_PANORAMA"); break;
		case interfaceAnimationHorizontalSlide:    str = _T("HORIZONTAL_SLIDE");    break;
		case interfaceAnimationHorizontalStripes:  str = _T("HORIZONTAL_STRIPES");  break;
		default:                                   str = _T("NONE");
	}
	snprintf(buf, sizeof(buf), "%s: %s", _T("MENU_ANIMATION"), str);
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_toggleInterfaceAnimation, NULL, settings_interface);
#endif
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, _T("CHANGE_HIGHLIGHT_COLOR"), output_toggleHighlightColor, NULL, settings_interface);
	snprintf(buf, sizeof(buf), "%s: %d %s", _T("PLAYCONTROL_SHOW_TIMEOUT"), interfacePlayControl.showTimeout, _T("SECOND_SHORT"));
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_togglePlayControlTimeout, NULL, settings_interface);
	snprintf(buf, sizeof(buf), "%s: %s", _T("PLAYCONTROL_SHOW_ON_START"), interfacePlayControl.showOnStart ? _T("ON") : _T("OFF") );
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_togglePlayControlShowOnStart, NULL, settings_interface);
#ifdef ENABLE_VOIP
	snprintf(buf, sizeof(buf), "%s: %s", _T("VOIP_INDICATION"), _T( interfaceInfo.enableVoipIndication ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_toggleVoipIndication, NULL, settings_interface);
	snprintf(buf, sizeof(buf), "%s: %s", _T("VOIP_BUZZER"), _T( appControlInfo.voipInfo.buzzer ? "ON" : "OFF" ));
	interface_addMenuEntry((interfaceMenu_t*)&InterfaceMenu, buf, output_toggleVoipBuzzer, NULL, settings_interface);
#endif

	interface_menuActionShowMenu(pMenu, (void*)&InterfaceMenu);
	interface_displayMenu(1);

	return 0;
}

void output_fillOutputMenu(void)
{
	char *str;
	interface_clearMenuEntries((interfaceMenu_t*)&OutputMenu);

//#ifdef STB6x8x
	str = _T("INFO");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, show_info, NULL, thumbnail_info);
//#endif
#ifdef ENABLE_STATS
	str = _T("PROFILE");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&StatsMenu, thumbnail_recorded);
#endif
	str = _T("LANGUAGE");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, l10n_initLanguageMenu, NULL, settings_language);

#ifdef ENABLE_MESSAGES
	str = _T("MESSAGES");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&MessagesMenu, thumbnail_messages);
#endif

	str = _T("TIME_DATE_CONFIG");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_fillTimeMenu, NULL, settings_datetime);

	str = _T("VIDEO_CONFIG");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_fillVideoMenu, NULL, settings_video);

#ifdef ENABLE_DVB
#ifdef HIDE_EXTRA_FUNCTIONS
	if ( appControlInfo.tunerInfo[0].status != tunerNotPresent ||
		 appControlInfo.tunerInfo[1].status != tunerNotPresent )
#endif
	{
		str = _T("DVB_CONFIG");
#ifndef ENABLE_PASSWORD
		interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_fillDVBMenu, NULL, settings_dvb);
#else
		interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_askPassword, (void*)output_fillDVBMenu, settings_dvb);
#endif
	}
#endif // #ifdef ENABLE_DVB

	str = _T("NETWORK_CONFIG");
#ifndef ENABLE_PASSWORD
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_fillNetworkMenu, NULL, settings_network);
#else
		interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_askPassword, (void*)output_fillNetworkMenu, settings_network);
#endif

	str = _T("INTERFACE");
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_fillInterfaceMenu, NULL, settings_interface);

	str = _T("RESET_SETTINGS");
#ifndef ENABLE_PASSWORD
	interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_resetSettings, NULL, thumbnail_warning);
#else
		interface_addMenuEntry((interfaceMenu_t*)&OutputMenu, str, output_askPassword, (void*)output_resetSettings, thumbnail_warning);
#endif

	output_fillStandardMenu();
	output_fillFormatMenu();
	output_fillTimeZoneMenu();
#if 0
    if(!(gfx_isHDoutput()))
    {
        /* Ensure that the correct format entry is selected */
		interface_setSelectedItem((interfaceMenu_t*)&FormatMenu, 1);
        /*menuInfra_setSelected(&FormatMenu, 2);
        menuInfra_setHighlight(&FormatMenu, 2);*/
    }
#endif
}


void output_buildMenu(interfaceMenu_t *pParent)
{

	createListMenu(&OutputMenu, _T("SETTINGS"), thumbnail_configure, NULL, pParent,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&VideoSubMenu, _T("VIDEO_CONFIG"), settings_video, NULL, (interfaceMenu_t*)&OutputMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

#ifdef STB225
	createListMenu(&Video3DSubMenu, _T("3D_SETTINGS"), settings_video, NULL, (interfaceMenu_t*)&VideoSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
#endif // #ifdef STB225

	createListMenu(&TimeSubMenu, _T("TIME_DATE_CONFIG"), settings_datetime, NULL, (interfaceMenu_t*)&OutputMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
#ifdef ENABLE_DVB
	createListMenu(&DVBSubMenu, _T("DVB_CONFIG"), settings_dvb, NULL, (interfaceMenu_t*)&OutputMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
#endif
	createListMenu(&NetworkSubMenu, _T("NETWORK_CONFIG"), settings_network, NULL, (interfaceMenu_t*)&OutputMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&StandardMenu, _T("TV_STANDARD"), settings_video, NULL, (interfaceMenu_t*)&VideoSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&FormatMenu, _T("TV_FORMAT"), settings_video, NULL, (interfaceMenu_t*)&VideoSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&BlankingMenu, _T("TV_BLANKING"), settings_video, NULL, (interfaceMenu_t*)&VideoSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&TimeZoneMenu, _T("TIME_ZONE"), settings_datetime, NULL, (interfaceMenu_t*)&TimeSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&InterfaceMenu, _T("INTERFACE"), settings_interface, NULL, (interfaceMenu_t*)&OutputMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);

	createListMenu(&WANSubMenu, "WAN", settings_network, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillWANMenu, NULL, (void*)ifaceWAN);

#ifdef ENABLE_PPP
	createListMenu(&PPPSubMenu, _T("PPP"), settings_network, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillPPPMenu, output_leavePPPMenu, (void*)ifaceWAN);
#endif
#ifdef ENABLE_LAN
	createListMenu(&LANSubMenu, "LAN", settings_network, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillLANMenu, NULL, (void*)ifaceLAN);

	createListMenu(&GatewaySubMenu, _T("GATEWAY_MODE"), settings_network, NULL, (interfaceMenu_t*)&LANSubMenu,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
#endif
#ifdef ENABLE_WIFI
	createListMenu(&WifiSubMenu, _T("WIRELESS"), settings_network, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillWifiMenu, NULL, (void*)ifaceWireless);
#endif
#ifdef ENABLE_IPTV
	createListMenu(&IPTVSubMenu, _T("TV_CHANNELS"), thumbnail_multicast, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillIPTVMenu, NULL, NULL);
#ifdef ENABLE_PROVIDER_PROFILES
	createListMenu(&ProfileMenu, _T("PROFILE"), thumbnail_account, NULL, (interfaceMenu_t*)&IPTVSubMenu,
		interfaceListMenuIconThumbnail, output_enterProfileMenu, output_leaveProfileMenu, NULL);
#endif
#endif
#ifdef ENABLE_VOD
	createListMenu(&VODSubMenu, _T("MOVIES"), thumbnail_vod, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillVODMenu, NULL, NULL);
#endif
	createListMenu(&WebSubMenu, _T("INTERNET_BROWSING"), thumbnail_internet, NULL, (interfaceMenu_t*)&NetworkSubMenu,
		interfaceListMenuIconThumbnail, output_fillWebMenu, NULL, NULL);

#ifdef ENABLE_MESSAGES
	messages_buildMenu((interfaceMenu_t*)&OutputMenu);
#endif

	TimeEntry.info.time.type    = interfaceEditTime24;

#ifdef ENABLE_WIFI
	wifiInfo.channelCount   = 0;
	wifiInfo.currentChannel = -1;
#endif

	output_fillStandardMenu();
	output_fillOutputMenu();
	output_fillFormatMenu();
	output_fillBlankingMenu();
	output_fillTimeZoneMenu();
}

#if (defined ENABLE_IPTV) && (defined ENABLE_PROVIDER_PROFILES)
static int output_select_profile(const struct dirent * de)
{
	if( de->d_name[0] == '.' )
		return 0;

	char full_path[MAX_PROFILE_PATH];
	char name[MENU_ENTRY_INFO_LENGTH];
	name[0] = 0;
	snprintf(full_path, sizeof(full_path), "%s%s",STB_PROVIDER_PROFILES_DIR,de->d_name);
	getParam(full_path, "NAME", "", name);
	return name[0] != 0;
}

static int output_enterProfileMenu(interfaceMenu_t *pMenu, void* pArg)
{
	if( output_profiles == NULL )
	{
		char name[MENU_ENTRY_INFO_LENGTH];
		char full_path[MAX_PROFILE_PATH];
		int i;

		interface_clearMenuEntries((interfaceMenu_t *)&ProfileMenu);
		if( readlink(STB_PROVIDER_PROFILE, output_profile, sizeof(output_profile)) <= 0 )
			output_profile[0] = 0;

		output_profiles_count = scandir( STB_PROVIDER_PROFILES_DIR, &output_profiles, output_select_profile, alphasort );
		for( i = 0; i < output_profiles_count; i++ )
		{
			snprintf(full_path, sizeof(full_path), "%s%s",STB_PROVIDER_PROFILES_DIR,output_profiles[i]->d_name);
			getParam(full_path, "NAME", "", name);
			interface_addMenuEntry( (interfaceMenu_t *)&ProfileMenu, name, output_setProfile, (void*)i,
			                        strcmp(full_path, output_profile) == 0 ? radiobtn_filled : radiobtn_empty );
		}
		if( 0 == output_profiles_count )
			interface_addMenuEntryDisabled((interfaceMenu_t *)&ProfileMenu, _T("NO_FILES"), thumbnail_info);
	}
	return 0;
}

static int output_leaveProfileMenu(interfaceMenu_t *pMenu, void* pArg)
{
	int i;
	if( output_profiles != NULL )
	{
		for( i = 0; i < output_profiles_count; i++ )
			free(output_profiles[i]);

		free(output_profiles);
		output_profiles = NULL;
		output_profiles_count = 0;
	}
	return 0;
}

static int output_setProfile(interfaceMenu_t *pMenu, void* pArg)
{
	int index = (int)pArg;
	int i;
	char full_path[MAX_PROFILE_PATH];
	char buffer[MAX_URL+64];
	char *value;
	size_t value_len;
	FILE *profile = NULL;

	snprintf(full_path, sizeof(full_path), "%s%s",STB_PROVIDER_PROFILES_DIR,output_profiles[index]->d_name);
	profile = fopen(full_path, "r");

	if( !profile )
	{
		eprintf("%s: Failed to open profile '%s': %s\n", __FUNCTION__, full_path, strerror(errno));
		interface_showMessageBox(_T("FAIL"), thumbnail_error, 3000);
		return 1;
	}

	interface_showMessageBox(_T("LOADING"), settings_renew, 0);
	eprintf("%s: loading profile %s\n", __FUNCTION__, full_path);
	while( fgets( buffer, sizeof(buffer), profile ) )
	{
		value = strchr(buffer, '=');
		if( NULL == value )
			continue;
		*value = 0;
		value++;
		value_len = strlen(value);
		while( value_len > 0 && (value[value_len-1] == '\n' || value[value_len-1] == '\r' ) )
		{
			value_len--;
			value[value_len] = 0;
		}

		if( strcmp( buffer, "RTPPLAYLIST" ) == 0 )
		{
			strcpy( appControlInfo.rtpMenuInfo.playlist, value );
			appControlInfo.rtpMenuInfo.usePlaylistURL = value[0] != 0;
		} else
		if( strcmp( buffer, "RTPEPG" ) == 0 )
		{
			strcpy( appControlInfo.rtpMenuInfo.epg, value );
		} else
		if( strcmp( buffer, "RTPPIDTIMEOUT" ) == 0 )
		{
			appControlInfo.rtpMenuInfo.pidTimeout = atol( value );
		} else
		if( strcmp( buffer, "VODIP") == 0 )
		{
			strcpy( appControlInfo.rtspInfo[0].streamIP, value );
		} else
		if( strcmp( buffer, "VODINFOURL" ) == 0 )
		{
			strcpy( appControlInfo.rtspInfo[0].streamInfoUrl, value );
			appControlInfo.rtspInfo[0].usePlaylistURL = value[0] != 0;
		} else
		if( strcmp( buffer, "VODINFOIP" ) == 0 )
		{
			strcpy( appControlInfo.rtspInfo[0].streamIP, "VODIP" );
		} else
		if( strcmp( buffer, "FWUPDATEURL" ) == 0 && value_len > 0 &&
			/* URL can have any characters, so we should  */
			value_len < sizeof(buffer)-(sizeof("hwconfigManager l -1 UPURL '")-1+1) &&
		   (strncasecmp( value, "http://", 7 ) == 0 || strncasecmp( value, "ftp://", 6 ))
		  )
		{
			char *dst, *src;

			eprintf("%s: Setting new fw update url %s\n", __FUNCTION__, value);
			src = &value[value_len-1];
			dst = &buffer[ sizeof("hwconfigManager l -1 UPURL '")-1 + value_len + 1 ];
			*dst-- = 0;
			*dst-- = '\'';
			while( src >= value )
				*dst-- = *src--;
			memcpy(buffer, "hwconfigManager l -1 UPURL '", sizeof("hwconfigManager l -1 UPURL '")-1);
			dprintf("%s: '%s'\n", __FUNCTION__, buffer);
			system(buffer);
		} else
		if( strcmp( buffer, "FWUPDATEADDR" ) == 0 && value_len > 0 && value_len < 24 ) // 234.4.4.4:4323
		{
#ifndef DEBUG
#define DBG_MUTE  " > /dev/null"
#else
#define DBG_MUTE
#endif
			char *dst, *port_str;
			unsigned int port;

			eprintf("%s: Setting new fw update address %s\n", __FUNCTION__, value);
			port_str = strchr(value, ':');
			if( port_str != NULL )
				*port_str++ = 0;
			dst = &value[value_len+1];
			sprintf(dst, "hwconfigManager s -1 UPADDR 0x%08X" DBG_MUTE, inet_addr(value));
			dprintf("%s: '%s'\n", __FUNCTION__, dst);
			system(dst);
			if( port_str != NULL )
			{
				port = atol(port_str) & 0xffff;
				sprintf(dst, "hwconfigManager s -1 UPADDR 0xFFFF%04X" DBG_MUTE, port);
				dprintf("%s: '%s'\n", __FUNCTION__, dst);
				system(dst);
			}
		}
#if 0
		else
		if( strcmp( buffer, "PASSWORD" ) == 0 && value_len == 32 )
		{
			char *dst = &value[value_len+1];

			sprintf( dst, "hwconfigManager l -1 PASSWORD %s", value );
			eprintf("%s: Setting new password %s\n", __FUNCTION__, value);
			dprintf("%s: '%s'\n", __FUNCTION__, buffer);
			system(buffer);
		} else
		if( strcmp( buffer, "COMMUNITY_PASSWORD" ) == 0 && value[0] != 0 && value_len < (sizeof(buffer)-sizeof("hwconfigManager l -1 COMMPWD ")) )
		{
			char *dst, *src;

			system("/usr/local/etc/init.d/S98snmpd stop");
			dst = &buffer[ sizeof("hwconfigManager l -1 COMMPWD ")-1 + value_len ];
			src = &value[value_len];
			eprintf("%s: Setting new SNMP password %s\n", __FUNCTION__, value);
			while( src >= value )
				*dst-- = *src--;
			memcpy(buffer, "hwconfigManager l -1 COMMPWD ", sizeof("hwconfigManager l -1 COMMPWD ")-1);
			dprintf("%s: '%s'\n", __FUNCTION__, buffer);
			system(buffer);
			system("/usr/local/etc/init.d/S98snmpd start");
		}
#endif
	}
	fclose(profile);
	interface_hideMessageBox();

	if (saveAppSettings() != 0 && bDisplayedWarning == 0)
	{
		bDisplayedWarning = 1;
		interface_showMessageBox(_T("SETTINGS_SAVE_ERROR"), thumbnail_warning, 0);
	}

	unlink(STB_PROVIDER_PROFILE);
	symlink(full_path, STB_PROVIDER_PROFILE);
	for( i = 0; i < ProfileMenu.baseMenu.menuEntryCount; i++ )
		ProfileMenu.baseMenu.menuEntry[i].thumbnail = ProfileMenu.baseMenu.menuEntry[i].pArg == pArg ? radiobtn_filled : radiobtn_empty;

	interface_menuActionShowMenu(pMenu, &interfaceMainMenu);

	return 0;
}

int output_checkProfile(void)
{
	int fd = open(STB_PROVIDER_PROFILE, O_RDONLY);
	if( fd < 0 )
	{
		interfaceInfo.currentMenu = (interfaceMenu_t*)&ProfileMenu;
		output_enterProfileMenu(interfaceInfo.currentMenu, NULL);
		return 1;
	}
	close(fd);
	return 0;
}
#endif

//#endif // #ifdef STBxx
