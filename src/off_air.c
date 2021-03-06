
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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/rtc.h>

/* for sdp.h */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <linux/dvb/frontend.h>
#include <directfb.h>
#include <ctype.h>
#include <pthread.h>

#include <phStbRpc_Common.h>
#include <phStbSystemManager.h>

#include "sdp.h"
#include "tools.h"

#include "app_info.h"
#include "sem.h"
#include "gfx.h"
#include "interface.h"
#include "l10n.h"
#include "menu_app.h"
#include "output.h"
#include "StbMainApp.h"
#ifdef ENABLE_PVR
#include "pvr.h"
#endif
#include "sem.h"
#include "media.h"
#include "playlist.h"
#include "off_air.h"
#include "rtp.h"
#include "rtsp.h"
#include "dlna.h"
#include "debug.h"
#ifdef STB225
#include "phStbSbmSink.h"
#endif
#include "list.h"
#ifdef ENABLE_STATS
#include "stats.h"
#endif
#include "teletext.h"

//extern char scan_messages[64*1024];

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define STREAM_INFO_SET(screen, channel) ((screen << 16) | (channel))
#define STREAM_INFO_GET_SCREEN(info)     (info >> 16)
#define STREAM_INFO_GET_STREAM(info)     (info & 0xFFFF)

#define MENU_ITEM_TIMELINE     (0)
#define MENU_ITEM_EVENT        (1)

#ifdef ENABLE_DVB
#define OFFAIR_SERVICES_FILENAME         "/config/StbMainApp/offair.conf"
#define OFFAIR_MULTIVIEW_FILENAME        "/tmp/dvb.ts"
#define OFFAIR_MULTIVIEW_INFOFILE        "/tmp/dvb.multi"

#define EPG_UPDATE_INTERVAL    (1000)

#define PSI_UPDATE_INTERVAL    (1000)

#define WIZARD_UPDATE_INTERVAL (100)
#endif

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

#ifdef ENABLE_DVB
typedef enum _wizardState_t
{
	wizardStateDisabled = 0,
	wizardStateConfirmLocation,
	wizardStateSelectLocation,
	wizardStateConfirmManualScan,
	wizardStateInitialFrequencyScan,
	wizardStateInitialFrequencyMonitor,
	wizardStateInitialServiceScan,
	wizardStateConfirmUpdate,
	wizardStateUpdating,
	wizardStateSelectChannel,
	wizardStateCustomFrequencySelect,
	wizardStateCustomFrequencyMonitor,

	wizardStateCount
} wizardState_t;

typedef struct _wizardSettings_t
{
	wizardState_t state;
	int allowExit;
	int locationCount;
	struct dirent **locationFiles;
	unsigned long frequency[64];
	unsigned long frequencyCount;
	unsigned long frequencyIndex;
	interfaceMenu_t *pFallbackMenu;
} wizardSettings_t;

#ifdef ENABLE_MULTI_VIEW
typedef struct _offair_multiviewInstance_t {
	int stop;
	int exit;
	pthread_t thread;
} offair_multiviewInstance_t;
#endif
#endif /* ENABLE_DVB */

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

#ifdef ENABLE_DVB

static void offair_setStateCheckTimer(int which, int bEnable);
static int  offair_startNextChannel(int direction, void* pArg);
static int  offair_setChannel(int channel, void* pArg);
static int  offair_infoTimerEvent(void *pArg);
static int  offair_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
static void offair_startDvbVideo(int which, DvbParam_t *param, int service_id, int ts_id, int audio_type, int video_type, int text_type);
static void offair_initDVBTOutputMenu(interfaceMenu_t *pParent, int which);
//static void offair_buildDVBTChannelMenu(int which);
static int  offair_fillEPGMenu(interfaceMenu_t *pMenu, void* pArg);
static void offair_sortEvents();
static void offair_swapServices(int first, int second);
static int  offair_getUserFrequency(interfaceMenu_t *pMenu, char *value, void* pArg);
static int  offair_getUserLCN(interfaceMenu_t *pMenu, char *value, void* pArg);
static int  offair_changeLCN(interfaceMenu_t *pMenu, void* pArg);
static int  offair_scheduleCheck( int channelNumber );
static int  offair_showSchedule(interfaceMenu_t *pMenu, void* pArg);
static int  offair_updateEPG(void* pArg);
static int  offair_playControlProcessCommand(pinterfaceCommandEvent_t cmd, void* pArg);
#ifdef ENABLE_DVB_DIAG
static int  offair_updatePSI(void* pArg);
#endif
static int  offair_confirmAutoScan(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

static void offair_getServiceDescription(EIT_service_t *service, char *desc, char *mode);
#ifdef ENABLE_PVR
static int  offair_confirmStopRecording(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void *pArg);
static int  offair_stopRecording(interfaceMenu_t *pMenu, void *pArg);
static void offair_EPGRecordMenuDisplay(interfaceMenu_t *pMenu);
static int  offair_EPGRecordMenuProcessCommand(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd);
static int  offair_EPGMenuKeyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
#endif
#ifdef ENABLE_MULTI_VIEW
static int offair_multi_callback(pinterfaceCommandEvent_t cmd, void* pArg);
static int offair_multiviewNext(int direction, void* pArg);
static int offair_multiviewPlay( interfaceMenu_t *pMenu, void *pArg);
#endif
#ifdef ENABLE_STATS
static int offair_updateStats(int which);
static int offair_updateStatsEvent(void* pArg);
#endif

#ifdef ENABLE_DVB_DIAG
static int offair_checkSignal(int which, list_element_t **pPSI);
#endif

static int wizard_init();
static int  wizard_show(int allowExit, int displayMenu, interfaceMenu_t *pFallbackMenu, unsigned long monitor_only_frequency);
static void wizard_cleanup(int finished);

#endif /* ENABLE_DVB */

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

#ifdef ENABLE_DVB
static interfaceListMenu_t EPGMenu;

static long offair_currentFrequency, offair_currentFrequencyNumber, offair_currentFrequencyNumberMax;

/* offair_indeces used to display sorted list of channels */
static int offair_indeces[MAX_MEMORIZED_SERVICES];
static int  offair_indexCount = 0;
static int  offair_scheduleIndex;   // service number
static char offair_lcn_buf[4];

static pmysem_t epg_semaphore = 0;
static pmysem_t offair_semaphore = 0;

static interfaceListMenu_t wizardHelperMenu;
static wizardSettings_t *wizardSettings = NULL;
#endif

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceEpgMenu_t EPGRecordMenu;
interfaceColor_t genre_colors[0x10] = {
	{ 151, 200, 142, 0x88 }, // other
	{ 255,  94,  81, 0x88 }, // movie
	{ 175, 175, 175, 0x88 }, // news
	{ 251, 147,  46, 0x88 }, // show
	{  89, 143, 198, 0x88 }, // sports
	{ 245, 231,  47, 0x88 }, // children's
	{ 255, 215, 182, 0x88 }, // music
	{ 165,  81,  13, 0x88 }, // art
	{ 236, 235, 235, 0x88 }, // politics
	{ 160, 151, 198, 0x88 }, // education
	{ 162, 160,   5, 0x88 }, // leisure
	{ 151, 200, 142, 0x88 }, // special
	{ 151, 200, 142, 0x88 }, // 0xC
	{ 151, 200, 142, 0x88 }, // 0xD
	{ 151, 200, 142, 0x88 }, // 0xE
	{ 151, 200, 142, 0x88 }  // 0xF
};

#ifdef ENABLE_DVB
/* Service index in offair_service used as channel number */
service_index_t offair_services[MAX_MEMORIZED_SERVICES];
int  offair_serviceCount = 0;

interfaceListMenu_t DVBTMenu;
interfaceListMenu_t DVBTOutputMenu[screenOutputs];

//interfaceSlider_t InstallSlider;
#endif

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

#ifdef ENABLE_DVB

tunerFormat offair_getTuner(void)
{
	stb810_tunerStatus status;
	for ( status = tunerInactive; status < tunerStatusGaurd; status++ )
	{
		tunerFormat tuner;
		for ( tuner = inputTuner0; tuner < inputTuners; tuner++ )
		{
			if ( appControlInfo.tunerInfo[tuner].status == status )
			{
				if ( status!=tunerInactive )
				{
					switch ( status )
					{
						case(tunerDVBPip) :
							offair_stopVideo(screenPip, 1);
							break;
						case(tunerDVBMain) :
							offair_stopVideo(screenMain, 1);
							break;
#ifdef ENABLE_PVR
						case(tunerDVBPvr2) :
							pvr_stopRecordingDVB(screenPip);
							break;
						case(tunerDVBPvr1) :
							pvr_stopRecordingDVB(screenMain);
							break;
#endif
						default :
							break;
					}
				}
				return tuner;
			}
		}
	}
	return inputTuner0;
}

static long offair_getFrequency(void *pArg)
{
    return offair_currentFrequency;
}

static long offair_getFrequencyNumber(void *pArg)
{
    return offair_currentFrequencyNumber;
}

void offair_setFrequency(long value, void *pArg)
{

}

static void offair_buildInstallSlider(int numChannels, tunerFormat tuner)
{
	char channelEntry[256];
	char installationString[2048];
	char *serviceName;
	__u32 low_freq, high_freq, freq_step;
	int i=0;

	dvb_getTuner_freqs(tuner, &low_freq, &high_freq, & freq_step);

	sprintf(installationString, _T("FOUND_CHANNELS"), offair_currentFrequency, numChannels);
	// in this moment semaphore is busy
	for ( i = numChannels-1; i > numChannels-4; i-- )
	{
		//int channelNumber;
		if ((serviceName = dvb_getTempServiceName(i)) != NULL)
		{
			sprintf(channelEntry, "[%s] ", serviceName);
			strcat(installationString, channelEntry);
		}
	}
	if (numChannels > 0)
	{
		strcat(installationString, "...");
	}

	//dprintf("%s: '%s', min %d, max %d, step %d, freq %d\n", __FUNCTION__, installationString, low_freq, high_freq, freq_step, offair_currentFrequency);

	interface_sliderSetText(installationString);
	if (offair_currentFrequencyNumberMax == 0)
	{
		interface_sliderSetMinValue(low_freq);
		interface_sliderSetMaxValue(high_freq);
		interface_sliderSetCallbacks(offair_getFrequency, offair_setFrequency, NULL);
	} else
	{
		interface_sliderSetMinValue(0);
		interface_sliderSetMaxValue(offair_currentFrequencyNumberMax);
		interface_sliderSetCallbacks(offair_getFrequencyNumber, offair_setFrequency, NULL);
	}
	interface_sliderSetDivisions(100);
	interface_sliderShow(1, 1);
#if 0
	InstallSlider.prev = &DVBTMenu;
	menuInfra_setSliderName(&InstallSlider, installationString);
	menuInfra_setSliderWidth(&InstallSlider, INSTALLATION_SLIDER_WIDTH);
	menuInfra_setSliderPosition(&InstallSlider, INSTALLATION_SLIDER_X, INSTALLATION_SLIDER_Y);
	menuInfra_setSliderRange(&InstallSlider, low_freq, high_freq);
	menuInfra_setSliderAccessFunctions(&InstallSlider, offair_getFrequency, offair_setFrequency);
	/* Force the slider range to be 0 - 100 */
	InstallSlider.divisions = 100;
#endif
}

static int offair_updateDisplay(long frequency, int channelCount, tunerFormat tuner, long frequencyNumber, long frequencyMax)
{
	interfaceCommand_t cmd;

	dprintf("%s: in\n", __FUNCTION__);

	//dprintf("%s: freq: %d\n", __FUNCTION__, frequency);
	offair_currentFrequency = frequency;
	offair_currentFrequencyNumber = frequencyNumber;
	offair_currentFrequencyNumberMax = frequencyMax;
	offair_buildInstallSlider(channelCount, tuner);
	//menuInfra_sliderDisplay((void*)&InstallSlider);
	//interface_displayMenu(1);

	while ((cmd = helperGetEvent(0)) != interfaceCommandNone)
	{
		//dprintf("%s: got command %d\n", __FUNCTION__, cmd);
		if (cmd != interfaceCommandCount)
		{
			dprintf("%s: exit on command %d\n", __FUNCTION__, cmd);
			/* Flush any waiting events */
			helperGetEvent(1);
			return -1;
		}
	}

	//dprintf("%s: got none %d\n", __FUNCTION__, cmd);

	return keepCommandLoopAlive ? 0 : -1;
}

static void offair_setInfoUpdateTimer(int which, int bEnable)
{
	//dprintf("%s: %s info timer\n", __FUNCTION__, bEnable ? "set" : "unset");

	if (bEnable)
	{
		offair_infoTimerEvent((void*)which);
	} else
	{
		//interface_notifyText(NULL, 1);
		interface_customSlider(NULL, NULL, 0, 1);
		interface_removeEvent(offair_infoTimerEvent, (void*)which);
	}
}

int offair_sliderCallback(int id, interfaceCustomSlider_t *info, void *pArg)
{
	int which = (int)pArg;
	uint16_t snr, signal;
	uint32_t ber, uncorrected_blocks;
	fe_status_t status;

	status = dvb_getSignalInfo(appControlInfo.dvbInfo[which].tuner, &snr, &signal, &ber, &uncorrected_blocks);

	if (id < 0 || info == NULL)
	{
		return 3;
	}

	//dprintf("%s: get info 0x%08X\n", __FUNCTION__, info);

	switch (id)
	{
	case 0:
		info->min = 0;
		info->max = MAX_SIGNAL;
		info->value = info->max > (signal&0xFF) ? (signal&0xFF) : info->max;
		info->steps = 3;
		sprintf(info->caption, _T("DVB_SIGNAL_INFO"), info->value*100/(info->max-info->min), _T(status == 1 ? "LOCKED" : "NO_LOCK"));
		break;
	case 1:
		info->min = 0;
		info->max = MAX_BER;
		info->value = status == 1 && info->max > (int)ber ? info->max-(int)ber : info->min;
		info->steps = 3;
		sprintf(info->caption, _T("DVB_BER"), info->value*100/(info->max-info->min));
		break;
	case 2:
		info->min = 0;
		info->max = MAX_UNC;
		info->value = status == 1 && info->max > (int)uncorrected_blocks ? info->max-(int)uncorrected_blocks : info->min;
		info->steps = 1;
		sprintf(info->caption, _T("DVB_UNCORRECTED"), info->value*100/(info->max-info->min));
		break;
		/*		case 3:
		info->min = 0;
		info->max = 255;
		info->value = (snr&0xFF) == 0 ? 0xFF : snr&0xFF;
		sprintf(info->caption, _T("DVB_SNR"), info->value*100/(info->max-info->min));
		break;*/
	default:
		return -1;
	}

	//dprintf("%s: done\n", __FUNCTION__);

	return 1;
}

static int offair_infoTimerEvent(void *pArg)
{

	int which = (int)pArg;
	/*char buf[BUFFER_SIZE];

	uint16_t snr, signal;
	uint32_t ber, uncorrected_blocks;
	fe_status_t status;*/

	mysem_get(offair_semaphore);

	if (appControlInfo.dvbInfo[which].active)
	{
		if (appControlInfo.dvbInfo[which].showInfo)
		{
			/*status = dvb_getSignalInfo(appControlInfo.dvbInfo[which].tuner, &snr, &signal, &ber, &uncorrected_blocks);

			sprintf(buf, _T("DVB_SIGNAL_INFO"),
				signal&0xFF, ber, uncorrected_blocks, _T(status == 1 ? "LOCKED" : "NO_LOCK") );*/
	/*
			sprintf(buf, LANG_TEXT_DVB_SIGNAL_INFO "%s",
				appControlInfo.tunerInfo[which].signal_strength, appControlInfo.tunerInfo[which].snr, appControlInfo.tunerInfo[which].ber, appControlInfo.tunerInfo[which].uncorrected_blocks, appControlInfo.tunerInfo[which].fe_status & FE_HAS_LOCK ? LANG_TEXT_LOCKED : LANG_TEXT_NO_LOCK);
	*/
			//interface_notifyText(buf, 1);
			interface_customSlider(offair_sliderCallback, pArg, 0, 1);
		}

		//offair_setInfoUpdateTimer(which, 1);

		interface_addEvent(offair_infoTimerEvent, (void*)which, INFO_TIMER_PERIOD, 1);
	}

	mysem_release(offair_semaphore);

	return 0;
}

static int offair_stateTimerEvent(void *pArg)
{
	int which = (int)pArg;
	DFBVideoProviderStatus status;

	mysem_get(offair_semaphore);

	if (appControlInfo.dvbInfo[which].active)
	{
		status = gfx_getVideoProviderStatus(which);
		switch( status )
		{
			case DVSTATE_FINISHED:
			case DVSTATE_STOP:
				//interface_showMenu(1, 0);
				mysem_release(offair_semaphore);
				if(status == DVSTATE_FINISHED)
					interface_showMessageBox(_T("ERR_STREAM_NOT_SUPPORTED"), thumbnail_error, 0);
				offair_stopVideo(which, 1);
				return 0;
			default:
				offair_setStateCheckTimer(which, 1);
		}
	}

	mysem_release(offair_semaphore);

	return 0;
}

static void offair_setStateCheckTimer(int which, int bEnable)
{
	//dprintf("%s: %s state timer\n", __FUNCTION__, bEnable ? "set" : "unset");

	if (bEnable)
	{
		interface_addEvent(offair_stateTimerEvent, (void*)which, 1000, 1);
	} else
	{
		interface_removeEvent(offair_stateTimerEvent, (void*)which);
	}
}

int offair_wizardStart(interfaceMenu_t *pMenu, void* pArg)
{
	dprintf("%s: in\n", __FUNCTION__);
	if (wizard_show(1, 1, pMenu, (unsigned long)pArg) == 0)
	{
		//interface_showMessageBox(_T("SETTINGS_WIZARD_NO_LOCATIONS"), thumbnail_warning, 5000);
		output_fillDVBMenu((interfaceMenu_t*)&wizardHelperMenu, (void*)screenMain);
		interface_showMenu(1, 1);
	}

	return 0;
}

int offair_serviceScan(interfaceMenu_t *pMenu, void* pArg)
{
	tunerFormat tuner;
	__u32 low_freq, high_freq, freq_step;
	char buf[256];

	tuner = offair_getTuner();
	dvb_getTuner_freqs(tuner, &low_freq, &high_freq, &freq_step);
	offair_updateDisplay(low_freq, 0, tuner, 0, 0);

	dvb_serviceScan(tuner, offair_updateDisplay);
	/* Re-build any channel related menus */
	/*memset(&DVBTChannelMenu[0], 0, sizeof(interfaceListMenu_t));
	memset(&DVBTChannelMenu[1], 0, sizeof(interfaceListMenu_t));*/
	appControlInfo.dvbInfo[screenMain].channel =
		appControlInfo.dvbInfo[screenPip].channel = 0;//dvb_getChannelNumber(0);

	output_fillDVBMenu(pMenu, pArg);
	offair_fillDVBTMenu();
	offair_fillDVBTOutputMenu(screenMain);
	offair_fillDVBTOutputMenu(screenPip);
#ifdef ENABLE_PVR
	pvr_updateSettings();
#endif
	//interface_showMessageBox(scan_messages, NULL, 0);
	//menuInfra_sliderClose(&InstallSlider);
	interface_sliderShow(0, 0);
	sprintf(buf, _T("SCAN_COMPLETE_CHANNELS_FOUND"), dvb_getNumberOfServices());
	interface_showMessageBox(buf, thumbnail_info, 5000);

	return  -1;
}

static int offair_getUserFrequencyToMonitor(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	unsigned long low_freq = 40000000, high_freq = 860000000;
	unsigned long freq;
	
	if( value == NULL)
		return 1;

	freq = strtoul(value, NULL, 10)*1000;

	if (freq == 0)
	{
		if (dvb_getNumberOfServices() > 0)
		{
			interface_hideMessageBox();
			offair_wizardStart(pMenu, (void*)-1);
		} else
		{
			interface_showMessageBox(_T("ERR_FREUQENCY_LIST_EMPTY"), thumbnail_error, 0);
			return -1;
		}
	} else if (freq >= low_freq && freq <= high_freq)
	{
		interface_hideMessageBox();
		offair_wizardStart(pMenu, (void*)freq);
	} else
	{
		interface_showMessageBox(_T("ERR_FREQUENCY_OUT_OF_RANGE"), thumbnail_error, 0);
		return -1;
	}

	return 0;
}

int offair_frequencyMonitor(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[BUFFER_SIZE];
	long low_freq = 40000000, high_freq = 860000000;

	sprintf(buf, "%s [%ld;%ld] (%s)", _T("ENTER_MONITOR_FREQUENCY"), low_freq / 1000, high_freq / 1000,_T("KHZ"));
	interface_getText(pMenu, buf, "\\d{6}", offair_getUserFrequencyToMonitor, NULL, inputModeDirect, pArg);
	return 0;
}

int offair_frequencyScan(interfaceMenu_t *pMenu, void* pArg)
{
	char buf[BUFFER_SIZE];
	long low_freq = 40000000, high_freq = 860000000;
	/*
	int which = (int)pArg;
	long freq_step;
	tunerFormat tuner = appControlInfo.dvbInfo[which].tuner;
	dvb_getTuner_freqs(tuner, &low_freq, &high_freq, &freq_step);
	*/
	sprintf(buf, "%s [%ld;%ld] (%s)", _T("ENTER_FREQUENCY"), low_freq / 1000, high_freq / 1000,_T("KHZ"));
	interface_getText(pMenu, buf, "\\d{6}", offair_getUserFrequency, NULL, inputModeDirect, pArg);
	return 0;
}

static int offair_getUserFrequency(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int which = (int)pArg;
	long low_freq = 40000000, high_freq = 860000000, frequency;
	tunerFormat tuner;
	char buf[256];
	NIT_table_t nit;
	
	if( value == NULL)
		return 1;

	frequency = strtol(value,NULL,10) * 1000;
	tuner = offair_getTuner(); //appControlInfo.dvbInfo[which].tuner
	/*
	long freq_step;
	dvb_getTuner_freqs(tuner, &low_freq, &high_freq, &freq_step);
	*/
	dprintf( "%s: [ %ld < %ld < %ld ]\n", __FUNCTION__, low_freq, frequency, high_freq);
	if( frequency < low_freq || frequency > high_freq )
	{
		eprintf("offair: %ld is out of frequency range\n", frequency);
		interface_showMessageBox(_T("ERR_FREQUENCY_OUT_OF_RANGE"), thumbnail_error, 0);
		return -1;
	}
	interface_hideMessageBox();
	offair_updateDisplay(frequency, dvb_getNumberOfServices(), which, 0, 1);
	memset(&nit, 0, sizeof(NIT_table_t));
	if( dvb_frequencyScan( tuner, frequency, NULL, offair_updateDisplay, appControlInfo.dvbCommonInfo.networkScan ? &nit : NULL, 1, NULL) == 0)
	{
		output_fillDVBMenu(pMenu, pArg);
		offair_fillDVBTMenu();
		offair_fillDVBTOutputMenu(which);
#ifdef ENABLE_PVR
		pvr_updateSettings();
#endif
	}
	dvb_clearNIT(&nit);
	interface_sliderShow(0, 0);
	sprintf(buf, _T("SCAN_COMPLETE_CHANNELS_FOUND"), dvb_getNumberOfServices());
	interface_showMessageBox(buf, thumbnail_info, 5000);
	return -1;
}

#ifdef ENABLE_MULTI_VIEW
static void *offair_multiviewStopThread(void* pArg)
{
	offair_multiviewInstance_t *multiviewStopInstance = (offair_multiviewInstance_t *)pArg;
	char buffer[128];
	int fp ;

	fp = open(OFFAIR_MULTIVIEW_FILENAME, O_WRONLY);
	if(fp>0)
	{
		int writeData = 0;
		memset(buffer,0xFF,128);
		while(!multiviewStopInstance->stop)
		{
			writeData = write(fp,(const void *)buffer,128);
			usleep(1000);
		}
		close(fp);
	}else
	{
		eprintf("offair: Can't open multiview file '%s'\n", OFFAIR_MULTIVIEW_FILENAME);
	}
	multiviewStopInstance->exit = 1;
	pthread_cancel (multiviewStopInstance->thread);
	return 0;
}
#endif

void offair_stopVideo(int which, int reset)
{
	mysem_get(offair_semaphore);

	if (appControlInfo.dvbInfo[which].active)
	{
		interface_playControlSelect(interfacePlayControlStop);

#ifdef ENABLE_DVB_DIAG
		interface_removeEvent(offair_updatePSI, (void*)screenMain);
		interface_removeEvent(offair_updatePSI, (void*)screenPip);
#endif

		interface_removeEvent(offair_updateEPG, (void*)screenMain);
		interface_removeEvent(offair_updateEPG, (void*)screenPip);
#ifdef ENABLE_STATS
		interface_removeEvent(offair_updateStatsEvent, (void*)screenMain);
		interface_removeEvent(offair_updateStatsEvent, (void*)screenPip);
		offair_updateStats(which);
#endif

#ifdef ENABLE_MULTI_VIEW
		offair_multiviewInstance_t multiviewStopInstance;
		int thread_create = -1;
		/* Force stop provider in multiview mode */
		if(appControlInfo.multiviewInfo.count>0 && appControlInfo.multiviewInfo.source == streamSourceDVB)
		{
			multiviewStopInstance.stop 	= 	0;
			multiviewStopInstance.exit	=	0;
			thread_create = pthread_create(&multiviewStopInstance.thread, NULL,  offair_multiviewStopThread,  &multiviewStopInstance);
		}
		/* Force stop provider in multiview mode */
		gfx_stopVideoProvider(which, reset || (appControlInfo.multiviewInfo.count > 0), 1);
		if(!thread_create)
		{
			appControlInfo.multiviewInfo.count = 0;
			multiviewStopInstance.stop = 1;
			while(!multiviewStopInstance.exit)
				usleep(100000);
		}
#else
		gfx_stopVideoProvider(which, reset, 1);
#endif
#ifdef ENABLE_PVR
		if( appControlInfo.pvrInfo.dvb.channel >=0 )
		{
			pvr_stopPlayback(screenMain);
		}
#endif

		dprintf("%s: Stop video screen %s\n", __FUNCTION__, which == screenPip ? "Pip" : "Main");
		dvb_stopDVB(appControlInfo.dvbInfo[which].tuner, reset);
		appControlInfo.tunerInfo[appControlInfo.dvbInfo[which].tuner].status = tunerInactive;
		appControlInfo.dvbInfo[which].active = 0;

		offair_setStateCheckTimer(which, 0);
		offair_setInfoUpdateTimer(which, 0);

		interface_disableBackground();
		/*if (which == screenMain)
		{
		offair_fillDVBTMenu();
		}
		else
		{
		offair_initDVBTOutputMenu(which);
		}*/
	}

	mysem_release(offair_semaphore);
}

static int offair_audioChange(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int which = STREAM_INFO_GET_SCREEN((int)pArg);
	int selected = STREAM_INFO_GET_STREAM((int)pArg);
	char buf[MENU_ENTRY_INFO_LENGTH];
	char str[MENU_ENTRY_INFO_LENGTH];

	if (cmd != NULL)
	{
		if (cmd->command == interfaceCommandExit || cmd->command == interfaceCommandRed || cmd->command == interfaceCommandLeft)
		{
			return 0;
		} else if (cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk || cmd->command == interfaceCommandGreen)
		{
			if (appControlInfo.dvbInfo[which].audio_track != selected)
			{
				if (dvb_getAudioTypeForService(offair_services[appControlInfo.dvbInfo[which].channel].service, selected) != dvb_getAudioTypeForService(offair_services[appControlInfo.dvbInfo[which].channel].service, appControlInfo.dvbInfo[which].audio_track))
				{
					offair_stopVideo(which, 0);
					appControlInfo.dvbInfo[which].audio_track = selected;
					offair_startVideo(which);
				} else
				{
					dvb_changeAudioPid(appControlInfo.dvbInfo[which].tuner, dvb_getAudioPidForService(offair_services[appControlInfo.dvbInfo[which].channel].service, selected));
				}
				appControlInfo.dvbInfo[which].audio_track = selected;
			}
			return 0;
		} else if (cmd->command == interfaceCommandDown)
		{
			selected++;
			if (selected >= dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service))
			{
				selected = dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service)-1;
			}
		}	else if (cmd->command == interfaceCommandUp)
		{
			selected--;
			if (selected < 0)
			{
				selected = 0;
			}
		}
	} else
	{
		selected = appControlInfo.dvbInfo[which].audio_track;
	}

	buf[0] = 0;
	if (dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service) > 0)
	{

		int i;

		for (i=0; i<dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service); i++)
		{
			if (selected == i)
			{
				sprintf(str, "> Audio Track %d [%d] <\n", i, dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service));
			} else
			{
				sprintf(str, "    Audio Track %d [%d]\n", i, dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service));
			}
			strcat(buf, str);
		}
		buf[strlen(buf)-1] = 0;

		interface_showConfirmationBox(buf, -1, offair_audioChange, (void *)STREAM_INFO_SET(which, selected));
	}
	return 1;
}

int offair_play_callback(interfacePlayControlButton_t button, void *pArg)
{
	char desc[BUFFER_SIZE];
	int which = (int)pArg;

	dprintf("%s: in %d\n", __FUNCTION__, button);

	if ( button == interfacePlayControlPrevious )
	{
		offair_startNextChannel(1, pArg);
	} else if ( button == interfacePlayControlNext )
	{
		offair_startNextChannel(0, pArg);
	} else if ( button == interfacePlayControlPlay )
	{
		dprintf("%s: play\n", __FUNCTION__);
		if ( !appControlInfo.dvbInfo[which].active )
		{
			offair_startVideo(which);
		}
	} else if ( button == interfacePlayControlStop )
	{
		dprintf("%s: stop\n", __FUNCTION__);
#ifdef ENABLE_PVR
		if( appControlInfo.pvrInfo.dvb.channel >= 0 && appControlInfo.pvrInfo.dvb.channel == offair_getCurrentServiceIndex(which) )
		{
			pvr_stopRecordingDVB(which);
			if( appControlInfo.dvbInfo[which].channel > 0 )
			{
				offair_getServiceDescription(offair_services[appControlInfo.dvbInfo[which].channel].service,desc,_T("DVB_CHANNELS"));
				interface_playControlUpdateDescriptionThumbnail(desc, offair_services[appControlInfo.dvbInfo[which].channel].service->service_descriptor.service_type == 2 ? thumbnail_radio : thumbnail_channels);
			}
		}
#endif
		if ( appControlInfo.dvbInfo[which].active )
		{
			offair_stopVideo(which, 1);
		}
	} else if (button == interfacePlayControlInfo)
	{
#ifdef ENABLE_DVB_DIAG
		/* Make diagnostics... */
		//if (appControlInfo.offairInfo.diagnosticsMode == DIAG_ON)
		{
			offair_checkSignal(which, NULL);
		}
		//offair_setInfoUpdateTimer(which, 1);
		interface_displayMenu(1);
#else
		appControlInfo.dvbInfo[which].showInfo = !appControlInfo.dvbInfo[which].showInfo;
		offair_setInfoUpdateTimer(which, appControlInfo.dvbInfo[which].showInfo);
		interface_displayMenu(1);
#endif
		return 0;
	} else if (button == interfacePlayControlAudioTracks)
	{
		//dprintf("%S: request change tracks\n", __FUNCTION__);
		if (dvb_getAudioCountForService(offair_services[appControlInfo.dvbInfo[which].channel].service) > 0)
		{
			//dprintf("%s: display change tracks\n", __FUNCTION__);
			offair_audioChange(interfaceInfo.currentMenu, NULL, (void *)STREAM_INFO_SET(which, 0));
		}
		interface_displayMenu(1);
		return 0;
	}else if (button == interfacePlayControlAddToPlaylist)
	{
		dprintf("%s: add to playlist %d\n", __FUNCTION__, appControlInfo.dvbInfo[which].channel);
		dvb_getServiceURL(offair_services[appControlInfo.dvbInfo[which].channel].service, desc);
		eprintf("offair: Add to Playlist '%s'\n", desc);
		playlist_addUrl(desc, dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service));
	} else if (button == interfacePlayControlMode )
	{
		if ( offair_scheduleCheck(appControlInfo.dvbInfo[which].channel) == 0 )
		{
			offair_showSchedule( interfaceInfo.currentMenu, (void*)appControlInfo.dvbInfo[which].channel );
		} else {
			interface_showMessageBox( _T("EPG_UNAVAILABLE"), thumbnail_epg ,3000);
		}
	}
#ifdef ENABLE_PVR
	else if (button == interfaceCommandRecord)
	{
		if( appControlInfo.pvrInfo.dvb.channel >= 0 )
		{
			pvr_stopRecordingDVB(which);
		} else
		{
			pvr_recordNow();
		}
	}
#endif
	else
	{
		// default action
		return 1;
	}

	interface_displayMenu(1);

	dprintf("%s: done\n", __FUNCTION__);

	return 0;
}

#ifdef ENABLE_MULTI_VIEW
static int offair_multiviewPlay( interfaceMenu_t *pMenu, void *pArg)
{
	__u32 f1 = 0,f;
	int mvCount, i;
	int payload[4];
	FILE *file;
	DvbParam_t param;
	int channelNumber = CHANNEL_INFO_GET_CHANNEL((int)pArg);

	if( dvb_getServiceFrequency(offair_services[channelNumber].service, &f1) != 0 || f1 == 0 )
	{
		eprintf("offair: Can't determine frequency of channel %d - can't start multiview!\n", channelNumber);
		return -1;
	}
	gfx_stopVideoProviders(screenMain);

	param.mode = DvbMode_Multi;
	param.frequency = f1;
	appControlInfo.multiviewInfo.source = streamSourceDVB;
	appControlInfo.multiviewInfo.pArg[0] = (void*)CHANNEL_INFO_SET(screenMain, channelNumber);
	dvb_getPIDs(offair_services[channelNumber].service,-1,&param.param.multiParam.channels[0],NULL,NULL,NULL);
	payload[0] = dvb_hasPayloadType( offair_services[channelNumber].service, payloadTypeH264 ) ? payloadTypeH264 : payloadTypeMpeg2;
	param.media = &offair_services[channelNumber].service->media;
	mvCount = 1;
	for( i = channelNumber+1; mvCount < 4 && i < offair_serviceCount; i++ )
	{
		if( offair_services[i].service != NULL && (dvb_getScrambled(offair_services[i].service) == 0 || appControlInfo.offairInfo.dvbShowScrambled == SCRAMBLED_PLAY) && dvb_getServiceFrequency(offair_services[i].service, &f) == 0 &&
			f == f1 && dvb_hasMediaType( offair_services[i].service, mediaTypeVideo ) )
		{
			appControlInfo.multiviewInfo.pArg[mvCount] = (void*)CHANNEL_INFO_SET(screenMain, i);
			dvb_getPIDs(offair_services[i].service,-1,&param.param.multiParam.channels[mvCount],NULL,NULL,NULL);
			payload[mvCount] = dvb_hasPayloadType( offair_services[i].service, payloadTypeH264 ) ? payloadTypeH264 : payloadTypeMpeg2;
			mvCount++;
		}
	}
	for( i = 0; mvCount < 4 && i<channelNumber; i++ )
	{
		if( offair_services[i].service != NULL && (dvb_getScrambled(offair_services[i].service) == 0 || appControlInfo.offairInfo.dvbShowScrambled == SCRAMBLED_PLAY) && dvb_getServiceFrequency(offair_services[i].service, &f) == 0 &&
			f == f1 && dvb_hasMediaType( offair_services[i].service, mediaTypeVideo ) )
		{
			appControlInfo.multiviewInfo.pArg[mvCount] = (void*)CHANNEL_INFO_SET(screenMain, i);
			dvb_getPIDs(offair_services[i].service,-1,&param.param.multiParam.channels[mvCount],NULL,NULL,NULL);
			payload[mvCount] = dvb_hasPayloadType( offair_services[i].service, payloadTypeH264 ) ? payloadTypeH264 : payloadTypeMpeg2;
			mvCount++;
		}
	}
	for( i = mvCount; i < 4; i++ )
	{
		appControlInfo.multiviewInfo.pArg[i] = NULL;
		param.param.multiParam.channels[i] = 0;
	}
	appControlInfo.multiviewInfo.selected = 0;
	appControlInfo.multiviewInfo.count = mvCount;

	param.vmsp = offair_getTuner();
	param.directory = NULL;
	file = fopen(OFFAIR_MULTIVIEW_INFOFILE, "w");
	fprintf(file, "%d %d %d %d %d %d %d %d",
		payload[0], param.param.multiParam.channels[0],
		payload[1], param.param.multiParam.channels[1],
		payload[2], param.param.multiParam.channels[2],
		payload[3], param.param.multiParam.channels[3]);
	fclose(file);
	remove(OFFAIR_MULTIVIEW_FILENAME);
	mkfifo(OFFAIR_MULTIVIEW_FILENAME, S_IRUSR | S_IWUSR);

	appControlInfo.dvbInfo[screenMain].showInfo = 0;
	offair_setInfoUpdateTimer(screenMain, 0);

	offair_startDvbVideo(screenMain, &param, 0,0,0,payload[0],TXT);
	return 0;
}

static int offair_multiviewNext(int direction, void* pArg)
{
	int indexdiff = direction == 0 ? 1 : -1;
	int channelIndex = CHANNEL_INFO_GET_CHANNEL((int)pArg);
	int newIndex = channelIndex;
	int i;
	for( i = 0; i < (indexdiff > 0 ? appControlInfo.multiviewInfo.count : 4); i++)
	{
		do
		{
			newIndex = (newIndex + offair_serviceCount + indexdiff) % offair_serviceCount;
		} while( newIndex != channelIndex && (offair_services[newIndex].service == NULL ||
			dvb_hasMediaType(offair_services[newIndex].service, mediaTypeVideo) == 0 || (dvb_getScrambled(offair_services[newIndex].service) != 0 && appControlInfo.offairInfo.dvbShowScrambled != SCRAMBLED_PLAY)) );
		if( newIndex == channelIndex )
		{
			return -1;
		}
	}

	return offair_multiviewPlay( interfaceInfo.currentMenu, (void*)CHANNEL_INFO_SET(screenMain, newIndex) );
}

static int offair_multi_callback(pinterfaceCommandEvent_t cmd, void* pArg)
{

	dprintf("%s: in\n", __FUNCTION__);

	switch( cmd->command )
	{
		case interfaceCommandStop:
			offair_stopVideo(screenMain, 1);
			interface_showMenu(1, 1);
			break;
		case interfaceCommandOk:
		case interfaceCommandEnter:
			offair_channelChange(interfaceInfo.currentMenu, appControlInfo.multiviewInfo.pArg[appControlInfo.multiviewInfo.selected]);
			break;
		default:
			return interface_multiviewProcessCommand(cmd, pArg);
	}

	interface_displayMenu(1);
	return 0;
}

static void offair_displayMultiviewControl()
{
	int i, x, y, w;
	char number[4];
	int number_len;

	if( appControlInfo.multiviewInfo.count <= 0 || interfaceInfo.showMenu )
		return;
#if 0
	w = interfaceInfo.screenWidth / 2;
	switch( appControlInfo.multiviewInfo.selected )
	{
		case 1:  x = w; y = 0; break;
		case 2:  x = 0; y = interfaceInfo.screenHeight / 2; break;
		case 3:  x = w; y = interfaceInfo.screenHeight / 2; break;
		default: x = 0; y = 0;
	}
	if( appControlInfo.multiviewInfo.selected > 1 )
	{
		gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, x, y, w, interfaceInfo.paddingSize);
	} else
	{
		gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, x, y - interfaceInfo.paddingSize+interfaceInfo.screenHeight / 2, w, interfaceInfo.paddingSize);
	}
	if( appControlInfo.multiviewInfo.selected % 2 == 1 )
	{
		gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, x, y, interfaceInfo.paddingSize, interfaceInfo.screenHeight / 2);
	} else
	{
		gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, x + w - interfaceInfo.paddingSize, y, interfaceInfo.paddingSize, interfaceInfo.screenHeight / 2);
	}
#else
	switch( appControlInfo.multiviewInfo.selected )
	{
		case 1:  x = INTERFACE_ROUND_CORNER_RADIUS + interfaceInfo.screenWidth/2; y = interfaceInfo.marginSize; break;
		case 2:  x = interfaceInfo.marginSize; y = INTERFACE_ROUND_CORNER_RADIUS + interfaceInfo.screenHeight/2; break;
		case 3:  x = INTERFACE_ROUND_CORNER_RADIUS + interfaceInfo.screenWidth/2; y = INTERFACE_ROUND_CORNER_RADIUS + interfaceInfo.screenHeight/2; break;
		default: x = interfaceInfo.marginSize; y = interfaceInfo.marginSize;
	}
	snprintf(number, sizeof(number), "%03d", CHANNEL_INFO_GET_CHANNEL((int)appControlInfo.multiviewInfo.pArg[appControlInfo.multiviewInfo.selected]));
	number[sizeof(number)-1] = 0;
	number_len = strlen(number);
	w = INTERFACE_CLOCK_DIGIT_WIDTH*number_len;

	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );

	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x+INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, w-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);

	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_ROUND_CORNER_RADIUS/2, w+INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS);

	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x+INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, w-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);

	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_NOFX) );
	DFBCHECK( DRAWING_SURFACE->SetColor(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA) );
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x-INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x+w-INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 1, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x+w-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 1, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

	for( i=0; i<number_len; i++ )
	{
		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "digits.png", x, y, INTERFACE_CLOCK_DIGIT_WIDTH, INTERFACE_CLOCK_DIGIT_HEIGHT, 0, number[i]-'0', DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop);
		x += INTERFACE_CLOCK_DIGIT_WIDTH;
	}
#endif
}
#endif

void offair_displayPlayControl()
{
	int x, y, w, h, fh, fa, sfh, adv, color;
	DFBRectangle clip, rect;
	float value;
	uint16_t snr, signal;
	uint32_t ber, uncorrected_blocks;
	fe_status_t status;
	int stepsize;
	int step;
	int cindex;
	char buffer[MAX_MESSAGE_BOX_LENGTH] = "";
	int colors[4][4] =	{
		{0xFF, 0x00, 0x00, 0xFF},
		{0xFF, 0xFF, 0x00, 0xFF},
		{0x00, 0xFF, 0x00, 0xFF},
		{0x00, 0xFF, 0x00, 0xFF},
	};

	if( interfaceChannelControl.pSet != NULL && interfaceChannelControl.showingLength > 0 )
	{
		int i;

		//interface_displayTextBox( interfaceInfo.screenWidth - interfaceInfo.marginSize + interfaceInfo.paddingSize + 22, interfaceInfo.marginSize, interfacePlayControl.channelNumber, NULL, 0, NULL, 0 );

		w = INTERFACE_CLOCK_DIGIT_WIDTH*interfaceChannelControl.showingLength;
		x = interfaceInfo.screenWidth - interfaceInfo.marginSize - INTERFACE_CLOCK_DIGIT_WIDTH*3 - INTERFACE_ROUND_CORNER_RADIUS/2;
		y = interfaceInfo.marginSize;

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x+INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, w-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_ROUND_CORNER_RADIUS/2, w+INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS);

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, x+INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, w-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_NOFX) );
		DFBCHECK( DRAWING_SURFACE->SetColor(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA) );
		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x-INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x+w-INTERFACE_ROUND_CORNER_RADIUS/2, y-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 1, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", x+w-INTERFACE_ROUND_CORNER_RADIUS/2, y+INTERFACE_CLOCK_DIGIT_HEIGHT-INTERFACE_ROUND_CORNER_RADIUS/2, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 1, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

		for( i=0; i<interfaceChannelControl.showingLength; i++ )
		{
			interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "digits.png", x, y, INTERFACE_CLOCK_DIGIT_WIDTH, INTERFACE_CLOCK_DIGIT_HEIGHT, 0, interfaceChannelControl.number[i]-'0', DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop);
			x += INTERFACE_CLOCK_DIGIT_WIDTH;
		}
	}

	if ( (!interfaceInfo.showMenu &&
	      ( (interfacePlayControl.enabled && interfacePlayControl.visibleFlag) ||
	         interfacePlayControl.showState || appControlInfo.dvbInfo[screenMain].reportedSignalStatus
#ifdef ENABLE_TELETEXT
	        || (appControlInfo.teletextInfo.exists && interfaceInfo.teletext.show && (!appControlInfo.teletextInfo.subtitleFlag))
#endif
	      )
		 ) && offair_services[appControlInfo.dvbInfo[screenMain].channel].service != NULL )
	{
		DFBCHECK( pgfx_font->GetHeight(pgfx_font, &fh) );
		DFBCHECK( pgfx_font->GetAscender(pgfx_font, &fa) );
		DFBCHECK( pgfx_smallfont->GetHeight(pgfx_smallfont, &sfh) );

		w = interfaceInfo.clientWidth;
		h = sfh*4+fh+interfaceInfo.paddingSize*2;

		x = interfaceInfo.clientX;
		y = interfaceInfo.screenHeight-interfaceInfo.marginSize-h;

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
		interface_drawRoundBoxColor(x, y, w, h, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA);

		status = dvb_getSignalInfo(appControlInfo.dvbInfo[screenMain].tuner, &snr, &signal, &ber, &uncorrected_blocks);

		signal &= 0xFF;

		rect.x = x;
		rect.y = y;
		rect.w = w/4;
		rect.h = fh;

		value = (float)(signal)/(float)(MAX_SIGNAL);
		if (value < 1.0f)
		{
			color = 0xb9;
			gfx_drawRectangle(DRAWING_SURFACE, color, color, color, 0xFF, rect.x, rect.y, rect.w, rect.h);
		}

		clip.x = 0;
		clip.y = 0;
		clip.w = rect.w*value;
		clip.h = rect.h;

		/* Leave at least a small part of slider */
		if (clip.w == 0)
		{
			clip.w = rect.w*4/100;
		}

		stepsize = (MAX_SIGNAL)/3;
		step = signal/stepsize;

		cindex = step;

		//interface_drawOuterBorder(DRAWING_SURFACE, 0xFF, 0xFF, 0xFF, 0xFF, rect.x, rect.y, rect.w, rect.h, 2, interfaceBorderSideAll);

		/*interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, rect.x-2, rect.y-2, rect.w+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideBottom|interfaceBorderSideRight);
		interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, rect.x-2, rect.y-2, rect.w+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideTop|interfaceBorderSideLeft);*/

		gfx_drawRectangle(DRAWING_SURFACE, colors[cindex][0], colors[cindex][1], colors[cindex][2], colors[cindex][3], rect.x, rect.y, clip.w, clip.h);

		sprintf(buffer, "% 4d%%", signal*100/MAX_SIGNAL);

		DFBCHECK( pgfx_font->GetStringWidth (pgfx_font, buffer, -1, &adv) );

		color = 0x00; //signal*2/MAX_SIGNAL > 0 ? 0x00 : 0xFF;

		gfx_drawText(DRAWING_SURFACE, pgfx_font, color, color, color, 0xFF, rect.x+rect.w/2-adv/2, rect.y+fa, buffer, 0, 0);

		//interface_drawOuterBorder(DRAWING_SURFACE, 0xFF, 0xFF, 0xFF, 0xFF, rect.x+rect.w+interfaceInfo.paddingSize, rect.y, w-rect.w-interfaceInfo.paddingSize*2, rect.h, 2, interfaceBorderSideAll);

		/*interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, rect.x+rect.w+interfaceInfo.paddingSize*3-2, rect.y-2, w-rect.w-interfaceInfo.paddingSize*3+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideBottom|interfaceBorderSideRight);
		interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, rect.x+rect.w+interfaceInfo.paddingSize*3-2, rect.y-2, w-rect.w-interfaceInfo.paddingSize*3+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideTop|interfaceBorderSideLeft);*/

		strcpy(buffer, dvb_getServiceName(offair_services[appControlInfo.dvbInfo[screenMain].channel].service));
		buffer[getMaxStringLengthForFont(pgfx_font, buffer, w-rect.w-DVBPC_STATUS_ICON_SIZE-interfaceInfo.paddingSize*3)] = 0;

#ifdef ENABLE_PVR
		if( appControlInfo.pvrInfo.dvb.channel >= 0 )
		{
			interface_drawImage(DRAWING_SURFACE, IMAGE_DIR "icon_record.png", rect.x+rect.w+interfaceInfo.paddingSize, rect.y+fh/2, DVBPC_STATUS_ICON_SIZE, DVBPC_STATUS_ICON_SIZE, 1, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignMiddle, NULL, NULL);
		}
#endif

		gfx_drawText(DRAWING_SURFACE, pgfx_font, 0xFF, 0xFF, 0xFF, 0xFF, rect.x+rect.w+DVBPC_STATUS_ICON_SIZE+interfaceInfo.paddingSize*2, rect.y+fa, buffer, 0, 0);
		//interface_drawTextWW(pgfx_font, 0xFF, 0xFF, 0xFF, 0xFF, rect.x+rect.w+interfaceInfo.paddingSize*3, rect.y+fh, w-rect.w-interfaceInfo.paddingSize*3, 10, buffer, ALIGN_LEFT);

		rect.x = x;
		rect.w = w;
		rect.y += rect.h+interfaceInfo.paddingSize*2;
		rect.h = h-rect.h-interfaceInfo.paddingSize*2;

		/*interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, rect.x-2, rect.y-2, w+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideBottom|interfaceBorderSideRight);
		interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, rect.x-2, rect.y-2, w+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideTop|interfaceBorderSideLeft);*/

		interface_drawOuterBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, rect.x-2, rect.y-2, w+4, rect.h+4, interfaceInfo.borderWidth, interfaceBorderSideTop|interfaceBorderSideBottom);

		offair_getServiceDescription(offair_services[appControlInfo.dvbInfo[screenMain].channel].service, buffer, NULL);

		interface_drawTextWW(pgfx_smallfont, 0xFF, 0xFF, 0xFF, 0xFF, rect.x+interfaceInfo.paddingSize, rect.y, rect.w-interfaceInfo.paddingSize, rect.h, buffer, ALIGN_LEFT);

		interface_addEvent((eventActionFunction)interface_displayMenu, (void*)1, 100, 1);
	}

	interface_slideshowControlDisplay();
}

static int offair_playControlProcessCommand(pinterfaceCommandEvent_t cmd, void *pArg)
{
	int sFlag=0;
	dprintf("%s: in 0x%08X\n", __FUNCTION__, cmd);

#ifdef ENABLE_TELETEXT
	if(appControlInfo.teletextInfo.exists)
	{
		if(cmd->command == interfaceCommandMic)
		{
			if(interfaceInfo.teletext.show == 0)
				interfaceInfo.teletext.show = 1;
			else
			{
				interfaceInfo.teletext.show = 0;
				appControlInfo.teletextInfo.subtitleFlag = 0;
				if(appControlInfo.teletextInfo.status == teletextStatus_demand)
					appControlInfo.teletextInfo.status = teletextStatus_processing;
			}
			appControlInfo.teletextInfo.freshCounter = 0;
			interfaceInfo.teletext.pageNumber = 100;
		}

		if(interfaceInfo.teletext.show && (!appControlInfo.teletextInfo.subtitleFlag))
		{
			if((cmd->command >= interfaceCommand0) && (cmd->command <= interfaceCommand9))
			{
				int i;
				switch (cmd->command)
				{
					case interfaceCommand1:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=1;
						break;
					case interfaceCommand2:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=2;
						break;
					case interfaceCommand3:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=3;
						break;
					case interfaceCommand4:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=4;
						break;
					case interfaceCommand5:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=5;
						break;
					case interfaceCommand6:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=6;
						break;
					case interfaceCommand7:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=7;
						break;
					case interfaceCommand8:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=8;
						break;
					case interfaceCommand9:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=9;
						break;
					case interfaceCommand0:
						appControlInfo.teletextInfo.fresh[appControlInfo.teletextInfo.freshCounter]=0;
						break;
					default:
						break;
				}
				appControlInfo.teletextInfo.freshCounter++;

				for(i=0;i<appControlInfo.teletextInfo.freshCounter;i++)
				{
					appControlInfo.teletextInfo.text[interfaceInfo.teletext.pageNumber][0][4+i]=
					appControlInfo.teletextInfo.fresh[i]+48;
				}

				for(i=appControlInfo.teletextInfo.freshCounter;i<3;i++)
				{
					appControlInfo.teletextInfo.text[interfaceInfo.teletext.pageNumber][0][4+i]=' ';
				}

				if(appControlInfo.teletextInfo.freshCounter==3)
				{
					interfaceInfo.teletext.pageNumber=appControlInfo.teletextInfo.fresh[0]*100+
						appControlInfo.teletextInfo.fresh[1]*10+
						appControlInfo.teletextInfo.fresh[2];
					appControlInfo.teletextInfo.freshCounter=0;

					if(interfaceInfo.teletext.pageNumber == appControlInfo.teletextInfo.subtitlePage)
					{
						appControlInfo.teletextInfo.subtitleFlag=1;
						sFlag=1;
					}
					else if (appControlInfo.teletextInfo.status != teletextStatus_ready)
						appControlInfo.teletextInfo.status = teletextStatus_demand;
				}
			}
			else if(appControlInfo.teletextInfo.status >= teletextStatus_demand)
			{
				switch (cmd->command)
				{
					case interfaceCommandLeft:
					case interfaceCommandBlue:
						interfaceInfo.teletext.pageNumber = appControlInfo.teletextInfo.previousPage;
						break;
					case interfaceCommandRight:
					case interfaceCommandRed:
						interfaceInfo.teletext.pageNumber = appControlInfo.teletextInfo.nextPage[0];
						break;
					case interfaceCommandGreen:
						interfaceInfo.teletext.pageNumber = appControlInfo.teletextInfo.nextPage[1];
						break;
					case interfaceCommandYellow:
						interfaceInfo.teletext.pageNumber = appControlInfo.teletextInfo.nextPage[2];
						break;
					default:
						break;
				}
				if(interfaceInfo.teletext.pageNumber == appControlInfo.teletextInfo.subtitlePage)
				{
					appControlInfo.teletextInfo.subtitleFlag=1;
					sFlag=1;
				}
			}
		}
	}
#endif // #ifdef ENABLE_TELETEXT
#ifdef ENABLE_PVR
	if(cmd->command == interfaceCommandRecord)
	{
		if( appControlInfo.pvrInfo.dvb.channel >= 0 )
		{
			pvr_stopRecordingDVB(screenMain);
		} else
		{
			pvr_recordNow();
		}
		return 0;
	}
#endif
	if (cmd->source == DID_FRONTPANEL)
	{
		switch (cmd->command)
		{
			case interfaceCommandRight:
				cmd->command = interfaceCommandVolumeUp;
				interface_soundControlProcessCommand(cmd);
				return 0;
			case interfaceCommandLeft:
				cmd->command = interfaceCommandVolumeDown;
				interface_soundControlProcessCommand(cmd);
				return 0;
			case interfaceCommandDown: cmd->command = interfaceCommandChannelDown; break;
			case interfaceCommandUp: cmd->command = interfaceCommandChannelUp; break;
			case interfaceCommandEnter: cmd->command = interfaceCommandBack; break;
			default:;
		}
	}

	if ((
		  cmd->command == interfaceCommandExit ||
		  cmd->command == interfaceCommandBack ||
		  cmd->command == interfaceCommandChannelUp ||
		  cmd->command == interfaceCommandChannelDown ||
		  cmd->command == interfaceCommandRecord ||
		  cmd->command == interfaceCommandPlay ||
		 (cmd->command == interfaceCommandStop && cmd->source == DID_STANDBY) ||
		  cmd->command == interfaceCommandRed ||
		  cmd->command == interfaceCommandGreen ||
		  cmd->command == interfaceCommandYellow ||
		  cmd->command == interfaceCommandBlue ||
		  cmd->command == interfaceCommandRefresh ||
		 (cmd->command >= interfaceCommand0 && cmd->command <= interfaceCommand9) ||
		  cmd->command == interfaceCommandTV
		) && (!sFlag))
	{
		if (appControlInfo.dvbInfo[screenMain].showInfo && cmd->command == interfaceCommandExit)
		{
			appControlInfo.dvbInfo[screenMain].showInfo = 0;
			offair_setInfoUpdateTimer(screenMain, 0);
			return 0;
		}

#ifdef ENABLE_MULTI_VIEW
		if( cmd->command == interfaceCommandTV)
		{
			int which = CHANNEL_INFO_GET_SCREEN((int)pArg);
			if( 
#ifdef ENABLE_PVR
			    appControlInfo.pvrInfo.dvb.channel < 0 &&
#endif
			    dvb_hasMediaType(offair_services[appControlInfo.dvbInfo[which].channel].service, mediaTypeVideo) == 1 && (dvb_getScrambled(offair_services[appControlInfo.dvbInfo[which].channel].service) == 0 || appControlInfo.offairInfo.dvbShowScrambled == SCRAMBLED_PLAY)
			  )
			{
				offair_multiviewPlay(interfaceInfo.currentMenu, (void *)STREAM_INFO_SET(which, appControlInfo.dvbInfo[which].channel));
				return 0;
			}
		}
#endif

		return 1;
	}

	dprintf("%s: break\n", __FUNCTION__);

	if( appControlInfo.teletextInfo.subtitleFlag == 0 )
		interface_playControlRefresh(1);
	else
		interface_playControlHide(1);

	return 0;
}

void offair_startVideo(int which)
{
	int audio_type;
	int video_type;
	DvbParam_t param;
	param.frequency = 0;
	param.mode = DvbMode_Watch;
	param.vmsp = offair_getTuner();
	param.param.liveParam.channelIndex = dvb_getServiceIndex(offair_services[appControlInfo.dvbInfo[which].channel].service);
	param.param.liveParam.audioIndex = appControlInfo.dvbInfo[which].audio_track;
	param.directory = NULL;

	if (offair_services[appControlInfo.dvbInfo[which].channel].service == NULL)
	{
		eprintf("offair: Failed to start channel %d: offair service is NULL\n", appControlInfo.dvbInfo[which].channel);
		return;
	}

	if (offair_services[appControlInfo.dvbInfo[which].channel].service->program_map.map.streams == NULL ||
		(offair_services[appControlInfo.dvbInfo[which].channel].service->flags & serviceFlagHasPMT) == 0)
	{
		interface_showMessageBox(_T("DVB_SCANNING_SERVICE"), thumbnail_loading, 0);
		eprintf("offair: Channel '%s' has no PMT info, force rescan on %lu Hz, type %d\n", dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service), offair_services[appControlInfo.dvbInfo[which].channel].service->media.frequency, offair_services[appControlInfo.dvbInfo[which].channel].service->media.type);
		dvb_frequencyScan(offair_getTuner(), offair_services[appControlInfo.dvbInfo[which].channel].service->media.frequency, &offair_services[appControlInfo.dvbInfo[which].channel].service->media, NULL, NULL, 1, NULL);
		offair_fillDVBTMenu();
		offair_fillDVBTOutputMenu(screenMain);
		offair_fillDVBTOutputMenu(screenPip);
		interface_hideMessageBox();
	}

	/*if (dvb_getScrambled(offair_services[appControlInfo.dvbInfo[which].channel].service) && appControlInfo.offairInfo.dvbShowScrambled == 0)
	{
		interface_showMessageBox(_T("ERR_SCRAMBLED_CHANNEL"), thumbnail_error, 0);
		eprintf("offair: Channel '%s' is scrambled!\n", dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service));
		return;
	}*/

	if (offair_services[appControlInfo.dvbInfo[which].channel].service->program_map.map.streams == NULL ||
		(offair_services[appControlInfo.dvbInfo[which].channel].service->flags & serviceFlagHasPMT) == 0)
	{
		eprintf("offair: Channel '%s' has no PIDs!\n", dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service));
		interface_showMessageBox(_T("ERR_NO_STREAMS_IN_CHANNEL"), thumbnail_error, 0);
		return;
	}

	param.media = &offair_services[appControlInfo.dvbInfo[which].channel].service->media;

	switch ( dvb_getAudioTypeForService(offair_services[appControlInfo.dvbInfo[which].channel].service,
		appControlInfo.dvbInfo[which].audio_track) )
	{
	case payloadTypeAC3:
		audio_type = AC3;
		break;
	case payloadTypeAAC:
		audio_type = AAC;
		break;
	default:
		audio_type = MP3;
	}
	video_type = dvb_hasMediaType(offair_services[appControlInfo.dvbInfo[which].channel].service, mediaTypeVideo) ?
		( dvb_hasPayloadType( offair_services[appControlInfo.dvbInfo[which].channel].service, payloadTypeH264 ) ? H264 : MPEG2 ) : 0;

#ifdef ENABLE_DVB_DIAG
	interface_addEvent(offair_updatePSI, (void*)which, 1000, 1);
#endif

	interface_addEvent(offair_updateEPG, (void*)which, 1000, 1);
#ifdef ENABLE_STATS
	time(&statsInfo.endTime);
	interface_addEvent(offair_updateStatsEvent, (void*)which, STATS_UPDATE_INTERVAL, 1);
#endif

	offair_startDvbVideo(which,&param,0,0,audio_type, video_type, TXT);
}

static void offair_startDvbVideo(int which, DvbParam_t *param, int service_id, int ts_id, int audio_type, int video_type, int text_type)
{
	char filename[256];
	char qualifier[64];

	int ret;
	gfx_stopVideoProviders(which);

	dprintf("%s: Start video screen %s\n", __FUNCTION__, which == screenPip ? "Pip" : "Main");

	appControlInfo.dvbInfo[which].tuner = param->vmsp;
	appControlInfo.tunerInfo[appControlInfo.dvbInfo[which].tuner].status = which ? tunerDVBPip : tunerDVBMain;

#ifdef STBTI
	sprintf(filename, "ln -s /dev/dvb/adapter%d/dvr0 %s", appControlInfo.dvbInfo[which].tuner, OFFAIR_MULTIVIEW_FILENAME);
	system(filename);
	strcpy(filename, OFFAIR_MULTIVIEW_FILENAME);
#else
#ifdef ENABLE_MULTI_VIEW
	if( param->mode == DvbMode_Multi )
	{
		sprintf(filename, OFFAIR_MULTIVIEW_INFOFILE);
	} else
#endif
	{
		sprintf(filename, "/dev/dvb/adapter%d/demux0", appControlInfo.dvbInfo[which].tuner);
	}
#endif
	sprintf(qualifier, "%s%s%s%s%s",
		(which==screenPip) ? ":SD:NoSpdif:I2S1" : "",
		audio_type == AC3 ? ":AC3" : "",
		video_type == H264 ? ":H264" : ( video_type == MPEG2 ? ":MPEG2" : ""),
		audio_type == AAC ? ":AAC" : AUDIO_MPEG,
		(appControlInfo.soundInfo.rcaOutput==1) ? "" : ":I2S0");

	dprintf("%s: Qualifier: %s\n", __FUNCTION__, qualifier);

	if (video_type != 0)
	{
		media_slideshowStop(1);
	}

#ifdef ENABLE_MULTI_VIEW
	/* Multiview provider want DVB to start first (#10661) */
	if( param->mode == DvbMode_Multi )
	{
		dprintf("%s: dvb_startDVB\n", __FUNCTION__);
		dvb_startDVB(param);
	}
#endif

	ret = gfx_startVideoProvider(filename, which, 0, qualifier);

	if (ret != 0)
	{
		eprintf("offair: Failed to start video provider '%s'\n", filename);
		interface_showMessageBox(_T("ERR_VIDEO_PROVIDER"), thumbnail_error, 0);
		dvb_stopDVB(appControlInfo.dvbInfo[which].tuner, 1);
		return;
	}

#ifdef ENABLE_MULTI_VIEW
	if( param->mode != DvbMode_Multi )
#endif
	{
		dprintf("%s: dvb_startDVB\n", __FUNCTION__);
		dvb_startDVB(param);
	}

	if (dvb_getScrambled(offair_services[appControlInfo.dvbInfo[which].channel].service) != 0 && appControlInfo.offairInfo.dvbShowScrambled != SCRAMBLED_PLAY)
	{
		// FIXME: Need demuxer without decoder to collect statistics...
		eprintf("offair: Scrambled channel and dvbShowScrambled != SCRAMBLED_PLAY!\n");
	}

	dprintf("%s: dvb_hasVideo == %d\n", __FUNCTION__,video_type);
	if (video_type == 0 && appControlInfo.slideshowInfo.state == slideshowDisabled)
	{
		interface_setBackground(0,0,0,0xFF, INTERFACE_WALLPAPER_IMAGE);//IMAGE_DIR "wallpaper_audio.png");
	}

	appControlInfo.dvbInfo[which].active = 1;
	appControlInfo.dvbInfo[which].scanPSI = 1;
	appControlInfo.dvbInfo[which].lastSignalStatus = signalStatusNoStatus;
	appControlInfo.dvbInfo[which].savedSignalStatus = signalStatusNoStatus;
	appControlInfo.dvbInfo[which].reportedSignalStatus = 0;

	offair_setStateCheckTimer(which, 1);
	offair_setInfoUpdateTimer(which, 1);

	/*if (which == screenMain)
	{
		offair_fillDVBTMenu();
	}
	else
	{
		offair_initDVBTOutputMenu(which);
	}*/

#ifdef ENABLE_MULTI_VIEW
	if( param->mode == DvbMode_Multi )
	{
		dprintf("%s: multiview mode\n", __FUNCTION__);
		interface_playControlSetup(NULL, appControlInfo.multiviewInfo.pArg[0], 0, NULL, thumbnail_channels);
		interface_playControlSetDisplayFunction(offair_displayMultiviewControl);
		interface_playControlSetProcessCommand(offair_multi_callback);
		interface_playControlSetChannelCallbacks(offair_multiviewNext, offair_setChannel);
		interface_playControlRefresh(0);
		interface_showMenu(0, 1);
	}
#endif

	interface_playControlSelect(interfacePlayControlStop);
	interface_playControlSelect(interfacePlayControlPlay);
	interface_displayMenu(1);

	dprintf("%s: done\n", __FUNCTION__);
}

#if 0
static int offair_startStopDVB(interfaceMenu_t *pMenu, void* pArg)
{
	if ( appControlInfo.dvbInfo[(int)pArg].active )
	{
		offair_stopVideo((int)pArg);
	}
	else
	{
		offair_startVideo((int)pArg);
	}
	//interface_menuActionShowMenu(pMenu, (void*)&rtpStreamMenu[which]);
	//menuInfra_display((void*)&DVBTOutputMenu[(int)pArg]);
	return 0;
}

static int offair_debugToggle(interfaceMenu_t *pMenu, void* pArg)
{
	if ( appControlInfo.offairInfo.tunerDebug )
	{
		appControlInfo.offairInfo.tunerDebug = 0;
	}
	else
	{
		appControlInfo.offairInfo.tunerDebug = 1;
	}
	offair_fillDVBTMenu();
	interface_displayMenu(1);
	/*interface_setSelectedItem()
	menuInfra_setHighlight(&DVBTMenu, 3);
	menuInfra_display((void*)&DVBTMenu);*/
	return 0;
}

static void offair_setRTC(void)
{
	int file;

	/* Open the Real Time Clock device */
	if ((file = open("/dev/rtc", O_RDWR)) >= 0)
	{
		struct tm time;

		time.tm_year = appControlInfo.timeInfo.year;
		time.tm_mon  = appControlInfo.timeInfo.month;
		time.tm_mday = appControlInfo.timeInfo.day;
		time.tm_hour = appControlInfo.timeInfo.hour;
		time.tm_min  = appControlInfo.timeInfo.minute;
		time.tm_sec  = appControlInfo.timeInfo.second;

		/* Perform the RTC_SET_TIME ioctl to set the RTC time */
		if (ioctl(file, RTC_SET_TIME, &time) < 0)
		{
			close(file);
			eprintf("offair: Set time command failed\n");
		}
		close(file);
	}
}

static int offair_getTime(interfaceMenu_t *pMenu, void* pArg)
{
	/* Extract the date and time information from the broadcast */
	stb810_getTimeInfo(&appControlInfo.timeInfo);
	/* Might a well update the RTC too! */
	offair_setRTC();
	/* Now update the display */
	//menuInfra_display((void*)&DVBTMenu);
	return 0;
}


#endif

static void offair_getServiceDescription(EIT_service_t *service, char *desc, char *mode)
{
	list_element_t *event_element;
	EIT_event_t *event;
	time_t start_time;
	char *str;

	if( service != NULL )
	{
		if (mode != NULL)
		{
			sprintf(desc, "%s: %s",mode, dvb_getServiceName(service));
		} else
		{
			desc[0] = 0;
		}

		offair_sortEvents(&service->present_following);
		event_element = service->present_following;
		while ( event_element != NULL )
		{
			event = (EIT_event_t*)event_element->data;
			switch(event->running_status)
			{
			case 2:
				//str = _T("STARTS_IN_FEW_SECONDS"); break;
			case 1:
				str = _T("NEXT");
				break;
			case 3:
				//str = _T("PAUSING"); break;
			case 4:
				str = _T("PLAYING");
				break;
			default: str = "";
			}
			sprintf( &desc[strlen(desc)], "%s%s ", desc[0] == 0 ? "" : "\n", str);
			offair_getLocalEventTime( event, NULL, &start_time );
			start_time += offair_getEventDuration( event );
			strftime(&desc[strlen(desc)], 11, "(%T)", localtime( &start_time ));
			sprintf( &desc[strlen(desc)], " %s", event->description.event_name );
			/*if (event->description.text[0] != 0)
			{
				sprintf( &desc[strlen(desc)], ". %s", event->description.text );
			}*/

			event_element = event_element->next;
		}
	} else
	{
		strcpy( desc, _T("RECORDING"));
	}
}

#ifdef ENABLE_PVR
int offair_startPvrVideo( int which )
{
	int ret;
	char desc[BUFFER_SIZE];
	int buttons;

	//sprintf(filename, "%s/info.pvr", appControlInfo.pvrInfo.playbackInfo[which].directory);
	if (helperFileExists(STBPVR_PIPE_FILE ".dummy" ))
	{
		gfx_stopVideoProviders(which);

		media_slideshowStop(1);

		dprintf("%s: start provider %s\n", __FUNCTION__, STBPVR_PIPE_FILE ".dummy");

		ret = gfx_startVideoProvider(STBPVR_PIPE_FILE ".dummy", which, 1, NULL);

		dprintf("%s: video provider started with return code %d\n", __FUNCTION__, ret);

		if ( ret != 0 )
		{
			interface_showMessageBox(_T("ERR_VIDEO_PROVIDER"), thumbnail_error, 0);
			eprintf("offair: Failed to start video provider %s\n", STBPVR_PIPE_FILE ".dummy");
		} else
		{
			buttons = interfacePlayControlStop|interfacePlayControlPlay|interfacePlayControlPrevious|interfacePlayControlNext;
			buttons |= appControlInfo.playbackInfo.playlistMode != playlistModeFavorites ? interfacePlayControlAddToPlaylist :  interfacePlayControlMode;

			offair_getServiceDescription(dvb_getService(appControlInfo.pvrInfo.dvb.channel),desc,_T("RECORDING"));

			appControlInfo.dvbInfo[which].active = 1;
			interface_playControlSetup(offair_play_callback, (void*)which, buttons, desc, thumbnail_recording);
			interface_playControlSetDisplayFunction(offair_displayPlayControl);
			interface_playControlSetProcessCommand(offair_playControlProcessCommand);
			interface_playControlSetChannelCallbacks(offair_startNextChannel, offair_setChannel);
			interface_playControlSelect(interfacePlayControlStop);
			interface_playControlSelect(interfacePlayControlPlay);
			interface_playControlRefresh(0);
			interface_showMenu(0, 1);
		}
	} else
	{
		ret = -1;
		eprintf("offair: %s do not exist\n", STBPVR_PIPE_FILE ".dummy");
	}
	return ret;
}
#endif

int offair_channelChange(interfaceMenu_t *pMenu, void* pArg)
{
	int screen = CHANNEL_INFO_GET_SCREEN((int)pArg);
	int channelNumber = CHANNEL_INFO_GET_CHANNEL((int)pArg);
	char desc[BUFFER_SIZE];
	int buttons;

	dprintf("%s: in %d\n", __FUNCTION__, channelNumber);

	if ( dvb_getServiceID(offair_services[channelNumber].service) == -1 )
	{
		eprintf("offair: Unknown service for channel %d\n", channelNumber);
		return -1;
	}

	if (/*(appControlInfo.offairInfo.dvbShowScrambled == 0 && dvb_getScrambled( offair_services[channelNumber].service )) || */!dvb_hasMedia(offair_services[channelNumber].service))
	{
		eprintf("offair: Scrambled or media-less service ignored %d\n", channelNumber);
		return -1;
	}

	dprintf("%s: screen %s to %d\n", __FUNCTION__, screen == screenPip ? "Pip" : "Main", channelNumber);
#ifdef ENABLE_PVR
	if ( appControlInfo.pvrInfo.dvb.channel >= 0 )
	{
		if( offair_getIndex(appControlInfo.pvrInfo.dvb.channel) == channelNumber )
		{
			pvr_startPlaybackDVB(screenMain);
		} else
		{
			pvr_showStopPvr( pMenu, (void*)channelNumber );
		}
		return 0;
	}
#endif
	if ( appControlInfo.dvbInfo[screen].active != 0 )
	{
		//interface_playControlSelect(interfacePlayControlStop);
		// force showState to NOT be triggered
		interfacePlayControl.activeButton = interfacePlayControlStop;
		offair_stopVideo(screen, 0);
	}

	appControlInfo.playbackInfo.playlistMode = playlistModeNone;
	appControlInfo.playbackInfo.streamSource = streamSourceDVB;
	appControlInfo.dvbInfo[screen].channel = channelNumber;
	appControlInfo.dvbInfo[screen].audio_track = 0;
	appControlInfo.dvbInfo[screen].scrambled = dvb_getScrambled(offair_services[channelNumber].service);

	buttons = interfacePlayControlStop|interfacePlayControlPlay|interfacePlayControlPrevious|interfacePlayControlNext;
	buttons |= appControlInfo.playbackInfo.playlistMode != playlistModeFavorites ? interfacePlayControlAddToPlaylist :  interfacePlayControlMode;

	offair_getServiceDescription(offair_services[channelNumber].service,desc,_T("DVB_CHANNELS"));
	appControlInfo.playbackInfo.channel = channelNumber;

	interface_playControlSetInputFocus(inputFocusPlayControl);
	interface_playControlSetup(offair_play_callback, (void*)screen, buttons, desc, offair_services[channelNumber].service->service_descriptor.service_type == 2 ? thumbnail_radio : thumbnail_channels);
	interface_playControlSetDisplayFunction(offair_displayPlayControl);
	interface_playControlSetProcessCommand(offair_playControlProcessCommand);
	interface_playControlSetChannelCallbacks(offair_startNextChannel, offair_setChannel);
	interface_channelNumberShow(appControlInfo.playbackInfo.channel);

	offair_startVideo(screen);
	//offair_buildDVBTChannelMenu(screen);
	offair_fillDVBTMenu();
	//menuInfra_display((void*)pCurrentMenu);
	saveAppSettings();

	if ( appControlInfo.dvbInfo[screen].active != 0 )
	{
		interface_showMenu(0, 1);
	}

	//interface_menuActionShowMenu(pMenu, (void*)&DVBTMenu);

	return 0;
}

#if 0
static void offair_buildDVBTChannelMenu(int which)
{
	int position = 0;
	char channelEntry[256];
	int i;
	//DVBTChannelMenu[which].prev = &(DVBTOutputMenu[which]);
	/*menuInfra_setWidth(&DVBTChannelMenu[which], DVBT_CHANNEL_MENU_WIDTH);
	menuInfra_setPosition(&DVBTChannelMenu[which], DVBT_CHANNEL_MENU_X, DVBT_CHANNEL_MENU_Y);
	menuInfra_setHeaderEntry(&DVBTChannelMenu[which], menuEntryHeading, "Channel");
	for ( i=0; i<dvb_getNumberOfServices(); i++ )
	{
	int channelNumber;
	channelNumber = dvb_getChannelNumber(i);
	sprintf(channelEntry, "%d: %s", channelNumber, dvb_getChannelName(channelNumber));
	menuInfra_setEntry(&DVBTChannelMenu[which], position++, menuEntryText, channelEntry, offair_channelChange, (void*)CHANNEL_INFO_SET(which, channelNumber));
	}
	menuInfra_setFooterEntry(&DVBTChannelMenu[which], menuEntryImage, IMAGE_DIR "stb810.png");
	menuInfra_setSelected(&DVBTChannelMenu[which], dvb_getChannelPosition(offair_services[appControlInfo.dvbInfo[which].channel].service));*/
}
#endif

void offair_fillDVBTOutputMenu(int which)
{
	//int position = 0;
	char channelEntry[MENU_ENTRY_INFO_LENGTH];
	EIT_service_t *service;
	int i, selectedMenuItem;
	int radio, scrambled, type = -1;
	__u32 lastFreqency = 0, serviceFrequency;
	char lastChar = 0, curChar, *serviceName, *str;

	interface_clearMenuEntries((interfaceMenu_t*)&DVBTOutputMenu[which]);

	//dprintf("%s: got %d channels for layer %d\n", __FUNCTION__, dvb_getNumberOfChannels(), which);

	for( i = 0; i < offair_indexCount; ++i )
	{
		service = offair_services[offair_indeces[i]].service;
		scrambled = dvb_getScrambled( service );
		radio = service->service_descriptor.service_type == 2;
		serviceName = dvb_getServiceName(service);
		switch( appControlInfo.offairInfo.sorting )
		{
		case serviceSortAlpha:
			if( serviceName == NULL )
				break;
			curChar = toupper( *serviceName );
			if( curChar != lastChar )
			{
				channelEntry[0] = lastChar = curChar;
				channelEntry[1] = 0;
				interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTOutputMenu[which], channelEntry, 0 );
			}
			break;
		case serviceSortType:
			if( type != radio )
			{
				type = radio;
				str = _T( type ? "RADIO" : "TV" );
				interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTOutputMenu[which], str, 0 );
			}
			break;
		case serviceSortFreq:
			if( dvb_getServiceFrequency( service, &serviceFrequency ) == 0 && serviceFrequency != lastFreqency)
			{
				lastFreqency = serviceFrequency;
				sprintf( channelEntry, "%lu %s", (long unsigned int)lastFreqency / 1000000, _T("MHZ") );
				interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTOutputMenu[which], channelEntry, 0 );
			}
			break;
		default: ;
		}
		if( offair_serviceCount < 10 )
		{
			sprintf(channelEntry, "%d", offair_indeces[i]);
		} else  if ( offair_serviceCount < 100 )
		{
			sprintf(channelEntry, "%02d", offair_indeces[i]);
		} else
		{
			sprintf(channelEntry, "%03d", offair_indeces[i]);
		}
		snprintf(&channelEntry[strlen(channelEntry)], MENU_ENTRY_INFO_LENGTH - 3, ". %s", serviceName);
		channelEntry[MENU_ENTRY_INFO_LENGTH-1] = 0;
		interface_addMenuEntry((interfaceMenu_t*)&DVBTOutputMenu[which], channelEntry, offair_channelChange, (void*)CHANNEL_INFO_SET(which, offair_indeces[i]), service->program_map.map.streams == NULL ? thumbnail_not_selected : (scrambled ? thumbnail_billed : ( radio ? thumbnail_radio : thumbnail_channels)) );
	}
	if( interface_getSelectedItem( (interfaceMenu_t*)&DVBTOutputMenu[which] ) >= offair_indexCount )
	{
		interface_setSelectedItem( (interfaceMenu_t*)&DVBTOutputMenu[which], MENU_ITEM_BACK);
	}

	/*for ( i=0; i < dvb_getNumberOfServices(); i++ )
	{
	sprintf(channelEntry, "%d: %s%s %s", i, dvb_getServiceName(i), dvb_hasVideo(i) != 0 ? "" : _T("RADIO"), dvb_getScrambled(i) ? _T("SCRAMBLED") : "");
	//dprintf("%s: add %s\n", __FUNCTION__, channelEntry);
	interface_addMenuEntry((interfaceMenu_t*)&DVBTOutputMenu[which], channelEntry, offair_channelChange, (void*)CHANNEL_INFO_SET(which, i), dvb_hasVideo(i) != 0 ? IMAGE_DIR "thumbnail_channels.png" : IMAGE_DIR "thumbnail_sound.png");
	}*/

	if (offair_indexCount == 0)
	{
		strcpy(channelEntry, _T("NO_CHANNELS"));
		interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTOutputMenu[which], channelEntry, thumbnail_info);
	}

	if( appControlInfo.dvbInfo[which].channel < 0 || appControlInfo.dvbInfo[which].channel == CHANNEL_CUSTOM || offair_serviceCount == 0 )
	{
		selectedMenuItem = MENU_ITEM_BACK;
	} else {
		selectedMenuItem = 0;
		for ( i = 0; i < appControlInfo.dvbInfo[which].channel; i++)
		{
			if( offair_services[i].service != NULL )
			{
				selectedMenuItem++;
			}
		}
	}

	interface_setSelectedItem((interfaceMenu_t*)&DVBTOutputMenu[which], selectedMenuItem);
}

static void offair_initDVBTOutputMenu(interfaceMenu_t *pParent, int which)
{
	int offair_icons[4] = { 
		statusbar_f1_sorting, 
		statusbar_f2_info,
#ifdef ENABLE_FAVORITES
		statusbar_f3_add,
#else
		0,
#endif
		statusbar_f4_number };
	//int position = 0;
	createListMenu(&DVBTOutputMenu[which], which == screenMain ? _T("MAIN_LAYER") : _T("PIP_LAYER"), thumbnail_dvb, offair_icons, pParent,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&DVBTOutputMenu[which], offair_keyCallback);

	offair_fillDVBTOutputMenu(which);

	/*DVBTOutputMenu[which].prev = &DVBTMenu;
	menuInfra_setWidth(&DVBTOutputMenu[which], DVBT_OUTPUT_MENU_WIDTH);
	menuInfra_setPosition(&DVBTOutputMenu[which], DVBT_OUTPUT_MENU_X, DVBT_OUTPUT_MENU_Y);
	menuInfra_setHeaderEntry(&DVBTOutputMenu[which], menuEntryHeading, which == 0 ? "Main Display" : "PiP Display");
	menuInfra_setEntry(&DVBTOutputMenu[which], position++, menuEntryText, appControlInfo.dvbInfo[which].active ? "Stop" : "Start", offair_startStopDVB, (void*)which);
	menuInfra_setEntry(&DVBTOutputMenu[which], position++, menuEntryText, "Channel Select", menuInfra_display, (void*)&DVBTChannelMenu[which]);
	menuInfra_setFooterEntry(&DVBTOutputMenu[which], menuEntryImage, IMAGE_DIR "stb810.png");
	menuInfra_setSelected(&DVBTOutputMenu[which], NONE_SELECTED);
	offair_buildDVBTChannelMenu(which);*/
}

static int offair_setChannel(int channel, void* pArg)
{
	int which = (int)pArg;

	if( channel < 0 || channel >= offair_serviceCount || appControlInfo.dvbInfo[which].channel == channel || offair_services[channel].service == NULL )
	{
		return 1;
	}

	offair_channelChange(interfaceInfo.currentMenu, (void*)CHANNEL_INFO_SET(which, channel));

	return 0;
}

static int offair_startNextChannel(int direction, void* pArg)
{
	int i;
	int which = (int)pArg;

	if(appControlInfo.playbackInfo.playlistMode == playlistModeFavorites)
	{
		return playlist_startNextChannel(direction,(void*)-1);
	}
	dprintf("%s: %d, screen%s\n", __FUNCTION__, direction, which == screenMain ? "Main" : "Pip" );
	direction = direction == 0 ? 1 : -1;
	for(
		i = (appControlInfo.dvbInfo[which].channel + offair_serviceCount + direction ) % offair_serviceCount;
		i != appControlInfo.dvbInfo[which].channel && (offair_services[i].service == NULL || (dvb_getScrambled(offair_services[i].service) && appControlInfo.offairInfo.dvbShowScrambled != SCRAMBLED_PLAY) || !dvb_hasMedia(offair_services[i].service));
		i = (i + direction + offair_serviceCount) % offair_serviceCount );

	dprintf("%s: i = %d, ch = %d, total = %d\n", __FUNCTION__, i, appControlInfo.dvbInfo[which].channel, offair_serviceCount);

	if (i != appControlInfo.dvbInfo[which].channel)
	{
		offair_channelChange(interfaceInfo.currentMenu, (void*)CHANNEL_INFO_SET(which, i));
	}

	return 0;
}

static int offair_confirmAutoScan(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		interface_hideMessageBox();
		offair_serviceScan(pMenu,pArg);
		return 0;
	}

	return 1;
}

int offair_enterDVBTMenu(interfaceMenu_t *pMenu, void* pArg)
{
	if( dvb_getNumberOfServices() == 0 )
	{
		interface_menuActionShowMenu(pMenu,(void*)&DVBSubMenu);
		output_fillDVBMenu((interfaceMenu_t *)&DVBSubMenu, pArg);
		interface_showConfirmationBox( _T("DVB_NO_CHANNELS"), thumbnail_dvb, offair_confirmAutoScan, (void*)screenMain);
		return 1;
	}

	/* Auto play */
	if( appControlInfo.playbackInfo.bAutoPlay && gfx_videoProviderIsActive( screenMain ) == 0 && appControlInfo.slideshowInfo.state == slideshowDisabled && offair_indexCount > 0)
	{
		dprintf("%s: Auto play dvb channel = %d\n", __FUNCTION__,appControlInfo.dvbInfo[screenMain].channel);
		if( appControlInfo.dvbInfo[screenMain].channel <= 0 || appControlInfo.dvbInfo[screenMain].channel >= dvb_getNumberOfServices() )
		{
			appControlInfo.dvbInfo[screenMain].channel = offair_indeces[0];
		}
		offair_channelChange(interfaceInfo.currentMenu, (void*)CHANNEL_INFO_SET(screenMain, appControlInfo.dvbInfo[screenMain].channel));
	}

	return 0;
}

int offair_initEPGRecordMenu(interfaceMenu_t *pMenu, void *pArg)
{
	interfaceEpgMenu_t *pEpg = (interfaceEpgMenu_t *)pMenu;
	list_element_t *event_element;
	EIT_event_t *event;
	time_t event_start, event_end;
	int i, events_found;

	if( (int)pArg < 0 ) // double call fix
	{
		return 0;
	}

	pEpg->currentService = (int)pArg;
	pEpg->serviceOffset = 0;
	if( offair_services[pEpg->serviceOffset].service == NULL || offair_services[pEpg->serviceOffset].service->schedule == NULL || dvb_hasMedia(offair_services[pEpg->serviceOffset].service) == 0)
	{
		for( pEpg->serviceOffset = 0; pEpg->serviceOffset < offair_serviceCount; pEpg->serviceOffset++ )
		{
			if( offair_services[pEpg->serviceOffset].service != NULL && offair_services[pEpg->serviceOffset].service->schedule != NULL && dvb_hasMedia(offair_services[pEpg->serviceOffset].service) != 0 )
			{
				break;
			}
		}
		if( pEpg->serviceOffset == offair_serviceCount || offair_services[pEpg->serviceOffset].service == NULL || offair_services[pEpg->serviceOffset].service->schedule == NULL || dvb_hasMedia(offair_services[pEpg->serviceOffset].service) == 0)
		{
			interface_showMessageBox(_T("EPG_UNAVAILABLE"), thumbnail_error, 3000);
			return 1;
		}
	}
	if(offair_services[pEpg->currentService].service == NULL || offair_services[pEpg->currentService].service->schedule == NULL || dvb_hasMedia(offair_services[pEpg->currentService].service) == 0)
	{
		pEpg->currentService = pEpg->serviceOffset;
		for( pEpg->serviceOffset++; pEpg->serviceOffset < offair_serviceCount; pEpg->serviceOffset++ )
		{
			if( offair_services[pEpg->serviceOffset].service != NULL && offair_services[pEpg->serviceOffset].service->schedule != NULL && dvb_hasMedia(offair_services[pEpg->serviceOffset].service) != 0)
			{
				break;
			}
		}
	}
	dprintf("%s: Found valid service #%d %s\n", __FUNCTION__, pEpg->currentService, dvb_getServiceName(offair_services[pEpg->currentService].service));

	time( &pEpg->curOffset );
	pEpg->minOffset = pEpg->maxOffset = pEpg->curOffset = 3600 * (pEpg->curOffset / 3600); // round to hours
	events_found = 0;
	for( i = 0; i < offair_serviceCount; i++)
	{
		offair_services[i].first_event = NULL;
		if( offair_services[i].service != NULL && offair_services[i].service->schedule != NULL && dvb_hasMedia(offair_services[i].service) != 0 )
		{
			for( event_element = offair_services[i].service->schedule; event_element != NULL; event_element = event_element->next)
			{
				event = (EIT_event_t *)event_element->data;
				if(offair_getLocalEventTime(event, NULL, &event_start) == 0 )
				{
					event_end = event_start + offair_getEventDuration(event);
					if( offair_services[i].first_event == NULL && event_end > pEpg->minOffset )
					{
						offair_services[i].first_event = event_element;
						events_found = 1;
					}
					/* Skip older events
					if( event_start < pEpg->minOffset )
					{
					pEpg->minOffset = 3600 * (event_start / 3600);
					}*/
					event_end -= 3600 * pEpg->displayingHours;
					if( event_end > pEpg->maxOffset)
					{
						pEpg->maxOffset = 3600 * (event_end / 3600 + 1);
					}
				}
			}
		}
	}
	if( events_found == 0 )
	{
		interface_showMessageBox(_T("EPG_UNAVAILABLE"), thumbnail_error, 3000);
		return -1;
	}
	if( pEpg->maxOffset - pEpg->minOffset < ERM_DISPLAYING_HOURS * 3600 )
	{
		pEpg->maxOffset = pEpg->minOffset + ERM_DISPLAYING_HOURS * 3600;
	}
	pEpg->highlightedEvent = offair_services[pEpg->currentService].first_event;
	if( pEpg->highlightedEvent == NULL )
	{
		for( pEpg->currentService = 0; offair_services[pEpg->currentService].first_event == NULL; pEpg->currentService++ );
		pEpg->highlightedEvent = offair_services[pEpg->currentService].first_event;
	}
	pEpg->highlightedService = pEpg->currentService;

	pEpg->baseMenu.selectedItem = MENU_ITEM_EVENT;
	pEpg->displayingHours = ERM_DISPLAYING_HOURS;
	pEpg->timelineX = interfaceInfo.clientX + 2*interfaceInfo.paddingSize + ERM_CHANNEL_NAME_LENGTH;
	pEpg->timelineWidth = interfaceInfo.clientWidth - 3*interfaceInfo.paddingSize - ERM_CHANNEL_NAME_LENGTH;
	pEpg->pps = (float)pEpg->timelineWidth / (pEpg->displayingHours * 3600);

	pEpg->baseMenu.pArg = (void*)-1; // double call fix

	return 0;
}

#define GLAW_EFFECT \
	gfx_drawRectangle(DRAWING_SURFACE, 255, 255, 255, 0x20, x, y+6, w, 2); \
	gfx_drawRectangle(DRAWING_SURFACE, 255, 255, 255, 0x40, x, y+4, w, 2); \
	gfx_drawRectangle(DRAWING_SURFACE, 255, 255, 255, 0x60, x, y+2, w, 2); \
	gfx_drawRectangle(DRAWING_SURFACE, 255, 255, 255, 0x80, x, y, w, 2);
static void offair_EPGRecordMenuDisplay(interfaceMenu_t *pMenu)
{
	interfaceEpgMenu_t *pEpg = (interfaceEpgMenu_t *)pMenu;
	int fh, x, y, w, l, displayedChannels, i, j, timelineEnd, r,g,b,a;
	interfaceColor_t *color;
	DFBRectangle rect;
	char buf[MAX_TEXT];
	char *str;
	list_element_t *event_element;
	EIT_event_t *event;
	time_t event_tt, event_len, end_tt;
	struct tm event_tm, *t;
	interfaceColor_t sel_color = { ERM_HIGHLIGHTED_CELL_RED, ERM_HIGHLIGHTED_CELL_GREEN, ERM_HIGHLIGHTED_CELL_BLUE, ERM_HIGHLIGHTED_CELL_ALPHA };
#ifdef ENABLE_PVR
	pvrJob_t *job;
	int job_channel;
#endif

	//dprintf("%s: menu.sel=%d ri.cur=%d ri.hi=%d ri.he=%p\n", __FUNCTION__, pEpg->baseMenu.selectedItem, pEpg->currentService, pEpg->highlightedService, pEpg->highlightedEvent);
	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
	DFBCHECK( DRAWING_SURFACE->SetColor(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA) );
	/* Menu background */
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX, interfaceInfo.clientY+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientWidth, interfaceInfo.clientHeight-2*INTERFACE_ROUND_CORNER_RADIUS);
	// top left corner
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientY, interfaceInfo.clientWidth-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", interfaceInfo.clientX, interfaceInfo.clientY, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);
	// bottom left corner
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientY+interfaceInfo.clientHeight-INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientWidth-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", interfaceInfo.clientX, interfaceInfo.clientY+interfaceInfo.clientHeight-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 0, DSBLIT_BLEND_COLORALPHA, interfaceAlignLeft|interfaceAlignTop);

	/* Show menu logo if needed */
	if (interfaceInfo.currentMenu->logo > 0 && interfaceInfo.currentMenu->logoX > 0)
	{
		interface_drawImage(DRAWING_SURFACE, resource_thumbnails[interfaceInfo.currentMenu->logo],
			interfaceInfo.currentMenu->logoX, interfaceInfo.currentMenu->logoY, interfaceInfo.currentMenu->logoWidth, interfaceInfo.currentMenu->logoHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignCenter|interfaceAlignMiddle, 0, 0);
	}

	interface_displayClock( 0 /* not detached */ );

	pgfx_font->GetHeight(pgfx_font, &fh);

	if(offair_services[pEpg->currentService].service == NULL || offair_services[pEpg->currentService].first_event == NULL)
	{
		eprintf("%s: Can't display '%': service %d event %d\n", __FUNCTION__, pEpg->baseMenu.name, offair_services[pEpg->currentService].service != NULL, offair_services[pEpg->currentService].first_event != NULL);
		return;
	}
	/* Timeline */
	x = pEpg->timelineX;
	w = pEpg->timelineWidth;
	y = interfaceInfo.clientY + interfaceInfo.paddingSize;
	end_tt = pEpg->curOffset + 3600 * pEpg->displayingHours;
	timelineEnd =  pEpg->timelineX + pEpg->timelineWidth - interfaceInfo.paddingSize;
	if( pEpg->baseMenu.selectedItem == MENU_ITEM_TIMELINE)
	{
		r = ERM_HIGHLIGHTED_CELL_RED;//interface_colors[interfaceInfo.highlightColor].R;
		g = ERM_HIGHLIGHTED_CELL_GREEN;//interface_colors[interfaceInfo.highlightColor].G;
		b = ERM_HIGHLIGHTED_CELL_BLUE;//interface_colors[interfaceInfo.highlightColor].B;
		a = ERM_HIGHLIGHTED_CELL_ALPHA;//interface_colors[interfaceInfo.highlightColor].A;*/
	} else {
		r = ERM_TIMELINE_RED;
		g = ERM_TIMELINE_GREEN;
		b = ERM_TIMELINE_BLUE;
		a = ERM_TIMELINE_ALPHA;
	}
	gfx_drawRectangle(DRAWING_SURFACE, r, g, b, a, x, y, w, fh);
	GLAW_EFFECT;
	/* Timeline stamps */
	event_tt = pEpg->curOffset;
	strftime( buf, 25, _T("DATESTAMP"), localtime(&event_tt));
	gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, interfaceInfo.clientX + interfaceInfo.paddingSize, y+fh - interfaceInfo.paddingSize, buf, 0, 0);
	for( i = 0; i < pEpg->displayingHours; i++)
	{
		strftime( buf, 10, "%H:%M", localtime(&event_tt));
		gfx_drawRectangle(DRAWING_SURFACE, ERM_TIMESTAMP_RED, ERM_TIMESTAMP_GREEN, ERM_TIMESTAMP_BLUE, ERM_TIMESTAMP_ALPHA, x, y, ERM_TIMESTAMP_WIDTH, (1+ERM_VISIBLE_CHANNELS)*interfaceInfo.paddingSize + (2+ERM_VISIBLE_CHANNELS)*fh);
		gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x+ERM_TIMESTAMP_WIDTH, y+fh - interfaceInfo.paddingSize, buf, 0, 0);
		x += (int)(pEpg->pps * 3600);
		event_tt += 3600;
	}
	strftime( buf, 10, "%H:%M", localtime(&event_tt));
	DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );
	x = timelineEnd - rect.w - ERM_TIMESTAMP_WIDTH;
	gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+fh - interfaceInfo.paddingSize, buf, 0, 0);
	gfx_drawRectangle(DRAWING_SURFACE, ERM_TIMESTAMP_RED, ERM_TIMESTAMP_GREEN, ERM_TIMESTAMP_BLUE, ERM_TIMESTAMP_ALPHA, interfaceInfo.clientX + interfaceInfo.clientWidth - interfaceInfo.paddingSize -  ERM_TIMESTAMP_WIDTH, y, ERM_TIMESTAMP_WIDTH, (1+ERM_VISIBLE_CHANNELS)*interfaceInfo.paddingSize + (2+ERM_VISIBLE_CHANNELS)*fh);

	/* Current service (no vertical scroll) */
	snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", dvb_getServiceName(offair_services[pEpg->currentService].service));
	buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
	l = getMaxStringLength(buf, ERM_CHANNEL_NAME_LENGTH);
	buf[l] = 0;
	DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );

	x = interfaceInfo.clientX + interfaceInfo.paddingSize;
	y += fh + interfaceInfo.paddingSize;

	gfx_drawRectangle(DRAWING_SURFACE, ERM_CURRENT_TITLE_RED, ERM_CURRENT_TITLE_GREEN, ERM_CURRENT_TITLE_BLUE, ERM_CURRENT_TITLE_ALPHA, x, y, ERM_CHANNEL_NAME_LENGTH, fh);
	if( pEpg->baseMenu.selectedItem == MENU_ITEM_EVENT && pEpg->highlightedService == pEpg->currentService )
	{
		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
		g = 3;
		b = x + ERM_CHANNEL_NAME_LENGTH - g;
		a = ERM_HIGHLIGHTED_CELL_ALPHA;
		for( r = 0; r < 8; r++ )
		{
			gfx_drawRectangle(DRAWING_SURFACE, ERM_HIGHLIGHTED_CELL_RED, ERM_HIGHLIGHTED_CELL_GREEN, ERM_HIGHLIGHTED_CELL_BLUE, a, b, y, g, fh);
			a -= 0x10;
			b -= g;
		}

		//gfx_drawRectangle(DRAWING_SURFACE, ERM_HIGHLIGHTED_CELL_RED, ERM_HIGHLIGHTED_CELL_GREEN, ERM_HIGHLIGHTED_CELL_BLUE, ERM_HIGHLIGHTED_CELL_ALPHA, x + ERM_CHANNEL_NAME_LENGTH - 2*interfaceInfo.paddingSize, y, 2*interfaceInfo.paddingSize, fh);

	}

	gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, buf, 0, 0);
	/* Current service events */
	x += interfaceInfo.paddingSize + ERM_CHANNEL_NAME_LENGTH;
	event_element = offair_services[pEpg->currentService].first_event;
	while( event_element != NULL )
	{
		event = (EIT_event_t*)event_element->data;

		if(offair_getLocalEventTime(event, &event_tm, &event_tt) == 0)
		{
			event_len = offair_getEventDuration(event);
			if( event_tt + event_len > pEpg->curOffset && event_tt < end_tt )
			{
				if( event_tt >= pEpg->curOffset)
				{
					x = pEpg->timelineX + (int)(pEpg->pps * (event_tt - pEpg->curOffset));
				}
				w = event_tt >= pEpg->curOffset
					? (int)(pEpg->pps * offair_getEventDuration(event)) - ERM_TIMESTAMP_WIDTH
					: (int)(pEpg->pps * (event_tt + event_len - pEpg->curOffset)) - ERM_TIMESTAMP_WIDTH;
				if (x + w > timelineEnd )
				{
					w = timelineEnd - x;
				}
				DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
				gfx_drawRectangle(DRAWING_SURFACE, ERM_CELL_START_RED, ERM_CELL_START_GREEN, ERM_CELL_START_BLUE, ERM_CELL_START_ALPHA, x, y, ERM_TIMESTAMP_WIDTH, fh);
				x += ERM_TIMESTAMP_WIDTH;
				color = pEpg->baseMenu.selectedItem == MENU_ITEM_EVENT && event_element == pEpg->highlightedEvent ? &sel_color : &genre_colors[( event->content.content >> 4) & 0x0F];
				gfx_drawRectangle(DRAWING_SURFACE, color->R, color->G, color->B, color->A, x, y, w, fh);
				GLAW_EFFECT;

				snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", event->description.event_name);
				buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
				l = getMaxStringLength(buf, w-ERM_TIMESTAMP_WIDTH);
				buf[l] = 0;
				gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+fh - interfaceInfo.paddingSize, buf, 0, 0);
			}
		}
		event_element = event_element->next;
	}

	/* Other services (vertically scrollable) */
	displayedChannels = 0;
	for( i = pEpg->serviceOffset; displayedChannels < ERM_VISIBLE_CHANNELS && i < offair_serviceCount; i++)
	{
		if( i != pEpg->currentService && offair_services[i].first_event != NULL && dvb_hasMedia(offair_services[i].service) )
		{
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", dvb_getServiceName(offair_services[i].service));
			buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
			l = getMaxStringLength(buf, ERM_CHANNEL_NAME_LENGTH);
			buf[l] = 0;
			DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );

			x = interfaceInfo.clientX + interfaceInfo.paddingSize;
			y += rect.h + interfaceInfo.paddingSize;
			gfx_drawRectangle(DRAWING_SURFACE, ERM_TITLE_RED, ERM_TITLE_GREEN, ERM_TITLE_BLUE, ERM_TITLE_ALPHA, x, y, ERM_CHANNEL_NAME_LENGTH, rect.h);
			if( pEpg->baseMenu.selectedItem == MENU_ITEM_EVENT && pEpg->highlightedService == i )
			{
				DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
				g = 3;
				b = x + ERM_CHANNEL_NAME_LENGTH - g;
				a = ERM_HIGHLIGHTED_CELL_ALPHA;
				for( r = 0; r < 8; r++ )
				{
					gfx_drawRectangle(DRAWING_SURFACE, ERM_HIGHLIGHTED_CELL_RED, ERM_HIGHLIGHTED_CELL_GREEN, ERM_HIGHLIGHTED_CELL_BLUE, a, b, y, g, fh);
					a -= 0x10;
					b -= g;
				}
				//gfx_drawRectangle(DRAWING_SURFACE, ERM_HIGHLIGHTED_CELL_RED, ERM_HIGHLIGHTED_CELL_GREEN, ERM_HIGHLIGHTED_CELL_BLUE, ERM_HIGHLIGHTED_CELL_ALPHA, x + ERM_CHANNEL_NAME_LENGTH - 2*interfaceInfo.paddingSize, y, 2*interfaceInfo.paddingSize, fh);
			}

			gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, buf, 0, 0);
			displayedChannels++;

			x += interfaceInfo.paddingSize + ERM_CHANNEL_NAME_LENGTH;
			event_element = offair_services[i].first_event;
			while( event_element != NULL )
			{
				event = (EIT_event_t*)event_element->data;

				if(offair_getLocalEventTime(event, &event_tm, &event_tt) == 0)
				{
					event_len = offair_getEventDuration(event);
					if( event_tt + event_len > pEpg->curOffset && event_tt < end_tt )
					{
						if( event_tt >= pEpg->curOffset)
						{
							x = pEpg->timelineX + (int)(pEpg->pps * (event_tt - pEpg->curOffset));
						}
						w = event_tt >= pEpg->curOffset
							? (int)(pEpg->pps * offair_getEventDuration(event)) - ERM_TIMESTAMP_WIDTH
							: (int)(pEpg->pps * (event_tt + event_len - pEpg->curOffset)) - ERM_TIMESTAMP_WIDTH;
						if (x + w > timelineEnd )
						{
							w = timelineEnd - x;
						}
						DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
						gfx_drawRectangle(DRAWING_SURFACE, ERM_CELL_START_RED, ERM_CELL_START_GREEN, ERM_CELL_START_BLUE, ERM_CELL_START_ALPHA, x, y, ERM_TIMESTAMP_WIDTH, fh);
						x += ERM_TIMESTAMP_WIDTH;
						color = pEpg->baseMenu.selectedItem == MENU_ITEM_EVENT && event_element == pEpg->highlightedEvent ? &sel_color : &genre_colors[(event->content.content >> 4) & 0x0F];
						gfx_drawRectangle(DRAWING_SURFACE, color->R, color->G, color->B, color->A, x, y, w, fh);
						GLAW_EFFECT;

						snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", event->description.event_name);
						buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
						l = getMaxStringLength(buf, w-ERM_TIMESTAMP_WIDTH);
						buf[l] = 0;
						gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, buf, 0, 0);
					}
				}
				event_element = event_element->next;
			}
		}
	} // end of services loop
	/* Arrows */
	if( displayedChannels == ERM_VISIBLE_CHANNELS )
	{
		x = interfaceInfo.clientX - interfaceInfo.paddingSize - ERM_ICON_SIZE;
		for( j = i; j < offair_serviceCount; j++ )
		{
			if( j != pEpg->currentService && offair_services[j].first_event != NULL )
			{
				interface_drawImage(DRAWING_SURFACE, IMAGE_DIR "icon_up.png", x, interfaceInfo.clientY + interfaceInfo.paddingSize + 2*(interfaceInfo.paddingSize + fh), ERM_ICON_SIZE, ERM_ICON_SIZE, 1, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop, NULL, NULL);
				interface_drawImage(DRAWING_SURFACE, IMAGE_DIR "icon_down.png", x, interfaceInfo.clientY + (2+displayedChannels)*(interfaceInfo.paddingSize + fh), ERM_ICON_SIZE, ERM_ICON_SIZE, 1, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignBottom, NULL, NULL);
				x = -1;
				break;
			}
		}
		if( x > 0 )
		{
			for( j = 0; j < pEpg->serviceOffset; j++ )
			{
				if( j != pEpg->currentService && offair_services[j].first_event != NULL )
				{
					interface_drawImage(DRAWING_SURFACE, IMAGE_DIR "icon_up.png", x, interfaceInfo.clientY + interfaceInfo.paddingSize + 2*(interfaceInfo.paddingSize + fh), ERM_ICON_SIZE, ERM_ICON_SIZE, 1, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop, NULL, NULL);
					interface_drawImage(DRAWING_SURFACE, IMAGE_DIR "icon_down.png", x, interfaceInfo.clientY + (2+displayedChannels)*(interfaceInfo.paddingSize + fh), ERM_ICON_SIZE, ERM_ICON_SIZE, 1, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignBottom, NULL, NULL);
					break;
				}
			}
		}
	}
	x = interfaceInfo.clientX + interfaceInfo.paddingSize;
	/* Fillers */
	for( ; displayedChannels < ERM_VISIBLE_CHANNELS; displayedChannels++ )
	{
		y += fh + interfaceInfo.paddingSize;
		gfx_drawRectangle(DRAWING_SURFACE, ERM_TITLE_RED, ERM_TITLE_GREEN, ERM_TITLE_BLUE, ERM_TITLE_ALPHA, x, y, interfaceInfo.clientWidth - 2*interfaceInfo.paddingSize, fh);
	}
#ifdef ENABLE_PVR
	/* Record jobs */
	for( event_element = pvr_jobs; event_element != NULL; event_element = event_element->next )
	{
		job = (pvrJob_t*)event_element->data;
		if( job->end_time > pEpg->curOffset && job->start_time < end_tt )
		{
			y = interfaceInfo.clientY + interfaceInfo.paddingSize + fh;
			x = job->start_time >= pEpg->curOffset
				? pEpg->timelineX + (int)(pEpg->pps * (job->start_time - pEpg->curOffset))
				: pEpg->timelineX;
			w = job->start_time >= pEpg->curOffset
				? (int)(pEpg->pps * (job->end_time - job->start_time))
				: (int)(pEpg->pps * (job->end_time - pEpg->curOffset));
			if (x + w > timelineEnd )
			{
				w = timelineEnd - x;
			}
			gfx_drawRectangle(DRAWING_SURFACE, ERM_RECORD_RED, ERM_RECORD_GREEN, ERM_RECORD_BLUE, ERM_RECORD_ALPHA, x+ERM_TIMESTAMP_WIDTH, y, w, interfaceInfo.paddingSize);
			if( job->type == pvrJobTypeDVB )
			{
				job_channel = offair_getIndex(job->info.dvb.channel);
				if((job_channel == pEpg->currentService || ( pEpg->serviceOffset <= job_channel && job_channel < i ) ))
				{
					y += interfaceInfo.paddingSize + fh;
					if( job_channel != pEpg->currentService )
					{
						displayedChannels = 1;
						for( j = pEpg->serviceOffset; j < job_channel; j++ )
						{
							if( j != pEpg->currentService && offair_services[j].first_event != NULL )
							{
								displayedChannels++;
							}
						}
						y += displayedChannels * ( fh + interfaceInfo.paddingSize );
					}
					gfx_drawRectangle(DRAWING_SURFACE, ERM_RECORD_RED, ERM_RECORD_GREEN, ERM_RECORD_BLUE, ERM_RECORD_ALPHA, x+ERM_TIMESTAMP_WIDTH, y, w, interfaceInfo.paddingSize);
				}
			}
		}
	}
#endif
	/* Current time */
	time(&event_tt);
	if(event_tt > pEpg->curOffset && event_tt < end_tt && (t = localtime(&event_tt)) != NULL)
	{
		gfx_drawRectangle(DRAWING_SURFACE, ERM_TIMESTAMP_RED, ERM_TIMESTAMP_GREEN, ERM_TIMESTAMP_BLUE, ERM_TIMESTAMP_ALPHA, pEpg->timelineX + (int)(pEpg->pps * (event_tt - pEpg->curOffset)), interfaceInfo.clientY + interfaceInfo.paddingSize + fh, ERM_TIMESTAMP_WIDTH, (1+ERM_VISIBLE_CHANNELS)*interfaceInfo.paddingSize + (1+ERM_VISIBLE_CHANNELS)*fh);
	}
	/* Event info */
	x = interfaceInfo.clientX + interfaceInfo.paddingSize;
	y = interfaceInfo.clientY + (3+ERM_VISIBLE_CHANNELS)*interfaceInfo.paddingSize + (2+ERM_VISIBLE_CHANNELS)*fh;
	switch(pEpg->baseMenu.selectedItem)
	{
	case MENU_ITEM_BACK:
		str = _T("BACK");
		gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
		break;
	case MENU_ITEM_MAIN:
		str = _T("MAIN_MENU");
		gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
		break;
	default:
		if(pEpg->highlightedEvent != NULL)
		{
			str = buf;
			event = (EIT_event_t*)pEpg->highlightedEvent->data;

			if(offair_getLocalEventTime(event, &event_tm, &event_tt) == 0)
			{
				strftime( buf, 25, _T("DATESTAMP"), &event_tm);
				DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );
				gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
				x += ERM_CHANNEL_NAME_LENGTH + interfaceInfo.paddingSize;
				strftime( buf, 10, "%H:%M:%S", &event_tm);
				DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );
				gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
				x += rect.w;
				event_tt += offair_getEventDuration(event);
				strftime( buf, 10, "-%H:%M:%S", localtime(&event_tt));
				gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
			}
			/* // channel name
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", dvb_getServiceName(offair_services[pEpg->highlightedService].service));
			buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
			l = getMaxStringLength(buf, interfaceInfo.clientX + interfaceInfo.clientWidth - 2*interfaceInfo.paddingSize - x);
			buf[l] = 0;
			DFBCHECK( pgfx_font->GetStringExtents(pgfx_font, buf, -1, &rect, NULL) );
			gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x+interfaceInfo.paddingSize, y+rect.h - interfaceInfo.paddingSize, str, 0, 0); */

			x = interfaceInfo.clientX + interfaceInfo.paddingSize;
			y += interfaceInfo.paddingSize + fh;
			gfx_drawRectangle(DRAWING_SURFACE, ERM_CELL_START_RED, ERM_CELL_START_GREEN, ERM_CELL_START_BLUE, ERM_CELL_START_ALPHA, x, y, interfaceInfo.clientWidth - 2*interfaceInfo.paddingSize, 2*fh);
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", event->description.event_name );
			buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
			interface_drawTextWW(pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y, interfaceInfo.clientWidth - 2*interfaceInfo.paddingSize, 2*fh, str, ALIGN_LEFT);
			y += interfaceInfo.paddingSize + 2*fh;
			snprintf(buf, MAX_TEXT, "%s", event->description.text );
			buf[MAX_TEXT-1] = 0;
			interface_drawTextWW(pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y, interfaceInfo.clientWidth - 2*interfaceInfo.paddingSize, interfaceInfo.clientY + interfaceInfo.clientHeight - y - interfaceInfo.paddingSize, str, ALIGN_LEFT);
			//gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
		}
		else if( pEpg->highlightedService >=0 && pEpg->highlightedService < offair_serviceCount && offair_services[pEpg->highlightedService].service != NULL )
		{
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s", dvb_getServiceName(offair_services[pEpg->highlightedService].service) );
			buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
			str = buf;
			gfx_drawText(DRAWING_SURFACE, pgfx_font, INTERFACE_BOOKMARK_RED, INTERFACE_BOOKMARK_GREEN, INTERFACE_BOOKMARK_BLUE, INTERFACE_BOOKMARK_ALPHA, x, y+rect.h - interfaceInfo.paddingSize, str, 0, 0);
		}
	}
}

static int offair_EPGRecordMenuProcessCommand(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd)
{
	interfaceEpgMenu_t *pEpg = (interfaceEpgMenu_t *)pMenu;
	interfaceMenu_t *pParent;
	list_element_t *event_element;
	EIT_event_t *event;
	int i, service_index, service_count;
	time_t eventOffset = 0, eventEnd = 0, end_tt;
	struct tm t;
#ifdef ENABLE_PVR
	list_element_t *job_element = NULL;
#endif

	switch(cmd->command)
	{
	case interfaceCommandUp:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_BACK:
		case MENU_ITEM_MAIN:
			pMenu->selectedItem = MENU_ITEM_EVENT;
			service_index = pEpg->currentService;
			for( i = offair_serviceCount - 1; i >= 0; i-- )
			{
				if( i != pEpg->currentService && offair_services[i].first_event != NULL )
				{
					service_index = i;
					break;
				}
			}
			if(service_index != pEpg->highlightedService || pEpg->highlightedEvent == NULL )
			{
				pEpg->highlightedService = service_index;
				end_tt = pEpg->curOffset + 3600 * pEpg->displayingHours;
				for( pEpg->highlightedEvent = offair_services[pEpg->highlightedService].first_event;
					pEpg->highlightedEvent->next != NULL;
					pEpg->highlightedEvent = pEpg->highlightedEvent->next )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
					if( eventEnd > pEpg->curOffset && eventOffset < end_tt )
					{
						break;
					}
				}
				if( eventEnd <= pEpg->curOffset && pEpg->highlightedEvent->next == NULL )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
				}
				if( eventEnd <= pEpg->curOffset || eventOffset >= end_tt )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&pEpg->curOffset);
					pEpg->curOffset = (pEpg->minOffset < pEpg->curOffset) ?
						3600 * (pEpg->curOffset / 3600) : pEpg->minOffset;
				}
				// counting displaying services
				if( pEpg->highlightedService != pEpg->currentService)
				{
					service_count = 0;
					for( i = pEpg->highlightedService; i >= 0; i-- )
					{
						if( i != pEpg->currentService && offair_services[i].first_event )
						{
							service_count++;
							pEpg->serviceOffset = i;
							if( service_count == ERM_VISIBLE_CHANNELS)
							{
								break;
							}
						}
					}
				}
			} else
			{
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				eventEnd = eventOffset + offair_getEventDuration(event);
				if( eventEnd <= pEpg->curOffset || eventOffset > pEpg->curOffset + 3600 * pEpg->displayingHours )
				{
					pEpg->curOffset = (pEpg->minOffset < eventOffset) ?
						3600 * (eventOffset / 3600) : pEpg->minOffset;
				}
			}
			break;
		case MENU_ITEM_TIMELINE:
			pMenu->selectedItem = MENU_ITEM_BACK;
			break;
		default: // MENU_ITEM_EVENT
			if( pEpg->highlightedService == pEpg->currentService)
			{
				pMenu->selectedItem = MENU_ITEM_TIMELINE;
			} else {
				/* Get our current position and guess new position */
				service_index = -1;
				for( i = 0; i < pEpg->highlightedService; i++)
				{
					if( i != pEpg->currentService && offair_services[i].first_event != NULL)
					{
						service_index = i;
					}
				}
				if( service_index < 0 )
				{
					pEpg->highlightedService = pEpg->currentService;
				} else
				{
					pEpg->highlightedService = service_index;
					if ( pEpg->highlightedService <  pEpg->serviceOffset)
					{
						pEpg->serviceOffset = pEpg->highlightedService;
					}
				}
				end_tt = pEpg->curOffset + 3600 * pEpg->displayingHours;
				for( pEpg->highlightedEvent = offair_services[pEpg->highlightedService].first_event;
					pEpg->highlightedEvent->next != NULL;
					pEpg->highlightedEvent = pEpg->highlightedEvent->next )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
					if( eventEnd > pEpg->curOffset && eventOffset < end_tt )
					{
						break;
					}
				}
				if( eventEnd <= pEpg->curOffset && pEpg->highlightedEvent->next == NULL )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
				}
				if( eventEnd <= pEpg->curOffset || eventOffset >= end_tt )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&pEpg->curOffset);
					pEpg->curOffset = (pEpg->minOffset < pEpg->curOffset) ?
						3600 * (pEpg->curOffset / 3600) : pEpg->minOffset;
				}
			}
		}
		break;
	case interfaceCommandDown:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_BACK:
		case MENU_ITEM_MAIN:
			pMenu->selectedItem = MENU_ITEM_TIMELINE;
			break;
		case MENU_ITEM_TIMELINE:
			pMenu->selectedItem = MENU_ITEM_EVENT;
			if(pEpg->currentService != pEpg->highlightedService || pEpg->highlightedEvent == NULL)
			{
				pEpg->highlightedService = pEpg->currentService;
				end_tt = pEpg->curOffset + 3600 * pEpg->displayingHours;
				for( pEpg->highlightedEvent = offair_services[pEpg->highlightedService].first_event;
					pEpg->highlightedEvent->next != NULL;
					pEpg->highlightedEvent = pEpg->highlightedEvent->next )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
					if( eventEnd > pEpg->curOffset && eventOffset < end_tt )
					{
						break;
					}
				}
				if( eventEnd <= pEpg->curOffset )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&eventOffset);
					eventEnd = eventOffset + offair_getEventDuration(event);
				}
				if( eventEnd <= pEpg->curOffset || eventOffset >= end_tt )
				{
					event = (EIT_event_t*)pEpg->highlightedEvent->data;
					offair_getLocalEventTime(event,&t,&pEpg->curOffset);
					pEpg->curOffset = (pEpg->minOffset < pEpg->curOffset) ?
						3600 * (pEpg->curOffset / 3600) : pEpg->minOffset;
				}
				// counting displaying services
				if( pEpg->highlightedService != pEpg->currentService)
				{
					service_count = 1; // highlighted
					for( i = pEpg->highlightedService - 1; i >= 0; i-- )
					{
						if( i != pEpg->currentService && offair_services[i].first_event != NULL)
						{
							service_count++;
							pEpg->serviceOffset = i;
							if( service_count == ERM_VISIBLE_CHANNELS)
							{
								break;
							}
						}
					}
				}
			} else
			{
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				eventEnd = eventOffset + offair_getEventDuration(event);
				if( eventEnd <= pEpg->curOffset || eventOffset > pEpg->curOffset + 3600 * pEpg->displayingHours )
				{
					pEpg->curOffset = (pEpg->minOffset < eventOffset) ?
						3600 * (eventOffset / 3600) : pEpg->minOffset;
				}
			}
			break;
		default: // MENU_ITEM_EVENT
			service_index = -1;
			for( i = pEpg->highlightedService == pEpg->currentService ? 0 : pEpg->highlightedService+1;
				i < offair_serviceCount; i++ )
			{
				if( i != pEpg->currentService && offair_services[i].first_event != NULL )
				{
					service_index = i;
					break;
				}
			}
			if( service_index < 0 )
			{
				pMenu->selectedItem = MENU_ITEM_BACK;
				break;
			}
			pEpg->highlightedService = service_index;
			end_tt = pEpg->curOffset + 3600 * pEpg->displayingHours;
			for( pEpg->highlightedEvent = offair_services[pEpg->highlightedService].first_event;
				pEpg->highlightedEvent->next != NULL;
				pEpg->highlightedEvent = pEpg->highlightedEvent->next )
			{
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				eventEnd = eventOffset + offair_getEventDuration(event);
				if( eventEnd > pEpg->curOffset && eventOffset < end_tt )
				{
					break;
				}
			}
			if( eventEnd <= pEpg->curOffset && pEpg->highlightedEvent->next == NULL )
			{
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				eventEnd = eventOffset + offair_getEventDuration(event);
			}
			if( eventEnd <= pEpg->curOffset || eventOffset >= end_tt )
			{
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&pEpg->curOffset);
				pEpg->curOffset = (pEpg->minOffset < pEpg->curOffset) ?
					3600 * (pEpg->curOffset / 3600) : pEpg->minOffset;
			}
			if(service_index < pEpg->serviceOffset)
			{
				pEpg->serviceOffset = service_index;
			} else
			{
				service_count = 1;
				for( i = service_index - 1 ; service_count < ERM_VISIBLE_CHANNELS && i >= 0; i-- )
				{
					if( offair_services[i].service != NULL && offair_services[i].first_event != NULL )
					{
						service_count++;
						service_index = i;
					}
				}
				if( service_count == ERM_VISIBLE_CHANNELS && service_index > pEpg->serviceOffset )
				{
					pEpg->serviceOffset = service_index;
				}
			}
		}
		break;
	case interfaceCommandLeft:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_BACK:
		case MENU_ITEM_MAIN:
			pMenu->selectedItem = 1-(pMenu->selectedItem+2)-2;
			break;
		case MENU_ITEM_TIMELINE:
			if(pEpg->curOffset > pEpg->minOffset)
			{
				pEpg->curOffset -= 3600;
			}
			break;
		default: // MENU_ITEM_EVENT
			if( pEpg->highlightedService < 0 || pEpg->highlightedService >= offair_serviceCount || offair_services[pEpg->highlightedService].first_event == NULL || pEpg->highlightedEvent == offair_services[pEpg->highlightedService].first_event )
			{
				break;
			}
			for(
				event_element = offair_services[pEpg->highlightedService].first_event;
				event_element->next != NULL && event_element->next != pEpg->highlightedEvent;
			event_element = event_element->next );
			if( event_element->next == pEpg->highlightedEvent)
			{
				pEpg->highlightedEvent = event_element;
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				if( eventOffset < pEpg->curOffset )
				{
					pEpg->curOffset = (pEpg->minOffset < eventOffset) ?
						3600 * (eventOffset / 3600) : pEpg->minOffset;
				}
			}
		}
		break;
	case interfaceCommandRight:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_BACK:
		case MENU_ITEM_MAIN:
			pMenu->selectedItem = 1-(pMenu->selectedItem+2)-2;
			break;
		case MENU_ITEM_TIMELINE:
			if(pEpg->curOffset < pEpg->maxOffset)
			{
				pEpg->curOffset += 3600;
			}
			break;
		default: // MENU_ITEM_EVENT
			if( pEpg->highlightedEvent == NULL || pEpg->highlightedEvent->next == NULL)
			{
				return 0;
			}
			pEpg->highlightedEvent = pEpg->highlightedEvent->next;
			event = (EIT_event_t*)pEpg->highlightedEvent->data;
			offair_getLocalEventTime(event,&t,&eventOffset);
			eventOffset = 3600 * (eventOffset / 3600);
			if( eventOffset >= (pEpg->curOffset + pEpg->displayingHours*3600))
			{
				pEpg->curOffset = eventOffset;
			}
		}
		break;
	case interfaceCommandPageUp:
		service_index = -1;
		for( i = pEpg->serviceOffset - 1; i >= 0; i--)
		{
			if( i != pEpg->currentService && offair_services[i].first_event != NULL )
			{
				service_index = i;
				break;
			}
		}
		if( service_index < 0)
		{
			return 0;
		}
		pEpg->serviceOffset = service_index;
		break;
	case interfaceCommandPageDown:
		service_index = -1;
		for( i = pEpg->serviceOffset + 1; i < offair_serviceCount; i++)
		{
			if( i != pEpg->currentService && offair_services[i].first_event != NULL )
			{
				service_index = i;
				break;
			}
		}
		if( service_index < 0)
		{
			return 0;
		}
		service_count = 1;
		for( i = service_index + 1; service_count < ERM_VISIBLE_CHANNELS && i < offair_serviceCount; i++)
		{
			if( i != pEpg->currentService && offair_services[i].first_event != NULL )
			{
				service_count++;
			}
		}
		if( service_count == ERM_VISIBLE_CHANNELS)
		{
			pEpg->serviceOffset = service_index;
		} else return 0;
		break;
#ifdef ENABLE_PVR
	case interfaceCommandRed:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_EVENT:
			{
				pvrJob_t job;
				event = (EIT_event_t*)pEpg->highlightedEvent->data;
				offair_getLocalEventTime(event,&t,&eventOffset);
				job.type         = pvrJobTypeDVB;
				job.start_time   = eventOffset;
				job.end_time     = eventOffset + offair_getEventDuration(event);
				job.info.dvb.channel  = dvb_getServiceIndex( offair_services[pEpg->highlightedService].service );
				job.info.dvb.service  = offair_services[pEpg->highlightedService].service;
				job.info.dvb.event_id = event->event_id;
				switch( pvr_findOrInsertJob( &job, &job_element) )
				{
				case PVR_RES_MATCH:
					if( pvr_deleteJob(job_element) != 0 )
						interface_showMessageBox( _T("ERROR_SAVE_RECORD_LIST") , thumbnail_error, 0 );
					break;
				case PVR_RES_COLLISION:
					interface_showMessageBox( _T("RECORD_JOB_COLLISION") , thumbnail_info, 0 );
					return 0;
				case PVR_RES_ADDED:
					if( pvr_exportJobList() != 0 )
					{
						interface_showMessageBox( _T("ERROR_SAVE_RECORD_LIST") , thumbnail_error, 0 );
					}
					break;
				default:
					eprintf("%s: pvr_findOrInsertJob failed\n", __FUNCTION__);
					//case 1: interface_showMessageBox( _T("RECORD_JOB_INVALID_SERVICE") , thumbnail_info, 0 );
					return 0;
				}
			}
			break;
			//case MENU_ITEM_JOBS:
			//	break;
		default: ;
		}
		break;
#endif
	case interfaceCommandGreen:
		if( pMenu->selectedItem == MENU_ITEM_EVENT )
		{
			pMenu->selectedItem = MENU_ITEM_TIMELINE;
		}
		else if( pMenu->selectedItem == MENU_ITEM_TIMELINE )
		{
			pMenu->selectedItem = MENU_ITEM_EVENT;
		}
		break;
	case interfaceCommandExit:
		/*interface_showMenu(!interfaceInfo.showMenu, 1);
		break;*/
	case interfaceCommandBack:
		if ( pMenu->pParentMenu != NULL )
		{
			//dprintf("%s: go back\n", __FUNCTION__);
			interface_menuActionShowMenu(pMenu, pMenu->pParentMenu);
			//interfaceInfo.currentMenu = pMenu->pParentMenu;
		}
		break;
	case interfaceCommandMainMenu:
		pParent = pMenu;
		while ( pParent->pParentMenu != NULL )
		{
			pParent = pParent->pParentMenu;
		}
		interface_menuActionShowMenu(pMenu, pParent);
		//interfaceInfo.currentMenu = pParent;
		break;
	case interfaceCommandEnter:
	case interfaceCommandOk:
		switch( pMenu->selectedItem )
		{
		case MENU_ITEM_BACK:
			interface_menuActionShowMenu(pMenu, pMenu->pParentMenu);
			return 0;
		case MENU_ITEM_MAIN:
			pParent = pMenu;
			while ( pParent->pParentMenu != NULL )
			{
				pParent = pParent->pParentMenu;
			}
			interface_menuActionShowMenu(pMenu, pParent);
			return 0;
		default: ;
		}
		break;
	default: ;
	}
	interface_displayMenu(1);
	return 0;
}

static int offair_EPGMenuKeyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int channelNumber;

	if(pMenu->selectedItem < 0)
	{
		return 1;
	}

	channelNumber = (int)pArg;

	if (channelNumber >= 0)
	{
		if (cmd->command == interfaceCommandRed)
		{
			offair_initEPGRecordMenu(pMenu, (void*)channelNumber);
			return 0;
		}
	}

	return 1;
}

#ifdef ENABLE_PVR
static int  offair_confirmStopRecording(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void *pArg)
{
	switch( cmd->command )
	{
	case interfaceCommandExit:
	case interfaceCommandLeft:
	case interfaceCommandRed:
		return 0;
		break;
	case interfaceCommandGreen:
	case interfaceCommandOk:
	case interfaceCommandEnter:
		pvr_stopRecordingDVB(screenMain);
		return 0;
		break;
	default: return 1;
	}
	return 0;
}

static int  offair_stopRecording(interfaceMenu_t *pMenu, void *pArg)
{
	interface_showConfirmationBox( _T("CONFIRM_STOP_PVR"), thumbnail_question, offair_confirmStopRecording, NULL );
	return 0;
}
#endif

int offair_epgEnabled()
{
	int i;
	for( i = 0; i < offair_serviceCount; ++i )
		if(offair_scheduleCheck( i ) == 0)
			return 1;
	return 0;
}

void offair_fillDVBTMenu()
{
	char *str;
	char  buf[PATH_MAX];

	interface_clearMenuEntries((interfaceMenu_t*)&DVBTMenu);

	offair_initServices();

#ifdef ENABLE_PVR
	if ( appControlInfo.pvrInfo.dvb.channel >= 0 )
	{
		str = dvb_getTempServiceName(appControlInfo.pvrInfo.dvb.channel);
		if( str == NULL )
		{
			str = _T("NOT_AVAILABLE_SHORT");
		}
		sprintf(buf, "%s: %s", _T("RECORDING"), str);
		interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, buf, offair_stopRecording, NULL, thumbnail_recording);
	} else
#endif
	{
		sprintf(buf, "%s: ", _T("SELECTED_CHANNEL"));
		if ( appControlInfo.dvbInfo[screenMain].channel <= 0 || appControlInfo.dvbInfo[screenMain].channel == CHANNEL_CUSTOM || appControlInfo.dvbInfo[screenMain].channel > offair_serviceCount || offair_services[appControlInfo.dvbInfo[screenMain].channel].service == NULL )
		{
			strcat(buf, _T("NONE"));
			interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTMenu, buf, thumbnail_not_selected);
		} else
		{
			strcat(buf, dvb_getServiceName(offair_services[appControlInfo.dvbInfo[screenMain].channel].service));
			interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, buf, offair_channelChange, (void*)CHANNEL_INFO_SET(screenMain, appControlInfo.dvbInfo[screenMain].channel), thumbnail_selected);
		}
	}

#ifndef HIDE_EXTRA_FUNCTIONS
	switch ( dvb_getType(0) )  // Assumes same type FEs
	{
	case FE_OFDM:
		sprintf(buf,"%s: DVB-T", _T("DVB_MODE") );
		break;
	case FE_QAM:
		sprintf(buf,"%s: DVB-C", _T("DVB_MODE") );
		break;
	//case FE_QPSK:
	//case FE_ATSC:
	default:
		eprintf("offair: unsupported FE type %d\n", dvb_getType(0));
		sprintf(buf,"%s: %s", _T("DVB_MODE"), _T("NOT_AVAILABLE_SHORT") );
		break;
	}

	interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTMenu, buf, thumbnail_channels);
#endif

	if (offair_indexCount > 0)
	{
		sprintf(buf, "%s (%d)", _T("MAIN_LAYER"), offair_indexCount);
	} else
	{
		sprintf(buf,"%s", _T("MAIN_LAYER"));
	}
	interface_addMenuEntryCustom((interfaceMenu_t*)&DVBTMenu, interfaceMenuEntryText, buf, strlen(buf)+1, dvb_getNumberOfServices() > 0, (menuActionFunction)menuDefaultActionShowMenu, NULL, NULL, NULL, (void*)&DVBTOutputMenu[0], thumbnail_channels);

#ifndef HIDE_EXTRA_FUNCTIONS
	if ((gfx_getNumberLayers() == GFX_MAX_LAYERS_5L) && !(gfx_isHDoutput()))
	{
		str = _T("PIP_LAYER");
		interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&DVBTOutputMenu[1], thumbnail_channels);
	}
#endif

	if ( offair_epgEnabled() )
	{
		str = _T("EPG_MENU");
		interface_addMenuEntryCustom((interfaceMenu_t*)&DVBTMenu, interfaceMenuEntryText, str, strlen(str)+1, dvb_services != NULL, (menuActionFunction)menuDefaultActionShowMenu, NULL, NULL, NULL, (void*)&EPGMenu, thumbnail_epg);
	} else
	{
		str = _T("EPG_UNAVAILABLE");
		interface_addMenuEntryDisabled((interfaceMenu_t*)&DVBTMenu, str, thumbnail_not_selected);
	}
#ifdef ENABLE_PVR
		str = _T("RECORDING");
		interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, str, pvr_initPvrMenu, (void*)pvrJobTypeDVB, thumbnail_recording);
	
#endif

#ifndef HIDE_EXTRA_FUNCTIONS
	if (appControlInfo.dvbInfo[screenMain].active)
	{
		str = _T("DISPLAY_TIME");
		interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, str, offair_getTime, NULL, thumbnail_configure);
	}

	str = appControlInfo.offairInfo.tunerDebug ? _T("DVB_DEBUG_ENABLE") : _T("DVB_DEBUG_DISABLE");
	interface_addMenuEntry((interfaceMenu_t*)&DVBTMenu, str, offair_debugToggle, NULL, appControlInfo.offairInfo.tunerDebug ? thumbnail_yes : thumbnail_no);
#endif
}

#define MAX_SEARCH_STACK 32

typedef int (*list_comp_func)(list_element_t *, list_element_t *);

static int event_time_cmp(list_element_t *elem1, list_element_t *elem2)
{
	int result;
	EIT_event_t *event1,*event2;
	event1 = (EIT_event_t *)elem1->data;
	event2 = (EIT_event_t *)elem2->data;
	result = event1->start_year - event2->start_year;
	if(result != 0)
	{
		return result;
	}
	result = event1->start_month - event2->start_month;
	if(result != 0)
	{
		return result;
	}
	result = event1->start_day - event2->start_day;
	if(result != 0)
	{
		return result;
	}
	return event1->start_time - event2->start_time;
}

static list_element_t *intersect_sorted(list_element_t *list1, list_element_t *list2, list_comp_func cmp_func)
{
	list_element_t *current, *p1, *p2, *result;
	p1 = list1;
	p2 = list2;
	if (cmp_func(p1,p2) <= 0)
	{
		current = p1;
		p1 = p1->next;
	} else
	{
		current = p2;
		p2 = p2->next;
	}
	result = current;
	while ((p1 != NULL) && (p2 != NULL))
	{
		if (cmp_func(p1,p2) <= 0)
		{
			current->next = p1;
			current = p1;
			p1 = p1->next;
		} else
		{
			current->next = p2;
			current = p2;
			p2 = p2->next;
		}
	}
	if( p1 != NULL)
	{
		current->next = p1;
	}	else
	{
		current->next = p2;
	}
	return result;
}

static void offair_sortEvents(list_element_t **event_list)
{
	struct list_stack_t
	{
		int level;
		list_element_t *list;
	} stack[MAX_SEARCH_STACK];
	int stack_pos;
	list_element_t *event_element;
	if(event_list == NULL)
	{
		return;
	}
	stack_pos = 0;
	event_element = *event_list;
	while( event_element != NULL )
	{
		stack[stack_pos].level = 1;
		stack[stack_pos].list = event_element;
		event_element = event_element->next;
		stack[stack_pos].list->next = NULL;
		stack_pos++;
		while( (stack_pos > 1) && (stack[stack_pos - 1].level == stack[stack_pos - 2].level))
		{
			stack[stack_pos - 2].list = intersect_sorted(stack[stack_pos - 2].list, stack[stack_pos - 1].list, event_time_cmp);
			stack[stack_pos - 2].level++;
			stack_pos--;
		}
	}
	while( (stack_pos > 1) && (stack[stack_pos - 1].level == stack[stack_pos - 2].level))
	{
		stack[stack_pos - 2].list = intersect_sorted(stack[stack_pos - 2].list, stack[stack_pos - 1].list, event_time_cmp);
		stack[stack_pos - 2].level++;
		stack_pos--;
	}
	if(stack_pos > 0)
	{
		*event_list = stack[0].list;
	}
}

static void offair_sortSchedule()
{
	list_element_t *service_element;

	EIT_service_t *service;

	for( service_element = dvb_services; service_element != NULL; service_element = service_element->next )
	{
		service = (EIT_service_t *)service_element->data;
		offair_sortEvents(&service->schedule);
	}
}

static void offair_exportServices(const char* filename)
{
	int i = 0;
	FILE* f = fopen( filename, "w" );
	if( f == NULL )
	{
		return;
	}
	for( i = 0; i < offair_serviceCount; ++i )
	{
		if( (offair_services[i].common.media_id | offair_services[i].common.service_id | offair_services[i].common.transport_stream_id) != 0 )
		{
			fprintf(f, "service %d media_id %lu service_id %hu transport_stream_id %hu\n", i,
				offair_services[i].common.media_id,
				offair_services[i].common.service_id,
				offair_services[i].common.transport_stream_id );
		}
	}
	fclose(f);
}

static void offair_importServices(const char* filename)
{
	char buf[BUFFER_SIZE];
	int i;
	unsigned long media_id;
	unsigned short service_id, transport_stream_id;

	FILE* fd = fopen( filename, "r" );
	if( fd == NULL )
	{
		dprintf("%s: Failed to open '%s'\n", __FUNCTION__, filename);
		return;
	}
	while (fgets(buf, BUFFER_SIZE, fd) != NULL)
	{
		media_id = i = service_id = transport_stream_id = 0;
		if ( sscanf(buf, "service %d media_id %lu service_id %hu transport_stream_id %hu\n",
			&i, &media_id, &service_id, &transport_stream_id) == 4)
		{
			if(i >= MAX_MEMORIZED_SERVICES || i < 0)
			{
				continue;
			}
			if ( i >= offair_serviceCount )
			{
				offair_serviceCount = i+1;
			}
			offair_services[i].common.media_id = media_id;
			offair_services[i].common.service_id = service_id;
			offair_services[i].common.transport_stream_id = transport_stream_id;
		}
	}
	fclose(fd);
	dprintf("%s: imported %d services\n", __FUNCTION__,offair_serviceCount);
}

static int service_cmp(const void *e1, const void *e2)
{
	int result = 0, i1=*(int*)e1, i2 = *(int*)e2;
	EIT_service_t *s1 = offair_services[i1].service ,
		*s2 = offair_services[i2].service;
	if( appControlInfo.offairInfo.sorting == serviceSortType )
	{
		result = dvb_hasMediaType(s2, mediaTypeVideo) - dvb_hasMediaType(s1, mediaTypeVideo);
		if( result != 0 )
			return result;
	}
	if( appControlInfo.offairInfo.sorting == serviceSortFreq )
	{
		__u32 f1,f2;
		dvb_getServiceFrequency(s1,&f1);
		dvb_getServiceFrequency(s2,&f2);
		if( f1 > f2 )
			result = 1;
		else if( f2 > f1 )
			result = -1;
		if( result != 0 )
			return result;
	}
	char *n1, *n2;
	n1 = dvb_getServiceName(s1);
	n2 = dvb_getServiceName(s2);
	return strcasecmp(dvb_getServiceName(s1), dvb_getServiceName(s2));
}

void offair_initServices()
{
	int i, old_count = offair_serviceCount;
	list_element_t *service_element;
	for( i = 0; i < old_count; ++i )
	{
		offair_services[i].service = NULL;
	}
	for( service_element = dvb_services; service_element != NULL; service_element = service_element->next )
	{
		for( i = 0; i < old_count; ++i )
		{
			if( memcmp(service_element->data, &offair_services[i].common, sizeof(EIT_common_t)) == 0 )
			{
				offair_services[i].service = (EIT_service_t *)service_element->data;
				break;
			}
		}
		if( i < old_count)
		{
			continue;
		}
		if(offair_serviceCount == MAX_MEMORIZED_SERVICES)
		{
			break;
		}
		i = offair_serviceCount++;
		offair_services[i].service = (EIT_service_t *)service_element->data;
		offair_services[i].common = offair_services[i].service->common;
	}
	offair_sortSchedule();
	offair_indexCount = 0;
	for( i = 0; i < offair_serviceCount; i++)
	{
		if( offair_services[i].service != NULL && (appControlInfo.offairInfo.dvbShowScrambled || dvb_getScrambled( offair_services[i].service ) == 0) && dvb_hasMedia(offair_services[i].service) )
		{
			offair_indeces[offair_indexCount++] = i;
		}
	}
	if(appControlInfo.offairInfo.sorting != serviceSortNone )
	{
		qsort( offair_indeces, offair_indexCount, sizeof(offair_indeces[0]), service_cmp );
	}

	dprintf("%s: Services re-inited. old=%d current=%d dvb=%d indeces=%d\n", __FUNCTION__ ,old_count,offair_serviceCount,dvb_getNumberOfServices(),offair_indexCount);

	offair_exportServices(OFFAIR_SERVICES_FILENAME);
	//return offair_serviceCount;
#ifdef ENABLE_STATS
	stats_load();
#endif
}

void offair_clearServices()
{
	/* We don't want user to see #0 channel (bug 9548) */
	offair_serviceCount = 1;
	memset( &offair_services, 0, sizeof(offair_services[0])*offair_serviceCount );
	offair_services[0].service = NULL;
	offair_services[0].common.media_id = (unsigned long)-1;
}

void offair_clearServiceList(int permanent)
{
#ifdef ENABLE_PVR
	pvr_stopRecordingDVB(screenMain);
#endif
	offair_stopVideo(screenMain, 1);
	dvb_clearServiceList(permanent);
	offair_initServices();
#ifdef ENABLE_PVR
	pvr_purgeDVBRecords();
#endif
}

static void offair_swapServices(int first, int second)
{
	service_index_t t;
	int i, first_i = -1, second_i = -1;
	for( i = 0; i < offair_serviceCount; i++ )
	{
		if( offair_indeces[i] == first )
			first_i = i;
		if( offair_indeces[i] == second )
			second_i = i;
	}
	if( first_i >= 0 )
	{
		offair_indeces[first_i] = second;
	}
	if( second_i >= 0 )
	{
		offair_indeces[second_i] = first;
	}
	t = offair_services[first];
	offair_services[first] = offair_services[second];
	offair_services[second] = t;
}

static int offair_scheduleCheck( int serviceNumber )
{
	if( offair_services[serviceNumber].service == NULL )
	{
		return -1;
	}
	return (offair_services[serviceNumber].service->schedule == NULL || dvb_hasMedia(offair_services[serviceNumber].service) == 0) ? 1 : 0;
}

static int offair_showSchedule(interfaceMenu_t *pMenu, void* pArg)
{
	char buffer[MAX_MESSAGE_BOX_LENGTH], *str;
	int buf_length = 0, event_length;
	list_element_t *event_element;
	EIT_event_t *event;
	int old_mday = -1;
	struct tm event_tm;
	time_t event_start;

	offair_scheduleIndex = (int)pArg;

	if( offair_services[offair_scheduleIndex].service == NULL )
	{
		return -1;
	}

	buffer[0] = 0;
	str = buffer;
	for( event_element = offair_services[offair_scheduleIndex].service->schedule; event_element != NULL; event_element = event_element->next )
	{
		event = (EIT_event_t *)event_element->data;
		event_length = strlen( (char*)event->description.event_name );
		/* 11 = strlen(timestamp + ". " + "\n"); */
		if( (buf_length + 11 + event_length) > MAX_MESSAGE_BOX_LENGTH )
		{
			break;
		}
		if(offair_getLocalEventTime( event, &event_tm, &event_start ) == 0)
		{
			if ( event_tm.tm_mday != old_mday )
			{
				/* 64 - approximate max datestamp length in most languages */
				if( (buf_length + 64 + 11 + event_length) > MAX_MESSAGE_BOX_LENGTH )
					break;
				strftime(str, 64, _T("SCHEDULE_TIME_FORMAT"), &event_tm);
				buf_length += strlen(str);
				str = &buffer[buf_length];
				*str = '\n';
				buf_length++;
				str++;
				old_mday = event_tm.tm_mday;
			}
			strftime( str, 9, "%T", &event_tm );
		} else
		{
			strcpy(str, "--:--:--");
		}
		buf_length += 8;
		str = &buffer[buf_length];
		sprintf(str, ". %s\n", (char*)event->description.event_name);
		buf_length += event_length + 3;
		str = &buffer[buf_length];
	}
	interface_showScrollingBox(buffer, thumbnail_epg, NULL, NULL );
	return 0;
}

static int offair_fillEPGMenu(interfaceMenu_t *pMenu, void* pArg)
{
	int i;
	char buf[MENU_ENTRY_INFO_LENGTH], *str;

	interface_clearMenuEntries((interfaceMenu_t*)&EPGMenu);

	if( dvb_services == NULL )
	{
		str = _T("NONE");
		interface_addMenuEntryDisabled((interfaceMenu_t*)&EPGMenu, str, thumbnail_not_selected);
		return 0;
	}

#ifndef ENABLE_PVR
	str = _T("SCHEDULE");
	interface_addMenuEntry((interfaceMenu_t*)&EPGMenu, str, (menuActionFunction)menuDefaultActionShowMenu, (void*)&EPGRecordMenu, thumbnail_epg);
#endif

	for( i = 0; i < offair_serviceCount; ++i )
	{
		if( offair_scheduleCheck(i) == 0 )
		{
			if( offair_serviceCount < 10 )
			{
				sprintf(buf, "%d. %s", i, offair_services[i].service->service_descriptor.service_name);
			} else // if ( offair_serviceCount < 100 )
			{
				sprintf(buf, "%02d. %s", i, offair_services[i].service->service_descriptor.service_name);
			}
			buf[MENU_ENTRY_INFO_LENGTH-1] = 0;
			interface_addMenuEntry((interfaceMenu_t*)&EPGMenu, buf, offair_showSchedule, (void*)i, thumbnail_channels);
		} else
		{
			if( offair_services[i].service != NULL )
			{
				dprintf("%s: Service %s has no EPG\n", __FUNCTION__, dvb_getServiceName(offair_services[i].service));
			} else
			{
				dprintf("%s: Service %d is not available\n", __FUNCTION__, i);
			}
		}
	}

	//interface_menuActionShowMenu(pMenu, (void*)&EPGMenu);

	return 0;
}

#ifdef ENABLE_DVB_DIAG
static int offair_diagnisticsInfoCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		appControlInfo.dvbInfo[screenMain].reportedSignalStatus = 0;
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		dprintf("%s: start info wizard\n", __FUNCTION__);
		interfaceInfo.showMessageBox = 0;
		offair_wizardStart((interfaceMenu_t *)&DVBTOutputMenu[screenMain], pArg);
		appControlInfo.dvbInfo[screenMain].reportedSignalStatus = 0;
		return 0;
	}

	return 1;
}

static int offair_diagnisticsCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{

		if (appControlInfo.offairInfo.diagnosticsMode == DIAG_ON)
		{
			interface_showMessageBox(_T("DIAGNOSTICS_DISABLED"), thumbnail_info, 5000);
			appControlInfo.offairInfo.diagnosticsMode = DIAG_OFF;
			return 1;
		}
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		dprintf("%s: start diag wizard\n", __FUNCTION__);
		interfaceInfo.showMessageBox = 0;
		offair_wizardStart((interfaceMenu_t *)&DVBTOutputMenu[screenMain], pArg);
		appControlInfo.dvbInfo[screenMain].reportedSignalStatus = 0;
		return 0;
	}

	return 1;
}

static int offair_checkSignal(int which, list_element_t **pPSI)
{
	uint16_t snr, signal;
	uint32_t ber, uncorrected_blocks;
	fe_status_t status;
	list_element_t *running_program_map = NULL;
	int res = 1;
	int ccerrors;
	stb810_signalStatus lastSignalStatus = signalStatusNoStatus;

	if (pPSI != NULL)
	{
		running_program_map = *pPSI;
	}

	dprintf("%s: Check signal %d!\n", __FUNCTION__, appControlInfo.dvbInfo[which].reportedSignalStatus);

	status = dvb_getSignalInfo(appControlInfo.dvbInfo[which].tuner, &snr, &signal, &ber, &uncorrected_blocks);

	ccerrors = phStbSystemManager_GetErrorsStatistics(phStbRpc_MainTMErrorId_DemuxerContinuityCounterMismatch, 1);

	dprintf("%s: Status: %d, ccerrors: %d\n", __FUNCTION__, status, ccerrors);

	if (status == 0)
	{
		lastSignalStatus = signalStatusNoSignal;
	} else if (signal < BAD_SIGNAL && (uncorrected_blocks > BAD_UNC || ber > BAD_BER))
	{
		lastSignalStatus = signalStatusBadSignal;
	} else if (running_program_map != NULL)
	{
		int has_new_channels = 0;
		int missing_old_channels = 0;
		list_element_t *element, *found;

		/* find new */
		element = running_program_map;
		while (element != NULL)
		{
			EIT_service_t *service = (EIT_service_t *)element->data;
			if (service != NULL && (service->flags & (serviceFlagHasPAT|serviceFlagHasPMT)) == (serviceFlagHasPAT|serviceFlagHasPMT))
			{
				found = find_element_by_header(dvb_services, (unsigned char*)&service->common, sizeof(EIT_common_t), NULL);
				if (found == NULL || (((EIT_service_t *)found->data)->flags & (serviceFlagHasPAT|serviceFlagHasPMT)) != (serviceFlagHasPAT|serviceFlagHasPMT))
				{
					dprintf("%s: Found new channel!\n", __FUNCTION__);
					has_new_channels = 1;
					break;
				}
			}
			element = element->next;
		}

		/* find missing */
		element = dvb_services;
		while (element != NULL)
		{
			EIT_service_t *service = (EIT_service_t *)element->data;
			if (service != NULL && (service->flags & (serviceFlagHasPAT|serviceFlagHasPMT)) == (serviceFlagHasPAT|serviceFlagHasPMT))
			{
				found = find_element_by_header(running_program_map, (unsigned char*)&service->common, sizeof(EIT_common_t), NULL);
				if (found == NULL || (((EIT_service_t *)found->data)->flags & (serviceFlagHasPAT|serviceFlagHasPMT)) != (serviceFlagHasPAT|serviceFlagHasPMT))
				{
					dprintf("%s: Some channels are missing!\n", __FUNCTION__);
					missing_old_channels = 1;
					break;
				}
			}
			element = element->next;
		}

		if (has_new_channels || missing_old_channels)
		{
			lastSignalStatus = signalStatusNewServices;
		}
	} else if (running_program_map == NULL && pPSI != NULL && uncorrected_blocks == 0)
	{
		lastSignalStatus = signalStatusNoPSI;
	}

	// When we're called by 'info' button or we're already showing info
	if (lastSignalStatus == signalStatusNoStatus && (pPSI == NULL || appControlInfo.dvbInfo[which].reportedSignalStatus))
	{
		if (signal > AVG_SIGNAL && (uncorrected_blocks > BAD_UNC || ber > BAD_BER))
		{
			lastSignalStatus = signalStatusBadQuality;
		} else if (signal > AVG_SIGNAL && uncorrected_blocks == 0 && ber < BAD_BER && ccerrors > BAD_CC)
		{
			lastSignalStatus = signalStatusBadCC;
		} else if (signal < BAD_SIGNAL && uncorrected_blocks == 0 && ber < BAD_BER)
		{
			lastSignalStatus = signalStatusLowSignal;
		} else
		{
			lastSignalStatus = signalStatusNoProblems;
		}
	}

	if (lastSignalStatus != signalStatusNoStatus && (pPSI == NULL || lastSignalStatus == appControlInfo.dvbInfo[which].lastSignalStatus))
	{
		char *str = "";
		int confirm = 1;
		int delay = 10000;
		void *param = (void*)offair_services[appControlInfo.dvbInfo[which].channel].service->media.frequency;

		dprintf("%s: Signal status changed!\n", __FUNCTION__);

		res = 0;

		switch (lastSignalStatus)
		{
		case signalStatusNoSignal:
			str = _T("DIAGNOSTICS_NO_SIGNAL");
			break;
		case signalStatusBadSignal:
			str = _T("DIAGNOSTICS_BAD_SIGNAL");
			break;
		case signalStatusNewServices:
			str = _T("DIAGNOSTICS_NEW_SERVICES");
			param = NULL;
			break;
		case signalStatusNoPSI:
			str = _T("DIAGNOSTICS_NO_PSI");
			confirm = 0;
			delay = 0;
			res = 1;
			break;
		case signalStatusBadQuality:
			str = _T("DIAGNOSTICS_BAD_QUALITY");
			break;
		case signalStatusBadCC:
			str = _T("DIAGNOSTICS_BAD_CC");
			confirm = 0;
			res = 1;
			break;
		case signalStatusLowSignal:
			str = _T("DIAGNOSTICS_LOW_SIGNAL");
			break;
		case signalStatusNoProblems:
		default:
			str = _T("DIAGNOSTICS_NO_PROBLEMS");
			confirm = 0;
			res = 1;
			break;
		}

		/* Don't show same error many times */
		if (appControlInfo.dvbInfo[which].savedSignalStatus != lastSignalStatus || pPSI == NULL)
		{
			dprintf("%s: Show message!\n", __FUNCTION__);
			/* Make sure play control is visible with it's signal levels */
			interface_playControlRefresh(0);
			if (confirm)
			{
				appControlInfo.dvbInfo[which].reportedSignalStatus = 1;
				interface_showConfirmationBox(str, thumbnail_question, pPSI == NULL ? offair_diagnisticsInfoCallback : offair_diagnisticsCallback, param);
			} else
			{
				appControlInfo.dvbInfo[which].reportedSignalStatus = 0;
				interface_showMessageBox(str, thumbnail_info, delay);
			}
		}
		appControlInfo.dvbInfo[which].savedSignalStatus = lastSignalStatus;
	}

	dprintf("%s: Signal id: %d/%d\n", __FUNCTION__, lastSignalStatus, appControlInfo.dvbInfo[which].lastSignalStatus);

	appControlInfo.dvbInfo[which].lastSignalStatus = lastSignalStatus;

	return res;
}

static int  offair_updatePSI(void* pArg)
{
	int which = (int)pArg;
	list_element_t *running_program_map = NULL;
	int my_channel = appControlInfo.dvbInfo[which].channel;

	dprintf("%s: in\n", __FUNCTION__);

	if( offair_services[appControlInfo.dvbInfo[which].channel].service == NULL )
	{
		eprintf("offair: Can't update PSI: service %d is null\n",appControlInfo.dvbInfo[which].channel);
		return -1;
	}

	//if (appControlInfo.dvbInfo[which].scanPSI)
	{
		dprintf("%s: *** updating PSI [%s]***\n", __FUNCTION__, dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service));
		dvb_scanForPSI( appControlInfo.dvbInfo[which].tuner, offair_services[appControlInfo.dvbInfo[which].channel].service->media.frequency, &running_program_map );
		dprintf("%s: *** PSI updated ***\n", __FUNCTION__ );

		dprintf("%s: active %d, channel %d/%d\n", __FUNCTION__, appControlInfo.dvbInfo[which].active, my_channel, appControlInfo.dvbInfo[which].channel);

		if( appControlInfo.dvbInfo[which].active && my_channel == appControlInfo.dvbInfo[which].channel ) // can be 0 if we switched from DVB when already updating
		{
			/* Make diagnostics... */
			if (appControlInfo.offairInfo.diagnosticsMode == DIAG_ON)
			{
				appControlInfo.dvbInfo[which].scanPSI = offair_checkSignal(which, &running_program_map);
			}
		}
	}

	if (appControlInfo.dvbInfo[which].active)
	{
		interface_addEvent(offair_updatePSI, pArg, PSI_UPDATE_INTERVAL, 1);
	}

	free_services(&running_program_map);

	dprintf("%s: out\n");

	return 0;
}
#endif

static int  offair_updateEPG(void* pArg)
{
	char desc[BUFFER_SIZE];
	int which = (int)pArg;
	int my_channel = appControlInfo.dvbInfo[which].channel;

	dprintf("%s: in\n", __FUNCTION__);

	if( offair_services[appControlInfo.dvbInfo[which].channel].service == NULL )
	{
		dprintf("offair: Can't update EPG: service %d is null\n",appControlInfo.dvbInfo[which].channel);
		return -1;
	}

	mysem_get(epg_semaphore);
	/*
	dprintf("%s: Check PSI: %d, diag mode %d\n", __FUNCTION__, appControlInfo.dvbInfo[which].scanPSI, appControlInfo.offairInfo.diagnosticsMode);

	if (appControlInfo.dvbInfo[which].scanPSI)
	{
	offair_updatePSI(pArg);
	}
	*/
	if( appControlInfo.dvbInfo[which].active && my_channel == appControlInfo.dvbInfo[which].channel ) // can be 0 if we switched from DVB when already updating
	{
		dprintf("%s: scan for epg\n", __FUNCTION__);

		dprintf("%s: *** updating EPG [%s]***\n", __FUNCTION__, dvb_getServiceName(offair_services[appControlInfo.dvbInfo[which].channel].service));
		dvb_scanForEPG( appControlInfo.dvbInfo[which].tuner, offair_services[appControlInfo.dvbInfo[which].channel].service->media.frequency );
		dprintf("%s: *** EPG updated ***\n", __FUNCTION__ );

		dprintf("%s: if active\n", __FUNCTION__);

		if( appControlInfo.dvbInfo[which].active && my_channel == appControlInfo.dvbInfo[which].channel ) // can be 0 if we switched from DVB when already updating
		{
			dprintf("%s: refresh event\n", __FUNCTION__);

			if (appControlInfo.dvbInfo[which].active)
			{
				offair_getServiceDescription(offair_services[appControlInfo.dvbInfo[which].channel].service,desc,_T("DVB_CHANNELS"));
				interface_playControlUpdateDescription(desc);
				interface_addEvent(offair_updateEPG, pArg, EPG_UPDATE_INTERVAL, 1);
			}
		}
	}

	dprintf("%s: update epg out\n", __FUNCTION__);

	mysem_release(epg_semaphore);

	return 0;
}

void offair_buildDVBTMenu(interfaceMenu_t *pParent)
{
	int offair_icons[4] = { 0, statusbar_f2_timeline, 0, 0};
	offair_clearServices();

	mysem_create(&epg_semaphore);
	mysem_create(&offair_semaphore);

	offair_importServices(OFFAIR_SERVICES_FILENAME);

	createListMenu(&DVBTMenu, _T("DVB_CHANNELS"), thumbnail_dvb, NULL, pParent,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		offair_enterDVBTMenu, NULL, NULL);

#ifdef ENABLE_PVR
	offair_icons[0] = statusbar_f1_record;
#endif
	createBasicMenu(&EPGRecordMenu.baseMenu, interfaceMenuEPG, _T("SCHEDULE"), thumbnail_epg, offair_icons,
#ifdef ENABLE_PVR
		(interfaceMenu_t *)&PvrMenu,
#else
		(interfaceMenu_t *)&EPGMenu,
#endif
		offair_EPGRecordMenuProcessCommand, offair_EPGRecordMenuDisplay, NULL, offair_initEPGRecordMenu, NULL, (void*)0);

	createListMenu(&EPGMenu, _T("EPG_MENU"), thumbnail_epg, NULL,
#ifdef ENABLE_PVR
		(interfaceMenu_t*)&DVBTMenu,
#else
		pParent,
#endif	
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,
		offair_fillEPGMenu, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&EPGMenu, offair_EPGMenuKeyCallback);

	offair_fillDVBTMenu();
	offair_initDVBTOutputMenu((interfaceMenu_t*)&DVBTMenu, screenMain);

	wizard_init();
#ifdef ENABLE_STATS
	stats_init();
#endif
}

void offair_cleanupMenu()
{
	if (epg_semaphore!=0) {
		mysem_destroy(epg_semaphore);
		epg_semaphore = 0;
	}
	if (offair_semaphore!=0) {
		mysem_destroy(offair_semaphore);
		offair_semaphore = 0;
	}
}

void offair_playURL(char* URL, int which)
{
	url_desc_t ud;
	int i, j;
	unsigned short srv_id = 0, ts_id = 0, pcr = 0, apid = 0, tpid = 0, vpid = 0, at = 0, vt = 0;
	__u32 frequency;
	char c;
	DvbParam_t param;

	dprintf("%s: '%s'\n", __FUNCTION__, URL);

	if( (i = parseURL(URL,&ud)) != 0 )
	{
		eprintf("offair: Error %d parsing '%s'\n",i,URL);
		return;
	}
	frequency = atol(ud.address);
	ts_id = atoi(ud.source);
	srv_id = ud.port;

	if(ud.stream[0] == '?')
	{
		i = 1;
		while(ud.stream[i])
		{
			switch(ud.stream[i])
			{
			case '&': ++i; break;
			case 'p': /* pcr=PCR */
				if(strncmp(&ud.stream[i],"pcr=",4) == 0)
				{
					for( j = i + 4; ud.stream[j]>= '0' && ud.stream[j] <= '9'; ++j);
					c = ud.stream[j];
					ud.stream[j] = 0;
					pcr = atoi(&ud.stream[i+4]);
					ud.stream[j] = c;
					i = j;
				}
				else
				{
					dprintf("%s: Error parsing dvb url at %d: 'pcr=' expected\n", __FUNCTION__,i);
					goto parsing_done;
				}
				break;
			case 'v': /* vt | vp */
				switch(ud.stream[i+1])
				{
				case 't': /* vt=VIDEO_TYPE */
					if( ud.stream[i+2] != '=' )
					{
						dprintf("%s: Error parsing dvb url at %d: '=' expected but '%c' found\n", __FUNCTION__,i,ud.stream[i+2]);
						goto parsing_done;
					}
					i += 3;
					if( strncasecmp(&ud.stream[i],"MPEG2",5)==0 )
					{
						vt = MPEG2;
						i += 5;
					}
					else if ( strncasecmp(&ud.stream[i],"H264",4)==0 )
					{
						vt = H264;
						i += 4;
					}
					else
					{
						dprintf("%s: Error parsing dvb url at %d: video type expected\n", __FUNCTION__,i,&ud.stream[i]);
						goto parsing_done;
					}
					break;
				case 'p': /* vp=VIDEO_PID */
					if( ud.stream[i+2] != '=' )
					{
						dprintf("%s: Error parsing dvb url at %d: '=' expected but '%c' found\n", __FUNCTION__,i,ud.stream[i+2]);
						goto parsing_done;
					}
					i += 3;
					for( j = i; ud.stream[j]>= '0' && ud.stream[j] <= '9'; ++j);
					c = ud.stream[j];
					ud.stream[j] = 0;
					vpid = atoi(&ud.stream[i]);
					ud.stream[j] = c;
					i = j;
					break;
				default:
					dprintf("%s: Error parsing dvb url at %d: vt or vp expected but v'%c' found\n", __FUNCTION__,i,ud.stream[i+1]);
					goto parsing_done;
				}
				break;  /* vt | vp */
			case 't':
				if( ud.stream[i+1] == 'p' && ud.stream[i+2] == '=' )
				{
					i += 3;
					for( j = i; ud.stream[j]>= '0' && ud.stream[j] <= '9'; ++j);
					c = ud.stream[j];
					ud.stream[j] = 0;
					tpid = atoi(&ud.stream[i]);
					ud.stream[j] = c;
					i = j;
				} else
				{
					dprintf("%s: Error parsing dvb url at %d: tp expected but t'%c' found\n", __FUNCTION__,i, ud.stream[i+1]);
					goto parsing_done;
				}
				break;
			case 'a': /* at | ap */
				switch(ud.stream[i+1])
				{
				case 't': /* at=AUDIO_TYPE */
					if( ud.stream[i+2] != '=' )
					{
						dprintf("%s: Error parsing dvb url at %d: '=' expected but '%c' found\n", __FUNCTION__,i,ud.stream[i+2]);
						goto parsing_done;
					}
					i += 3;
					if( strncasecmp(&ud.stream[i],"MP3",3)==0 )
					{
						at = MP3;
						i += 3;
					}
					else if ( strncasecmp(&ud.stream[i],"AAC",3)==0 )
					{
						at = AAC;
						i += 3;
					}
					else if ( strncasecmp(&ud.stream[i],"AC3",3)==0 )
					{
						at = AC3;
						i += 3;
					}
					else
					{
						dprintf("%s: Error parsing dvb url at %d: video type expected\n", __FUNCTION__,i);
						goto parsing_done;
					}
					break;
				case 'p': /* ap=AUDIO_PID */
					if( ud.stream[i+2] != '=' )
					{
						dprintf("%s: Error parsing dvb url at %d: '=' expected but '%c' found\n", __FUNCTION__,i,ud.stream[i+2]);
						goto parsing_done;
					}
					i += 3;
					for( j = i; ud.stream[j]>= '0' && ud.stream[j] <= '9'; ++j);
					c = ud.stream[j];
					ud.stream[j] = 0;
					apid = atoi(&ud.stream[i]);
					ud.stream[j] = c;
					i = j;
					break;
				default:
					dprintf("%s: Error parsing dvb url at %d: at or ap expected but a'%c' found\n", __FUNCTION__,i,ud.stream[i+1]);
					goto parsing_done;
				}
				break; /* at | ap */
			default:
				dprintf("%s: Error parsing dvb url at %d: unexpected symbol '%c' found\n", __FUNCTION__,i,ud.stream[i]);
				goto parsing_done;
			}
		}
		/* while(ud.stream[i]) */
	}
parsing_done:
	dprintf("%s: dvb url parsing done: freq=%ld srv_id=%d ts_id=%d vpid=%d apid=%d vtype=%d atype=%d pcr=%d\n", __FUNCTION__,
		frequency,srv_id,ts_id,vpid,apid,vt,at,pcr);


#ifdef ENABLE_PVR
	if ( appControlInfo.pvrInfo.dvb.channel >= 0 )
	{
		pvr_showStopPvr( interfaceInfo.currentMenu, (void*)-1 );
		return;
	}
#endif

	if ( appControlInfo.dvbInfo[which].active != 0 )
	{
		// force showState to NOT be triggered
		interfacePlayControl.activeButton = interfacePlayControlStop;
		offair_stopVideo(which, 0);
	}

	param.frequency = frequency;
	param.mode = DvbMode_Watch;
	param.vmsp = offair_getTuner();
	param.param.playParam.audioPid = apid;
	param.param.playParam.videoPid = vpid;
	param.param.playParam.textPid  = tpid;
	param.param.playParam.pcrPid = pcr;
	param.directory = NULL;
	param.media = NULL;

	interface_playControlSetInputFocus(inputFocusPlayControl);

	interface_playControlSetup(offair_play_callback, (void*)which, interfacePlayControlStop|interfacePlayControlPlay|interfacePlayControlPrevious|interfacePlayControlNext, URL, vpid != 0 ? thumbnail_channels : thumbnail_radio);
	interface_playControlSetDisplayFunction(offair_displayPlayControl);
	interface_playControlSetProcessCommand(offair_playControlProcessCommand);
	interface_playControlSetChannelCallbacks(offair_startNextChannel, appControlInfo.playbackInfo.playlistMode == playlistModeFavorites ? playlist_setChannel : offair_setChannel);

	offair_startDvbVideo(which, &param, srv_id, ts_id,  at, vt, TXT);

	playlist_setLastUrl(URL);

	if ( appControlInfo.dvbInfo[which].active != 0 )
	{
		interface_showMenu(0, 1);
	}
}

static char* offair_getLCNText(int field, void *pArg)
{
	return field == 0 ? offair_lcn_buf : NULL;
}

int offair_changeLCN(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_LCN"), MAX_MEMORIZED_SERVICES <= 100 ? "\\d{2}" : "\\d{3}" , offair_getUserLCN, offair_getLCNText, inputModeDirect, pArg);
	return 0;
}

static int offair_getUserLCN(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int serviceNumber = CHANNEL_INFO_GET_CHANNEL((int)pArg);
	int screen = CHANNEL_INFO_GET_SCREEN((int)pArg);
	int lcn;

	if( value == NULL )
	{
		return 1;
	}

	lcn = strtol(value,NULL,10);
	if( serviceNumber == lcn || lcn < 0 || lcn >= offair_serviceCount)
	{
		return 0;
	}
	offair_swapServices(lcn, serviceNumber);
	offair_exportServices(OFFAIR_SERVICES_FILENAME);
	offair_fillDVBTOutputMenu(screen);
	return 0;
}

static int offair_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int channelNumber;
	char URL[MAX_URL];

	dprintf("%s: in\n", __FUNCTION__);

	if( cmd->command == interfaceCommandRed )
	{
		appControlInfo.offairInfo.sorting = (appControlInfo.offairInfo.sorting + 1) % serviceSortCount;
		saveAppSettings();
		if(appControlInfo.offairInfo.sorting != serviceSortNone )
		{
			qsort( offair_indeces, offair_indexCount, sizeof(offair_indeces[0]), service_cmp );
		} else
		{
			offair_indexCount = 0;
			for( channelNumber = 0; channelNumber < offair_serviceCount; channelNumber++)
			{
				if( offair_services[channelNumber].service != NULL && (appControlInfo.offairInfo.dvbShowScrambled || dvb_getScrambled( offair_services[channelNumber].service ) == 0) && dvb_hasMedia(offair_services[channelNumber].service) )
				{
					offair_indeces[offair_indexCount++] = channelNumber;
				}
			}
		}
		offair_fillDVBTOutputMenu(screenMain);
		interface_displayMenu(1);
		return 0;
	}

	if(pMenu->selectedItem < 0 || (int)pArg < 0)
	{
		return 1;
	}

	channelNumber = CHANNEL_INFO_GET_CHANNEL((int)pArg);

	switch( cmd->command )
	{
		case interfaceCommandYellow:
			dvb_getServiceURL(offair_services[channelNumber].service, URL);
			eprintf("offair: Add to Playlist '%s'\n",URL);
			playlist_addUrl(URL, dvb_getServiceName(offair_services[channelNumber].service));
			return 0;
		case interfaceCommandBlue:
			snprintf(offair_lcn_buf, sizeof(offair_lcn_buf),"%d",channelNumber);
			offair_changeLCN(pMenu, pArg);
			return 0;
		case interfaceCommandGreen:
			dvb_getServiceDescription(offair_services[channelNumber].service, URL);
			eprintf("offaie: Channel %d info:\n%s\n", channelNumber, URL);
			interface_showMessageBox(URL, thumbnail_info, 0);
			return 0;
#ifdef ENABLE_MULTI_VIEW
		case interfaceCommandTV:
			 
#ifdef ENABLE_PVR
			if( appControlInfo.pvrInfo.dvb.channel < 0 &&
#endif
			    (dvb_getScrambled( offair_services[channelNumber].service ) == 0 || appControlInfo.offairInfo.dvbShowScrambled == SCRAMBLED_PLAY) && dvb_hasMediaType(offair_services[channelNumber].service, mediaTypeVideo) > 0
			  )
				offair_multiviewPlay(pMenu, pArg);
			return 0;
#endif
		default: ;
	}

	return 1;
}

int offair_getServiceCount()
{
	return offair_serviceCount;
}

int offair_getIndex(int index)
{
	EIT_service_t* service = dvb_getService(index);
	return offair_getServiceIndex(service);
}

int offair_getServiceIndex(EIT_service_t *service)
{
	int index;
	if( service == NULL )
	{
		return -2;
	}
	for( index = 0; index < offair_serviceCount; index++)
	{
		if( offair_services[index].service == service )
		{
			return index;
		}
	}
	return -1;
}

EIT_service_t* offair_getService(int index)
{
	return offair_services[index].service;
}

int offair_getCurrentServiceIndex(int which)
{
	if( appControlInfo.dvbInfo[which].active == 0)
	{
		return -1;
	}
	return dvb_getServiceIndex(offair_services[appControlInfo.dvbInfo[which].channel].service);
}

#ifdef ENABLE_STATS
int offair_findService(EIT_common_t *header)
{
	int i;
	for(i = 1; i < offair_serviceCount; i++)
		if (memcmp(&offair_services[i].common, header, sizeof(EIT_common_t)) == 0)
			return i;
	return -1;
}

static int offair_updateStats(int which)
{
	time_t now = time() - statsInfo.today;
	if( now >= 0 && now < 24*3600 )
	{
		now /= STATS_RESOLUTION;
		statsInfo.watched[now] = appControlInfo.dvbInfo[which].channel;
		stats_save();
	} else /* Date changed */
	{
		stats_init();
		offair_updateStats(which);
	}
	return 0;
}

static int offair_updateStatsEvent(void *pArg)
{
	int which = (int)pArg;
	if( appControlInfo.dvbInfo[which].active == 0 )
		return 1;
	offair_updateStats(which);
	if (appControlInfo.dvbInfo[which].active)
	{
		interface_addEvent(offair_updateStatsEvent, pArg, STATS_UPDATE_INTERVAL, 1);
	}
	return 0;
}
#endif

int offair_checkForUpdates()
{
	return 0;
}

/****************************** WIZARD *******************************/

static int wizard_infoTimerEvent(void *pArg)
{
	//dprintf("%s: update display\n", __FUNCTION__);
	if (wizardSettings != NULL)
	{
		interface_displayMenu(1);
		interface_addEvent(wizard_infoTimerEvent, (void*)screenMain, WIZARD_UPDATE_INTERVAL, 1);
	}

	return 0;
}

static int wizard_checkAbort(long frequency, int channelCount, tunerFormat tuner, long frequencyNumber, long frequencyMax)
{
	interfaceCommand_t cmd;

	dprintf("%s: in\n", __FUNCTION__);

	while ((cmd = helperGetEvent(0)) != interfaceCommandNone)
	{
		//dprintf("%s: got command %d\n", __FUNCTION__, cmd);
		if (cmd == interfaceCommandExit && wizardSettings->allowExit != 0)
		{
			wizardSettings->state = wizardStateDisabled;
			return -1;
		}
		if (wizardSettings->state == wizardStateInitialFrequencyScan)
		{
			if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk || cmd == interfaceCommandRed || cmd == interfaceCommandBack)
			{
				if (cmd == interfaceCommandRed || cmd == interfaceCommandBack)
				{
					wizardSettings->state = wizardStateConfirmLocation;
				}
				goto aborted;
			}
		} else if (wizardSettings->state == wizardStateInitialServiceScan)
		{
			if (cmd == interfaceCommandRed || cmd == interfaceCommandBack)
			{
				wizardSettings->state = wizardStateConfirmLocation;
				goto aborted;
			} else if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk)
			{
				goto aborted;
			}
		} else if (wizardSettings->state == wizardStateInitialFrequencyMonitor)
		{
			if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk || cmd == interfaceCommandRed || cmd == interfaceCommandBack)
			{
				if (cmd == interfaceCommandRed || cmd == interfaceCommandBack)
				{
					wizardSettings->state = wizardStateConfirmLocation;
				} else
				{
					wizardSettings->state = wizardStateInitialServiceScan;
				}
				goto aborted;
			}
		} else if (wizardSettings->state == wizardStateCustomFrequencyMonitor)
		{
			if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk || cmd == interfaceCommandRed || cmd == interfaceCommandBack)
			{
				goto aborted;
			}
		} else if (wizardSettings->state == wizardStateConfirmUpdate)
		{
			if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk || cmd == interfaceCommandRed || cmd == interfaceCommandBack)
			{
				if (cmd == interfaceCommandGreen || cmd == interfaceCommandEnter || cmd == interfaceCommandOk)
				{
					wizardSettings->state = wizardStateUpdating;
				}
				goto aborted;
			}
		}
	}

	return keepCommandLoopAlive ? 0 : -1;
aborted:
	//dprintf("%s: exit on command %d\n", __FUNCTION__, cmd);
	/* Flush any waiting events */
	helperGetEvent(1);
	return -1;
}

static int wizard_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{

	dprintf("%s: in\n", __FUNCTION__);

	if (cmd->command == interfaceCommandExit && wizardSettings->allowExit != 0)
	{
		wizardSettings->state = wizardStateDisabled;
		wizard_cleanup(0);
		return 1;
	} else if (wizardSettings->state == wizardStateCustomFrequencySelect)
	{
		if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
		{
			wizardSettings->state = wizardStateCustomFrequencyMonitor;

			//dprintf("%s: selected %d = %lu\n", __FUNCTION__, pMenu->selectedItem, (unsigned long)pMenu->menuEntry[pMenu->selectedItem].pArg);

			wizardSettings->frequencyIndex = pMenu->selectedItem;

			wizard_infoTimerEvent((void*)screenMain);
			dvb_frequencyScan( appControlInfo.dvbInfo[screenMain].tuner, (unsigned long)pMenu->menuEntry[pMenu->selectedItem].pArg, NULL, wizard_checkAbort, NULL, -1, (dvb_cancelFunctionDef*)wizard_checkAbort);
			wizard_cleanup(-1);
		} else if (cmd->command == interfaceCommandUp ||
			cmd->command == interfaceCommandDown)
		{
			interface_listMenuProcessCommand(pMenu, cmd);
		} else if (cmd->command == interfaceCommandBack ||
			cmd->command == interfaceCommandRed)
		{
			wizardSettings->state = wizardStateDisabled;
			wizard_cleanup(-1);
		}

		return 1;
	} else if (wizardSettings->state == wizardStateSelectLocation)
	{
		if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
		{
			int i;

			if (pMenu->selectedItem < pMenu->menuEntryCount)
			{
				strcpy(appControlInfo.offairInfo.profileLocation, pMenu->menuEntry[pMenu->selectedItem].pArg);
			}

			wizardSettings->state = wizardStateConfirmLocation;
			if (wizardSettings->locationFiles != NULL)
			{
				for (i=0; i<wizardSettings->locationCount; i++)
				{
					//dprintf("%s: free %s\n", __FUNCTION__, wizardSettings->locationFiles[i]->d_name);
					free(wizardSettings->locationFiles[i]);
				}
				free(wizardSettings->locationFiles);
				wizardSettings->locationFiles = NULL;
			}
		} else if (cmd->command == interfaceCommandUp ||
			cmd->command == interfaceCommandDown)
		{
			interface_listMenuProcessCommand(pMenu, cmd);
		} else if (cmd->command == interfaceCommandBack ||
			cmd->command == interfaceCommandRed)
		{
			wizardSettings->state = wizardStateConfirmLocation;
		}
	} else if (wizardSettings->state == wizardStateConfirmManualScan)
	{
		if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
		{
			output_fillDVBMenu((interfaceMenu_t*)&wizardHelperMenu, (void*)screenMain);
			wizard_cleanup(1);
		} else if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandBack)
		{
			wizardSettings->state = wizardStateConfirmLocation;
		}
	} else if (wizardSettings->state == wizardStateConfirmLocation)
	{
		if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
		{
			char *str;
			char buffer[2048];
			int res;

			//dprintf("%s: goto scan\n", __FUNCTION__);

			sprintf(buffer, PROFILE_LOCATIONS_PATH "/%s", appControlInfo.offairInfo.profileLocation);
			if (getParam(buffer, "FREQUENCIES", NULL, buffer))
			{
				unsigned long minfreq, minindex;

				//dprintf("%s: do scan '%s'\n", __FUNCTION__, buffer);

				wizardSettings->frequencyCount = wizardSettings->frequencyIndex = 0;

				/* Get list of frequencies */
				str = strtok(buffer, ",");
				while (str != NULL && wizardSettings->frequencyCount < 64)
				{
					unsigned long freqval = strtoul(str, NULL, 10);
					if (freqval < 1000)
					{
						freqval *= 1000000;
					} else if (freqval < 1000000)
					{
						freqval *= 1000;
					}
					//dprintf("%s: got freq %lu\n", __FUNCTION__, freqval);
					wizardSettings->frequency[wizardSettings->frequencyCount++] = freqval;
					str = strtok(NULL, ",");
				}

				wizardSettings->state = wizardStateInitialFrequencyScan;

				wizard_infoTimerEvent((void*)screenMain);

				//dprintf("%s: find any working freq\n", __FUNCTION__);

				/* Try to tune to any of specified frequencies. If none succeeded - use lowest */
				minfreq = wizardSettings->frequency[0];
				minindex = 0;
				for (wizardSettings->frequencyIndex=0; wizardSettings->frequencyIndex < wizardSettings->frequencyCount; wizardSettings->frequencyIndex++)
				{
					//dprintf("%s: tune to %lu\n", __FUNCTION__, wizardSettings->frequency[wizardSettings->frequencyIndex]);
					interface_displayMenu(1);
					if (minfreq > wizardSettings->frequency[wizardSettings->frequencyIndex])
					{
						minfreq = wizardSettings->frequency[wizardSettings->frequencyIndex];
						minindex = wizardSettings->frequencyIndex;
					}
					if( (res = dvb_frequencyScan( appControlInfo.dvbInfo[screenMain].tuner, wizardSettings->frequency[wizardSettings->frequencyIndex], NULL, NULL, NULL, -2, (dvb_cancelFunctionDef*)wizard_checkAbort)) == 1)
					{
						//dprintf("%s: found smth on %lu\n", __FUNCTION__, wizardSettings->frequency[wizardSettings->frequencyIndex]);
						minfreq = wizardSettings->frequency[wizardSettings->frequencyIndex];
						minindex = wizardSettings->frequencyIndex;
						break;
					}
					if (res == -1)
					{
						//dprintf("%s: scan abort\n", __FUNCTION__);
						break;
					}
					if (wizard_checkAbort(wizardSettings->frequency[wizardSettings->frequencyIndex], 0, appControlInfo.dvbInfo[screenMain].tuner, wizardSettings->frequencyIndex, wizardSettings->frequencyCount) == -1)
					{
						//dprintf("%s: user abort\n", __FUNCTION__);
						break;
					}
				}

				/* If user did not abort our scan - proceed to monitoring and antenna adjustment */
				if (wizardSettings->state == wizardStateInitialFrequencyScan)
				{
					int foundAll = 1;

					//dprintf("%s: monitor\n", __FUNCTION__);

					wizardSettings->frequencyIndex = minindex;

					wizardSettings->state = wizardStateInitialFrequencyMonitor;

					interface_displayMenu(1);

					/* Stay in infinite loop until user takes action */
					res = dvb_frequencyScan( appControlInfo.dvbInfo[screenMain].tuner, wizardSettings->frequency[wizardSettings->frequencyIndex], NULL, wizard_checkAbort, NULL, -1, (dvb_cancelFunctionDef*)wizard_checkAbort);

					//dprintf("%s: done monitoring!\n", __FUNCTION__);

					if (wizardSettings->state == wizardStateInitialServiceScan)
					{
						int lastServiceCount, currentServiceCount;
						//dprintf("%s: scan services\n", __FUNCTION__);

						offair_clearServiceList(0);

						lastServiceCount = dvb_getNumberOfServices();

						/* Go through all frequencies and scan for services */
						for (wizardSettings->frequencyIndex=0; wizardSettings->frequencyIndex < wizardSettings->frequencyCount; wizardSettings->frequencyIndex++)
						{
							//dprintf("%s: scan %lu\n", __FUNCTION__, wizardSettings->frequency[wizardSettings->frequencyIndex]);
							interface_displayMenu(1);
							res = dvb_frequencyScan( appControlInfo.dvbInfo[screenMain].tuner, wizardSettings->frequency[wizardSettings->frequencyIndex], NULL, NULL, NULL, 0, (dvb_cancelFunctionDef*)wizard_checkAbort);
							if (res == -1)
							{
								//dprintf("%s: scan abort\n", __FUNCTION__);
								break;
							}
							if (wizard_checkAbort(wizardSettings->frequency[wizardSettings->frequencyIndex], dvb_getNumberOfServices(), appControlInfo.dvbInfo[screenMain].tuner, wizardSettings->frequencyIndex, wizardSettings->frequencyCount) == -1)
							{
								//dprintf("%s: user abort service scan\n", __FUNCTION__);
								break;
							}

							if (offair_checkForUpdates())
							{
								wizardSettings->state = wizardStateConfirmUpdate;

								interface_displayMenu(1);

								while (wizard_checkAbort(0,0,0,0,0) != -1)
								{
									sleepMilli(500);
								}

								if (wizardSettings->state == wizardStateUpdating)
								{
									// TODO: update code here, save location and reboot
									sleep(3);
									system("reboot");
									break;
								}

								wizardSettings->state = wizardStateInitialServiceScan;
								interface_displayMenu(1);
							}

							currentServiceCount = dvb_getNumberOfServices();

							if (currentServiceCount <= lastServiceCount)
							{
								foundAll = 0;
							}
							lastServiceCount = currentServiceCount;
						}

						//dprintf("%s: done scanning %d!\n", __FUNCTION__, dvb_getNumberOfServices());

						/* If user didn't cancel our operation, we now can display channel list */
						if (wizardSettings->state == wizardStateInitialServiceScan)
						{
							int tmp;

							interface_removeEvent(wizard_infoTimerEvent, (void*)screenMain);

							wizard_cleanup(1);

							tmp = appControlInfo.playbackInfo.bAutoPlay;
							appControlInfo.playbackInfo.bAutoPlay = 0;
							interface_menuActionShowMenu(pMenu, (void*)&DVBTOutputMenu[screenMain]);
							appControlInfo.playbackInfo.bAutoPlay = tmp;

							if (foundAll == 0)
							{
								interface_showMessageBox(_T("SETTINGS_WIZARD_NOT_ALL_CHANNELS_FOUND"), thumbnail_info, 10000);
							}

							return 1;
						}
					}
				}

				interface_removeEvent(wizard_infoTimerEvent, (void*)screenMain);

				//dprintf("%s: done with scanning\n", __FUNCTION__);

				if (wizardSettings->state == wizardStateDisabled)
				{
					wizard_cleanup(0);
				}
			} else
			{
				wizardSettings->state = wizardStateConfirmManualScan;
			}

		} else if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandBack)
		{
			char buffer[2048];
			int res, i, found;

			//dprintf("%s: goto location list\n", __FUNCTION__);

			found = 0;

			interface_clearMenuEntries((interfaceMenu_t*)&wizardHelperMenu);

			/* Fill menu with locations */
			res = scandir(PROFILE_LOCATIONS_PATH, &wizardSettings->locationFiles, NULL, alphasort);

			if (res > 0)
			{
				for (i=0;i<res; i++)
				{
					/* Skip if we have found suitable file or file is a directory */
					if ((wizardSettings->locationFiles[i]->d_type & DT_DIR))
					{
						continue;
					}

					//dprintf("Test location %s\n", wizardSettings->locationFiles[i]->d_name);

					sprintf(buffer, PROFILE_LOCATIONS_PATH "/%s", wizardSettings->locationFiles[i]->d_name);
					if (getParam(buffer, "LOCATION_NAME", NULL, buffer))
					{
						interface_addMenuEntry((interfaceMenu_t*)&wizardHelperMenu, buffer, NULL, wizardSettings->locationFiles[i]->d_name, thumbnail_info);
						found = 1;
					}
				}

				if (found)
				{
					wizardSettings->locationCount = res;
					wizardSettings->state = wizardStateSelectLocation;
				} else
				{
					if (wizardSettings->locationFiles != NULL)
					{
						for (i=0; i<res; i++)
						{
							//dprintf("free %s\n", wizardSettings->locationFiles[i]->d_name);
							free(wizardSettings->locationFiles[i]);
						}
						free(wizardSettings->locationFiles);
						wizardSettings->locationFiles = NULL;
					}
				}
			} else
			{
				// show error
			}
		}
	}
	interface_displayMenu(1);
	return 1;
}

static int wizard_sliderCallback(int id, interfaceCustomSlider_t *info, void *pArg)
{
	if (id < 0 || info == NULL)
	{
		return 1;
	}

	//dprintf("%s: get info 0x%08X\n", __FUNCTION__, info);

	switch (id)
	{
	case 0:
		sprintf(info->caption, _T("SETTINGS_WIZARD_SCANNING_SERVICES_SLIDER"), dvb_getNumberOfServices());
		info->min = 0;
		info->max = wizardSettings->frequencyCount;
		info->value = wizardSettings->frequencyIndex;
		break;
	default:
		return -1;
	}

	return 1;
}

static void wizard_displayCallback(interfaceMenu_t *pMenu)
{
	char *str, *helpstr, *infostr;
	char buffer[MAX_MESSAGE_BOX_LENGTH];
	char buffer2[MAX_MESSAGE_BOX_LENGTH];
	char cmd[1024];
	int  icons[4] = { statusbar_f1_cancel, statusbar_f2_ok, 0, 0 };
	int fh, fa;
	int x,y,w,h,pos, centered;

	//dprintf("%s: in\n", __FUNCTION__);

	/*sprintf(buffer, "%d", wizardSettings->state);
	gfx_drawText(DRAWING_SURFACE, pgfx_font, 0xFF, 0xFF, 0xFF, 0xFF, interfaceInfo.clientX+interfaceInfo.paddingSize*2, interfaceInfo.clientY-interfaceInfo.paddingSize*2, buffer, 0, 0);*/

	x = interfaceInfo.clientX;
	y = interfaceInfo.clientY;
	w = interfaceInfo.clientWidth;
	h = interfaceInfo.clientHeight;

	if (wizardSettings->state != wizardStateSelectLocation &&
		wizardSettings->state != wizardStateCustomFrequencySelect &&
		wizardSettings->state != wizardStateSelectChannel)
	{
		interface_drawRoundBox(x, y, w, h);
	}

	pgfx_font->GetHeight(pgfx_font, &fh);
	pgfx_font->GetHeight(pgfx_font, &fa);

	switch (wizardSettings->state)
	{
	case wizardStateSelectLocation:
	case wizardStateCustomFrequencySelect:
	case wizardStateSelectChannel:
		{
			interface_listMenuDisplay(pMenu);
		} break;
	default:
		{
			customSliderFunction showSlider = offair_sliderCallback;
			infostr = NULL;
			centered = ALIGN_LEFT;

			if (wizardSettings->state == wizardStateInitialFrequencyScan)
			{
				helpstr = _T("SETTINGS_WIZARD_SCANNING_FREQUENCY_HINT");

				str = buffer;
				sprintf(buffer, _T("SETTINGS_WIZARD_SCANNING_FREQUENCY"), wizardSettings->frequency[wizardSettings->frequencyIndex]/1000000);
			} else if (wizardSettings->state == wizardStateInitialFrequencyMonitor ||
				wizardSettings->state == wizardStateCustomFrequencyMonitor)
			{
				static time_t lastUpdate = 0;
				static int rating = 5;
				char tindex[] = "SETTINGS_WIZARD_DIAG_SIGNAL_0";

				if (time(NULL)-lastUpdate > 1)
				{
					uint16_t snr, signal;
					uint32_t ber, uncorrected_blocks;
					fe_status_t status;

					rating = 5;
					lastUpdate = time(NULL);

					status = dvb_getSignalInfo(appControlInfo.dvbInfo[screenMain].tuner, &snr, &signal, &ber, &uncorrected_blocks);

					if (status == 0)
					{
						rating = min(rating, 1);
					}

					if (uncorrected_blocks > BAD_UNC)
					{
						rating = min(rating, 2);
					}

					if (ber > BAD_BER)
					{
						rating = min(rating, 3);
					}

					if (signal < AVG_SIGNAL)
					{
						rating = min(rating, 4);
					}

					if (signal < BAD_SIGNAL)
					{
						rating = min(rating, 3);
					}
				}

				tindex[strlen(tindex)-1] += rating;

				sprintf(cmd, PROFILE_LOCATIONS_PATH "/%s", appControlInfo.offairInfo.profileLocation);
				getParam(cmd, "TRANSPONDER_LOCATION", _T("SETTINGS_WIZARD_NO_TRANSPONDER_LOCATION"), cmd);

				helpstr = buffer2;
				sprintf(buffer2, _T(tindex), cmd);

				if (wizardSettings->state == wizardStateCustomFrequencyMonitor)
				{
					icons[0] = 0;
					icons[1] = 0;
				} else if (rating >= 3)
				{
					strcat(buffer2, " ");
					strcat(buffer2, _T("SETTINGS_WIZARD_DIAG_SIGNAL_DO_SCAN"));
				}

				str = buffer;
				sprintf(buffer, _T("SETTINGS_WIZARD_MONITORING_FREQUENCY"), wizardSettings->frequency[wizardSettings->frequencyIndex]/1000000);
			} else if (wizardSettings->state == wizardStateInitialServiceScan)
			{
				helpstr = _T("SETTINGS_WIZARD_SCANNING_SERVICES_HINT");

				showSlider = wizard_sliderCallback;

				str = buffer;
				sprintf(buffer, _T("SETTINGS_WIZARD_SCANNING_SERVICES"), wizardSettings->frequency[wizardSettings->frequencyIndex]/1000000);
			} else if (wizardSettings->state == wizardStateConfirmUpdate)
			{
				showSlider = NULL;

				helpstr = _T("SETTINGS_WIZARD_CONFIRM_UPDATE_HINT");

				str = _T("SETTINGS_WIZARD_CONFIRM_UPDATE");
			} else if (wizardSettings->state == wizardStateConfirmManualScan)
			{
				showSlider = NULL;

				helpstr = _T("SETTINGS_WIZARD_NO_FREQUENCIES_HINT");

				// TODO: add version display, etc

				str = _T("SETTINGS_WIZARD_NO_FREQUENCIES");
			} else
			{
				showSlider = NULL;

				helpstr = _T("SETTINGS_WIZARD_CONFIRM_LOCATION_HINT");

				sprintf(cmd, PROFILE_LOCATIONS_PATH "/%s", appControlInfo.offairInfo.profileLocation);
				if (getParam(cmd, "LOCATION_NAME", NULL, buffer))
				{
					infostr = buffer;
				} else
				{
					infostr = _T("SETTINGS_WIZARD_NO_LOCATIONS");
				}
				//interface_displayCustomTextBoxColor(interfaceInfo.clientX+interfaceInfo.clientWidth/2, interfaceInfo.clientY+interfaceInfo.clientHeight/2-fh*2, str, NULL, 0, &rect, 0, NULL, 0,0,0,0, 0xFF,0xFF,0xFF,0xFF, pgfx_font);

				str = _T("SETTINGS_WIZARD_CONFIRM_LOCATION");
				centered = ALIGN_CENTER;
			}

			pos = y;
			pos += interface_drawTextWW(pgfx_font, 0xFF, 0xFF, 0xFF, 0xFF, x, pos, w, h, str, centered)+interfaceInfo.paddingSize;
			if (showSlider != NULL)
			{
				if (pos < y+fh*3+interfaceInfo.paddingSize)
				{
					pos = y+fh*3+interfaceInfo.paddingSize;
				}
				pos += interface_displayCustomSlider(showSlider, (void*)screenMain, 1, interfaceInfo.screenWidth/2-(interfaceInfo.clientWidth)/2, pos, interfaceInfo.clientWidth, pgfx_smallfont)+interfaceInfo.paddingSize;
			} else if (infostr != NULL)
			{
				if (pos < y+fh*4+interfaceInfo.paddingSize)
				{
					pos = y+fh*4+interfaceInfo.paddingSize;
				}
				pos += interface_drawTextWW(pgfx_font, 0xFF, 0xFF, 0xFF, 0xFF, x, pos, w, h, infostr, ALIGN_CENTER)+interfaceInfo.paddingSize;
			}
			if (pos < y+fh*10+interfaceInfo.paddingSize)
			{
				pos = y+fh*10+interfaceInfo.paddingSize;
			}
			pos += interface_drawTextWW(pgfx_smallfont, 0xFF, 0xFF, 0xFF, 0xFF, x, pos, w, h, helpstr, ALIGN_LEFT)+interfaceInfo.paddingSize;
			if (pos < y+h-INTERFACE_STATUSBAR_ICON_HEIGHT)
			{
				int i, n, icons_w;

				n = 0;
				for( i = 0; i < 4; i++)
				{
					if( icons[i] > 0)
					{
						n++;
					}
				}
				icons_w = n * INTERFACE_STATUSBAR_ICON_WIDTH + (n-1)*3*interfaceInfo.paddingSize;

				x = x+(w - icons_w) / 2;
				y = y+h-INTERFACE_STATUSBAR_ICON_HEIGHT;
				for( i = 0; i < 4; i++)
				{
					if( icons[i] > 0)
					{
						interface_drawImage(DRAWING_SURFACE, resource_thumbnails[icons[i]], x, y, INTERFACE_STATUSBAR_ICON_WIDTH, INTERFACE_STATUSBAR_ICON_HEIGHT, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignLeft|interfaceAlignTop, 0, 0);
						x += INTERFACE_STATUSBAR_ICON_WIDTH + interfaceInfo.paddingSize * 3;
					}
				}
			}
			//interface_displayCustomTextBoxColor(interfaceInfo.clientX+interfaceInfo.clientWidth/2, interfaceInfo.clientY+fh*2+fh/2, str, NULL, 0, &rect, 0, NULL, 0,0,0,0, 0xFF,0xFF,0xFF,0xFF, pgfx_font);
			//interface_displayCustomTextBoxColor(interfaceInfo.clientX+interfaceInfo.clientWidth/2, interfaceInfo.clientY+interfaceInfo.clientHeight-fh*6+fh/2, helpstr, NULL, 0, &rect, 0, icons, 0,0,0,0, 0xFF,0xFF,0xFF,0xFF, pgfx_smallfont);



		} break;
	}
}

static int wizard_show(int allowExit, int displayMenu, interfaceMenu_t *pFallbackMenu, unsigned long monitor_only_frequency)
{
	char cmd[1024];

	dprintf("%s: %lu\n", __FUNCTION__, monitor_only_frequency);

	sprintf(cmd, PROFILE_LOCATIONS_PATH "/%s", appControlInfo.offairInfo.profileLocation);
	if (monitor_only_frequency == 0 && !getParam(cmd, "LOCATION_NAME", NULL, NULL))
	{
		return 0;
	}

	gfx_stopVideoProviders(screenMain);

	interfaceInfo.enableClock = 0;
	appControlInfo.playbackInfo.streamSource = streamSourceNone;
	appControlInfo.dvbInfo[screenMain].tuner = offair_getTuner();
	wizardHelperMenu.baseMenu.selectedItem = 0;

	wizardSettings = dmalloc(sizeof(wizardSettings_t));
	memset(wizardSettings, 0, sizeof(wizardSettings_t));

	wizardSettings->pFallbackMenu = pFallbackMenu;
	wizardSettings->allowExit = allowExit;
	wizardSettings->state = monitor_only_frequency > 0 ? (monitor_only_frequency == (unsigned long)-1 ? wizardStateCustomFrequencySelect : wizardStateCustomFrequencyMonitor) : wizardStateConfirmLocation;
	wizardSettings->frequency[wizardSettings->frequencyIndex] = monitor_only_frequency;

	if (wizardSettings->state == wizardStateCustomFrequencySelect)
	{
		list_element_t *cur;
		EIT_service_t *service;
		unsigned long freq, i, found;

		interface_clearMenuEntries((interfaceMenu_t*)&wizardHelperMenu);

		wizardSettings->frequencyCount = 0;

		cur = dvb_services;
		while (cur != NULL)
		{
			service = (EIT_service_t*)cur->data;
			freq = service->media.frequency;

			found = 0;
			for (i=0; i<wizardSettings->frequencyCount; i++)
			{
				if (freq == wizardSettings->frequency[i])
				{
					found = 1;
					break;
				}
				if (freq < wizardSettings->frequency[i])
				{
					break;
				}
			}

			if (!found)
			{
				wizardSettings->frequencyCount++;
				for (; i<wizardSettings->frequencyCount; i++)
				{
					found = wizardSettings->frequency[i];
					wizardSettings->frequency[i] = freq;
					freq = found;
				}
			}

			cur = cur->next;
		}

		for (i=0; i<wizardSettings->frequencyCount; i++)
		{
			sprintf(cmd, "%lu %s", wizardSettings->frequency[i]/1000000, _T("MHZ"));
			interface_addMenuEntry((interfaceMenu_t*)&wizardHelperMenu, cmd, NULL, (void*)wizardSettings->frequency[i], thumbnail_info);
		}
	}

	interfaceInfo.currentMenu = (interfaceMenu_t*)&wizardHelperMenu;

	interface_showMenu(1, displayMenu);
	interfaceInfo.lockMenu = 1;

	if (monitor_only_frequency > 0 && monitor_only_frequency != (unsigned long)-1)
	{
		wizard_infoTimerEvent((void*)screenMain);
		dvb_frequencyScan( appControlInfo.dvbInfo[screenMain].tuner, monitor_only_frequency, NULL, wizard_checkAbort, NULL, -1, (dvb_cancelFunctionDef*)wizard_checkAbort);
		wizard_cleanup(-1);
	}

	return 1;
}

static void wizard_cleanup(int finished)
{
	int i;

	interface_removeEvent(wizard_infoTimerEvent, (void*)screenMain);

	interfaceInfo.lockMenu = 0;
	interfaceInfo.enableClock = 1;

	if (finished == 1)
	{
		dvb_exportServiceList(appControlInfo.dvbCommonInfo.channelConfigFile);
		appControlInfo.offairInfo.wizardFinished = 1;
		saveAppSettings();
	} else if (finished == 0)
	{
		offair_clearServiceList(0);
		dvb_readServicesFromDump(appControlInfo.dvbCommonInfo.channelConfigFile);
	}

	if (finished != -1)
	{
		offair_fillDVBTMenu();
		offair_fillDVBTOutputMenu(screenMain);
	}

	if (wizardSettings->locationFiles != NULL)
	{
		for (i=0; i<wizardSettings->locationCount; i++)
		{
			//dprintf("%s: free %s\n", __FUNCTION__, wizardSettings->locationFiles[i]->d_name);
			free(wizardSettings->locationFiles[i]);
		}
		free(wizardSettings->locationFiles);
		wizardSettings->locationFiles = NULL;
	}

	if (wizardSettings->pFallbackMenu != NULL)
	{
		interface_menuActionShowMenu((interfaceMenu_t*)&wizardHelperMenu, (void*)wizardSettings->pFallbackMenu);
	}

	dfree(wizardSettings);
	wizardSettings = NULL;
}

int wizard_init()
{
	createListMenu(&wizardHelperMenu, NULL, thumbnail_logo, NULL, NULL,
		/* interfaceInfo.clientX, interfaceInfo.clientY,
		interfaceInfo.clientWidth, interfaceInfo.clientHeight,*/ interfaceListMenuIconThumbnail,//interfaceListMenuIconThumbnail,
		NULL, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&wizardHelperMenu, wizard_keyCallback);
	interface_setCustomDisplayCallback((interfaceMenu_t*)&wizardHelperMenu, wizard_displayCallback);

	if (appControlInfo.offairInfo.wizardFinished == 0 || appControlInfo.offairInfo.profileLocation[0] == 0) // wizard
	{
		dprintf("%s: start init wizard\n", __FUNCTION__);
		if (wizard_show(0, 0, NULL, 0) == 0 && dvb_getNumberOfServices() == 0)
		{
			//interface_showMessageBox(_T("SETTINGS_WIZARD_NO_LOCATIONS"), thumbnail_warning, 5000);

#ifdef ENABLE_DVB_DIAG
			output_fillDVBMenu((interfaceMenu_t*)&wizardHelperMenu, (void*)screenMain);
#endif
		}
	}

	return 0;
}

#endif /* ENABLE_DVB */

int offair_getLocalEventTime(EIT_event_t *event, struct tm *local_tm, time_t *local_time_t)
{
	time_t local_time;
	if(event == NULL )
	{
		return -2;
	}
	if( event->start_time == TIME_UNDEFINED )
	{
		return 1;
	}
	local_time = event_start_time( event );
	if( local_time_t )
	{
		*local_time_t = local_time;
	}
	if( local_tm )
	{
		localtime_r( &local_time, local_tm );
	}
	return 0;
}

time_t offair_getEventDuration(EIT_event_t *event)
{
	if(event == NULL || event->duration == TIME_UNDEFINED)
		return 0;

	return decode_bcd_time(event->duration);
}

int offair_findCurrentEvent(list_element_t *schedule, time_t now, EIT_event_t **pevent, time_t *event_start, time_t *event_length, struct tm *start)
{
	/* Find current programme */
	EIT_event_t *event = NULL;
	list_element_t *element;

	*pevent = NULL;
	for( element = schedule; element != NULL; element = element->next )
	{
		event = (EIT_event_t *)element->data;
		offair_getLocalEventTime( event, start, event_start );
		if( *event_start > now )
		{
			//dprintf("%s: %d (%s) %s event_start > now\n", __func__, channel, streams.items[channel].session_name, event->description.event_name);
			event = NULL;
			break;
		}
		*event_length = offair_getEventDuration( event );
		if( *event_start+*event_length >= now )
			break;
		else
			continue;
	}
	if( element != NULL && event != NULL )
	{
		*pevent = event;
		return 0;
	}
	return 1;
}

/*********************************************************************/

//#endif
