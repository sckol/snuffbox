#if !defined(__RTP_COMMON_H)
#define __RTP_COMMON_H
	
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

#include "rtp_func.h"
#include "rtsp.h"

/*******************
* EXPORTED MACROS  *
********************/

#if (defined STB82)
#include <phStbDFBVideoProviderCommonTypes.h>
#else
#if (defined STB225)
#include <phStbDFBVideoProviderCommonElcTypes.h>
#include <phStbSbmSink.h>
#else
#define SET_PAUSE(f, w)
#define SET_NORMAL_SPEED(f, w)
#define SET_FAST_SPEED(f, w)
#define FLUSH_PROVIDER(f, w)
#define SET_AUDIO_PID(f, w, p)
#define SET_VIDEO_PID(f, w, p)
#endif
#endif

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef struct
{
	// file descriptors for frontend, demux-video, demux-audio
	int fdv;
	int fda;
	int fdp;
	int dvrfd;
#ifdef STB225
	phStbSbmSink_t sbmSink;
#endif

	sdp_desc selectedDesc;
	rtsp_stream_info stream_info;

	/* indicates if current stream is playing (not previewing) */
	int playingFlag;

	char pipeString[PATH_MAX];

	pthread_t thread;
	pthread_t collectThread;

	int collectFlag;

	int data_timeout;

	struct list_head audio_tracks;

	struct rtp_session_t *rtp_session;
} rtp_common_instance;


struct audio_track 
{
	struct list_head list;
	int pid;
};

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

void rtp_common_init_instance(rtp_common_instance *pInst);

void rtp_common_close(rtp_common_instance *pInst);

int rtp_common_open(/* IN */	char *demuxer, char *pipe, char *pipedev, char *dvrdev, int videopid, int audiopid, int pcrpid,
					/* OUT */	int *dvrfd, int *videofd, int *audiofd, int *pcrfd);

void rtp_common_get_pids(pStreamsPIDs streamPIDs, int *vFormat, int *vPID, int *aFormat, int *aPID, int *pPID, struct list_head *audio);

#ifdef STB225
int rtp_common_sink_open(rtp_common_instance *pInst);
int rtp_common_sink_prepare(rtp_common_instance *pInst);
#endif

int rtp_common_change_audio(rtp_common_instance *pInst, int aPID);

#endif //__RTP_COMMON_H
