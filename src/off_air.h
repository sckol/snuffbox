#if !defined(__OFF_AIR_H)
#define __OFF_AIR_H

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

#include <time.h>

#include "dvb.h"
#include "interface.h"

/*******************
* EXPORTED MACROS  *
********************/

#define interfaceMenuEPG (3)

#define CHANNEL_INFO_SET(screen, channel) ((screen << 16) | (channel))
#define CHANNEL_INFO_GET_SCREEN(info)     (info >> 16)
#define CHANNEL_INFO_GET_CHANNEL(info)    (info & 0xFFFF)
#define MAX_MEMORIZED_SERVICES MENU_MAX_ENTRIES

/* EPG Record menu layout */

#define ERM_CHANNEL_NAME_LENGTH (100)
/* Current channel is always visible and not counted */
#define ERM_VISIBLE_CHANNELS (5)
#define ERM_DISPLAYING_HOURS (3)
#define ERM_TIMESTAMP_WIDTH  (3)
#define ERM_ICON_SIZE        (34)

/* DVB play control layout */
#define DVBPC_STATUS_ICON_SIZE 22

/* EPG Record menu colors */

#define ERM_TIMELINE_RED   121
#define ERM_TIMELINE_GREEN 121
#define ERM_TIMELINE_BLUE  121
#define ERM_TIMELINE_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_TIMESTAMP_RED   196
#define ERM_TIMESTAMP_GREEN 196
#define ERM_TIMESTAMP_BLUE  196
#define ERM_TIMESTAMP_ALPHA 255

#define ERM_CURRENT_TITLE_RED   121
#define ERM_CURRENT_TITLE_GREEN 121
#define ERM_CURRENT_TITLE_BLUE  121
#define ERM_CURRENT_TITLE_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_TITLE_RED   160
#define ERM_TITLE_GREEN 160
#define ERM_TITLE_BLUE  160
#define ERM_TITLE_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_CELL_START_RED   40
#define ERM_CELL_START_GREEN 40
#define ERM_CELL_START_BLUE  40
#define ERM_CELL_START_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_HIGHLIGHTED_CELL_RED   84
#define ERM_HIGHLIGHTED_CELL_GREEN 239
#define ERM_HIGHLIGHTED_CELL_BLUE  58
#define ERM_HIGHLIGHTED_CELL_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_CELL_RED   69
#define ERM_CELL_GREEN 117
#define ERM_CELL_BLUE  165
#define ERM_CELL_ALPHA INTERFACE_BACKGROUND_ALPHA

#define ERM_RECORD_RED   255
#define ERM_RECORD_GREEN 24
#define ERM_RECORD_BLUE  24
#define ERM_RECORD_ALPHA INTERFACE_BACKGROUND_ALPHA

#define INFO_TIMER_PERIOD (100)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef struct
{
	EIT_common_t common;
	EIT_service_t *service;
	/* First EPG event which fit to current timeline.
	Updated on each call to offair_initEPGRecordMenu. */
	list_element_t *first_event;
} service_index_t;

typedef struct
{
	interfaceMenu_t baseMenu;

	int currentService;
	int highlightedService;
	int serviceOffset;

	time_t curOffset;
	time_t minOffset;
	time_t maxOffset;

	int displayingHours;
	int timelineX;
	int timelineWidth;
	float pps; //pixel per second

	list_element_t *highlightedEvent;
} interfaceEpgMenu_t;

/*******************
* EXPORTED DATA    *
********************/

#ifdef ENABLE_DVB
/* Service index in offair_service used as channel number */
extern service_index_t offair_services[MAX_MEMORIZED_SERVICES];
extern int  offair_serviceCount;

extern interfaceListMenu_t DVBTMenu;
extern interfaceListMenu_t DVBTOutputMenu[];
#endif

extern interfaceEpgMenu_t  EPGRecordMenu;
extern interfaceColor_t    genre_colors[];

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef ENABLE_DVB
/**
*   @brief Function used to build the DVB T menu data structures
*
*   @retval void
*/
void offair_buildDVBTMenu(interfaceMenu_t *pParent);

/**
*   @brief Function used to obtain a tuner
*
*   @retval The most suitable tuner to be used
*/
tunerFormat offair_getTuner(void);

/**
*   @brief Function used to perform a channel change
* 
*   @param pArg     I       Channel to change to (void* is cast to int)
*
*   @retval The most suitable tuner to be used
*/
int offair_channelChange(interfaceMenu_t* pMenu, void* pArg);

/**
*   @brief Function used to start DVB T output
*
*   @param which    I       Screen to start video on
*
*   @retval void
*/
void offair_startVideo(int which);

/**
*   @brief Function used to stop DVB T output
*
*   @param which    I       Screen to stop video on

 *   @retval void
*/
void offair_stopVideo(int which, int reset);

int  offair_serviceScan(interfaceMenu_t *pMenu, void* pArg);

int  offair_frequencyScan(interfaceMenu_t *pMenu, void* pArg);

int  offair_frequencyScan(interfaceMenu_t *pMenu, void* pArg);

int  offair_frequencyMonitor(interfaceMenu_t *pMenu, void* pArg);

int  offair_wizardStart(interfaceMenu_t *pMenu, void* pArg);

void offair_clearServiceList(int permanent);

/*
DVB_URL    ::= dvb://[TS_ID@]FREQUENCY[:SERVICE_ID]/[PARAM]

PARAMS     ::= ?PARAM{&PARAM}
PARAM      ::= vt=VIDEO_TYPE | vp=VIDEO_PID | at=AUDIO_TYPE | ap=AUDIO_PID | pcr=PCR | tp=TELETXT_PID
AUDIO_TYPE ::= MPEG | AAC | AC3
VIDEO_TYPE ::= MPEG2 | MPEGTS | MPEG4 | H264
FREQUENCY  ::= ulong
TS_ID      ::= ushort
PCR        ::= ushort
SERVICE_ID ::= ushort
VIDEO_PID  ::= ushort
AUDIO_PID  ::= ushort
TELETXT_PID::= ushort
*/

void offair_playURL(char* URL, int which);

void offair_initServices();

void offair_clearServices();

void offair_cleanupMenu();

int  offair_enterDVBTMenu(interfaceMenu_t *pMenu, void* pArg);

/* Get offair service index (LCN) by dvb channel index */
int  offair_getIndex(int index);

int  offair_getServiceIndex(EIT_service_t *service);

int  offair_getServiceCount();

int  offair_getCurrentServiceIndex(int which);

EIT_service_t* offair_getService(int index);

void offair_fillDVBTMenu();

void offair_fillDVBTOutputMenu(int which);

int  offair_epgEnabled();

int  offair_frequencyMonitor(interfaceMenu_t *pMenu, void* pArg);

int  offair_checkForUpdates(void);

#endif /* ENABLE_DVB */

int  offair_getLocalEventTime(EIT_event_t *event, struct tm *local_tm, time_t *local_time_t);

time_t offair_getEventDuration(EIT_event_t *event);

/** Find schedule event which runs at specified time.
 *  @param[in]  schedule     EPG schedule with list of EIT_event_t elements
 *  @param[in]  now
 *  @param[out] pevent       Pointer to found event will be stored at this address
 *  @param[out] event_start  Value stored will be valid only on successfull call
 *  @param[out] event_length Value stored will be valid only on successfull call
 *  @param[out] start        Can be null
 *  @return 0 if event found, 1 if no such event, *pevent will be set to NULL
 */
int offair_findCurrentEvent(list_element_t *schedule, time_t now, EIT_event_t **pevent, time_t *event_start, time_t *event_length, struct tm *start);

#ifdef ENABLE_PVR
int  offair_initEPGRecordMenu(interfaceMenu_t *pMenu, void *pArg);

int  offair_startPvrVideo(int which);
#endif

#endif /* __OFF_AIR_H      Do not add any thing below this line */
