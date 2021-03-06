#if !defined(__DVB_H)
#define __DVB_H

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

/** @file dvb.h DVB playback backend
 */
/** @defgroup dvb DVB features
 *  @ingroup StbMainApp
 *  In DVB service and channels are synonymous and freely interchanged
 */
/** @defgroup dvb_instance DVB instance features
 *  @ingroup dvb
 */
/** @defgroup dvb_service DVB service features
 *  @ingroup dvb
 */

/*******************
* INCLUDE FILES    *
********************/

#include "defines.h"
#ifdef ENABLE_DVB

#include "platform.h"
#include "sdp.h"
#include "service.h"

#include "app_info.h"
#include "dvb_types.h"

/*******************
* EXPORTED MACROS  *
********************/

#define SCALE_FACTOR       (10000)

#define MAX_SIGNAL         (180)
#define AVG_SIGNAL         (150)
#define BAD_SIGNAL         (70)

#define MAX_BER            (50000)
#define BAD_BER            (20000)

#define MAX_UNC            (100)
#define BAD_UNC            (0)

#define BAD_CC             (0)

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef int dvb_displayFunctionDef(long , int, tunerFormat, long, long);

typedef int dvb_cancelFunctionDef(void);

typedef struct dvb_filePosition {
	int index;
	int offset;
} DvbFilePosition_t;

typedef struct __dvb_playParam {
	int audioPid;
	int videoPid;
	int textPid;
	int pcrPid;
	int *pEndOfFile;
	DvbFilePosition_t position;
} DvbPlayParam_t;

typedef struct __dvb_liveParam {
	int channelIndex;
	int audioIndex;
} DvbLiveParam_t;

#ifdef ENABLE_MULTI_VIEW
typedef struct __dvb_multiParam {
	int channels[4];
} DvbMultiParam_t;
#endif

/** @ingroup dvb_instance
 * DVB watch settings used to initialize tuner for specified frequency and PID filters
 */
typedef struct __dvb_param {
	DvbMode_t mode;
	int vmsp;
	char *directory;
	union 
	{
		DvbLiveParam_t liveParam;
		DvbPlayParam_t playParam;
#ifdef ENABLE_MULTI_VIEW
		DvbMultiParam_t multiParam;
#endif
	} param;
	EIT_media_config_t *media;
	__u32 frequency;
} DvbParam_t;

/*******************
* EXPORTED DATA    *
********************/

extern list_element_t *dvb_services;

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

/**  @ingroup dvb
 *   @brief Function used to initialise the DVB component
 *
 *   Must be called before initiating offair
 */
void dvb_init(void);

/**  @ingroup dvb
 *   @brief Function used to terminate the DVB component
 *
 *   Must be called after terminating offair
 */
void dvb_terminate(void);

/**  @ingroup dvb
 *   @brief Returns the fe_type of the FrontEnd device
 *
 *   @param[in]  tuner               Tuner to use
 *
 *   @return fe_type or -1 if failed
 */
int dvb_getType(tunerFormat tuner);

/**  @ingroup dvb
 *   @brief For the given tuner returns the Frontend's freq. limits
 *
 *   @param[out] low_freq  Initial search frequency
 *   @param[out] high_freq Final search frequency
 *   @param[out] freq_step Frequency step size
 *   @param[in]  tuner     Tuner to use
 *
 *   @return fe_type or -1 if failed
 */
int dvb_getTuner_freqs(tunerFormat tuner, __u32 * low_freq, __u32 * high_freq, __u32 * freq_step);

/**  @ingroup dvb_instance
 *   @brief Function used to start DVB decoding of a given channel
 *
 *   @param[in]  pParam    DVB startup parameters
 *
 *   @return 0 on success
 *   @sa dvb_stopDVB()
 */
int dvb_startDVB(DvbParam_t *pParam);

/**  @ingroup dvb_instance
 *   @brief Function used to stop DVB decoding from a given tuner
 *
 *   @param[in]  vmsp      DVB tuner/vmsp to be used
 *   @param[in]  reset     Flag to indicate if the tuner will be put into sleep mode
 *
 *   @return 0 on success
 *   @sa dvb_startDVB()
 */
int dvb_stopDVB(int vmsp, int force);

/**  @ingroup dvb_instance
 *   @brief Change audio PID of currently running channel
 *
 *   @param[in]  vmsp     DVB tuner/vmsp to be used
 *   @param[in]  aPID     New audio PID to be used
 *
 *   @return 0 on success
 */
int dvb_changeAudioPid(int vmsp, short unsigned int aPID);

/**  @ingroup dvb_instance
 *   @brief Get signal info from specified tuner
 *
 *   @param[in]  tuner     Tuner to be used
 *   @param[out] snr
 *   @param[out] signal
 *   @param[out] ber
 *   @param[out] uncorrected_blocks
 *
 *   @return 0 on success
 */
int dvb_getSignalInfo(tunerFormat tuner, uint16_t *snr, uint16_t *signal, uint32_t *ber, uint32_t *uncorrected_blocks);

/**  @ingroup dvb_instance
 *   @ingroup dvb_service
 *   @brief Function used to scan for DVB-T channels
 *
 *   @param[in]  tuner     Tuner to use
 *   @param[in]  pFunction Callback function
 *
 *   @return 0 on success
 */
int dvb_serviceScan( tunerFormat tuner, dvb_displayFunctionDef* pFunction);

/**  @ingroup dvb_instance
 *   @ingroup dvb_service
 *   @brief Function used to scan for DVBT channels on custom frequency
 *
 *   @param[in]  tuner             Tuner to use
 *   @param[in]  frequency         Frequency to scan
 *   @param[in]  media             Tune settings
 *   @param[in]  pFunction         Callback function to display progress and check for user cancel
 *   @param[in]  scan_network      NIT table to scan
 *   @param[in]  save_service_list Allow saving channel list to permanent storage
 *   @param[in]  pCancelFunction   Function used to cancel scanning during frequency check
 *
 *   @return 0 on success
 */
int dvb_frequencyScan( tunerFormat tuner, __u32 frequency, EIT_media_config_t *media, dvb_displayFunctionDef* pFunction, NIT_table_t *scan_network, int save_service_list, dvb_cancelFunctionDef* pCancelFunction);

/**  @ingroup dvb_instance
 *   @ingroup dvb_service
 *   @brief Function used to scan for EPG during playback
 *
 *   @param[in]  tuner             Tuner to use
 *   @param[in]  frequency         Frequency to scan
 */
void dvb_scanForEPG( tunerFormat tuner, unsigned long frequency );

/**  @ingroup dvb_instance
 *   @ingroup dvb_service
 *   @brief Function used to scan for PAT+PMT during playback
 *   @param[in]  tuner             Tuner to use
 *   @param[in]  frequency         Frequency to scan
 *   @param[out] out_list          Service list to store PAT+PMT data
 */
void dvb_scanForPSI( tunerFormat tuner, unsigned long frequency, list_element_t **out_list );

/**  @ingroup dvb_service
 *   @brief Function used to return the number of available DVB channels
 *
 *   @return Number of available DVB channels
 */
int dvb_getNumberOfServices(void);

/**  @ingroup dvb_service
 *   @brief Get channel at specified index
 *
 *   @param[in]  which     Channel index
 *
 *   @return NULL if index is out of range
 */
EIT_service_t* dvb_getService(int which);

/**  @ingroup dvb_service
 *   @brief Get channel name at specified index
 *
 *   @param[in]  which     Channel index
 *
 *   @return NULL if service is out of range, translated N/A if name is not specified
 */
char *dvb_getTempServiceName(int index);

/**  @ingroup dvb_service
 *   @brief Get index of specified channel
 *
 *   @param[in]  service   Channel to be used
 *
 *   @return Channel index starting from 0 or -1 if service not found
 */
int dvb_getServiceIndex( EIT_service_t* service);

/**  @ingroup dvb_service
 *   @brief Function used to return the name of a given DVB channel
 *
 *   @param[in]  service   Channel to be used
 *
 *   @return Channel name or NULL if not available
 */
char* dvb_getServiceName(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function used to get textual description of a given DVB channel
 *
 *   @param[in]  service   Channel to be used
 *   @param[out] buf       Channel description
 *
 *   @return 0 on success
 */
int dvb_getServiceDescription(EIT_service_t *service, char* buf);

/**  @ingroup dvb_service
 *   @brief Function used to return the service ID of a given DVB channel
 *
 *   @param[in]  service   Service ID to be returned
 *
 *   @return Service ID or -1 if not available
 */
int dvb_getServiceID(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function used to get the service frequency of a given DVB channel
 *
 *   @param[in]  service     Channel to be used
 *   @param[out] pFrequency  Channel frequency
 *
 *   @return 0 on success
 */
int dvb_getServiceFrequency(EIT_service_t *service, __u32* pFrequency);

/**  @ingroup dvb_service
 *   @brief Function used to return the scrambled state of a given DVB channel
 *
 *   @param[in]  service   Channel to be checked
 *
 *   @return 1 if scrambled
 */
int dvb_getScrambled(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function used to get the pseudo url of DVB channel
 *
 *   @param[in]  service   Channel to be used
 *   @param[out] URL       DVB channel's pseudo URL
 *
 *   @return 1 if scrambled
 */
int dvb_getServiceURL(EIT_service_t *service, char* URL);

/**  @ingroup dvb_service
 *   @brief Function used to return video and audio PIDs for the given DVB channel
 *
 *   @param[in]  service   Channel to be used
 *   @param[in]  audio     Audio track number
 *   @param[out] pVideo    Video PID for channel
 *   @param[out] pAudio    Audio PID for channel
 *   @param[out] pText     Teletext PID for channel
 *   @param[out] pPcr      Pcr PID for channel
 *
 *   @return 0 on success
 */
int dvb_getPIDs(EIT_service_t *service, int audio, int* pVideo, int* pAudio, int* pText, int* pPcr);

/**  @ingroup dvb_service
 *   @brief Function checks stream has specified payload type
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  p_type    Payload type to check
 *
 *   @return 1 if found
 *   @sa dvb_hasPayloadTypeNB()
 *   @sa dvb_hasMediaType()
 */
int dvb_hasPayloadType(EIT_service_t *service, payload_type p_type);

/**  @ingroup dvb_service
 *   @brief Function checks stream has specified media type (video or audio)
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  m_type    Media type to check
 *
 *   @return 1 if found
 *   @sa dvb_hasMedia()
 *   @sa dvb_hasMediaTypeNB()
 */
int dvb_hasMediaType(EIT_service_t *service, media_type m_type);

/**  @ingroup dvb_service
 *   @brief Function checks stream has any supported media
 *
 *   @param[in]  serivce   Channel to be used
 *
 *   @return 1 if found
 *   @sa dvb_hasMediaType()
 */
int dvb_hasMedia(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function checks stream has specified payload type (non-blocking)
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  p_type    Payload type to check
 *
 *   @return 1 if found
 *   @sa dvb_hasPayloadType()
 */
int dvb_hasPayloadTypeNB(EIT_service_t *service, payload_type p_type);

/**  @ingroup dvb_service
 *   @brief Function checks stream has specified media type (video or audio) (non-blocking)
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  m_type    Media type to check
 *
 *   @return 1 if found
 *   @sa dvb_hasMediaType()
 */
int dvb_hasMediaTypeNB(EIT_service_t *service, media_type m_type);

/**  @ingroup dvb_service
 *   @brief Function checks stream has any supported media (non-blocking)
 *
 *   @param[in]  serivce   Channel to be used
 *
 *   @return 1 if found
 *   @sa dvb_hasMedia()
 */
int dvb_hasMediaNB(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function used to get number of audio tracks of a given channel
 *
 *   @param[in]  serivce   Channel to be used
 *
 *   @return Audio track count or -1 on error
 */
int dvb_getAudioCountForService(EIT_service_t *service);

/**  @ingroup dvb_service
 *   @brief Function used to get audio PID of specified track of a given channel
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  audio     Audio track number
 *
 *   @return Audio track PID or -1 on error
 */
short unsigned int dvb_getAudioPidForService(EIT_service_t *service, int audio);

/**  @ingroup dvb_service
 *   @brief Function used to get audio type of specified track of a given channel
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  audio     Audio track number
 *
 *   @return Audio track type or -1 on error
 */
int dvb_getAudioTypeForService(EIT_service_t *service, int audio);

/**  @ingroup dvb_service
 *   @brief Function used to get name of event with specified ID from a given channel
 *
 *   @param[in]  serivce   Channel to be used
 *   @param[in]  event_id  Event ID
 *
 *   @return NULL if event not found
 */
char* dvb_getEventName( EIT_service_t* service, short int event_id );

/**  @ingroup dvb_service
 *   @brief Cleanup NIT structure
 */
void dvb_clearNIT(NIT_table_t *nit);

/**  @ingroup dvb_service
 *   @brief Function exports current channel list to specified file
 *
 *   @param[in]  filename  Channel file name
 */
void dvb_exportServiceList(char* filename);

/**  @ingroup dvb_service
 *   @brief Function imports current channel list from specified file
 *
 *   @param[in]  filename  Channel file name
 *
 *   @return 0 on success
 */
int dvb_readServicesFromDump(char* filename);

/**  @ingroup dvb_service
 *   @brief Function cleans current channel list
 *
 *   @param[in]  permanent Save changes to disk
 */
void dvb_clearServiceList(int permanent);

/** @defgroup dvb_pvr DVB PVR features
 *  @ingroup dvb
 *  @deprecated PVR functions moved to StbPvr project
 *  @{
 */

/** Function used to obtain the length of the given PVR stream
 *
 *   @param[in]  which     PVR stream being played
 *   @param[out] pPosition Pointer to variable to hold length
 */
void dvb_getPvrLength(int which, DvbFilePosition_t *pPosition);

/** Function used to obtain the current playback position of the given PVR stream
 *
 *   @param[in]  which     PVR stream being played
 *   @param[out] pPosition Pointer to variable to hold current position
 */
void dvb_getPvrPosition(int which, DvbFilePosition_t *pPosition);

/** Function used to return the bit rate for the given pvr playback
 *
 *   @param[in]  which     PVR playback session
 *
 *   @return Bit rate in bytes per second
 */
int dvb_getPvrRate(int which);

/** @} */

#endif /* ENABLE_DVB */

#endif /* __DVB_H      Do not add any thing below this line */
