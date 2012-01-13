
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "debug.h"
#include "sdp.h"
#include "rtp_func.h"
#include "smrtsp.h"

prtspConnection RTSPConnection = NULL;

int rtsp_init(sdp_desc *streamdesc, char *addr, unsigned int port, char *stream)
{
	int res;

	if (RTSPConnection != NULL)
	{
		smrtsp_destroy(RTSPConnection);
		RTSPConnection = NULL;
	}

	if ((res = smrtsp_easy_init(&RTSPConnection, addr, port, stream)) != 0)
	{
		eprintf("rtps_func: smrtsp_easy_init failed with code %d\n", res);
		return res;
	}

	memcpy(streamdesc, &RTSPConnection->session.sessionSDP, sizeof(sdp_desc));

	dprintf("%s: --- before play %s:%d type %d\n", __FUNCTION__, inet_ntoa(streamdesc->connection.address.IPv4), streamdesc->media[0].port, streamdesc->media[0].proto);

	return 0;
}

void rtsp_set_rtcp(struct rtp_session_t *rtp_session, unsigned long ip)
{
	if (RTSPConnection != NULL)
	{
		dprintf("%s: RTCP Server %s:%d, Local %s:%d\n", __FUNCTION__, inet_ntoa(*(struct in_addr*)&RTSPConnection->session.rtcp_source_address), RTSPConnection->session.rtcp_server_port, inet_ntoa(*(struct in_addr*)&ip), RTSPConnection->session.rtcp_client_port);
		if (RTSPConnection->session.rtcp_client_port > 0 && RTSPConnection->session.rtcp_source_address != INADDR_ANY && RTSPConnection->session.rtcp_source_address != INADDR_NONE)
		{
			if (rtp_set_rtcp(rtp_session, RTSPConnection->session.rtcp_source_address, RTSPConnection->session.rtcp_server_port, ip, RTSPConnection->session.rtcp_client_port) != 0)
			{
				eprintf("rtps_func: Warning: RTCP channel initialisation failed!\n");
			} else
			{
				eprintf("rtps_func: RTCP channel initialised\n");
			}
		}
	}
}

int rtsp_pause()
{
	if (RTSPConnection != NULL)
	{
		/*if (smrtsp_pause(RTSPConnection) != 0)
		{
			eprintf("rtps_func: WARNING: RTSP Pause command failed, breaking connection!\n");
			smrtsp_destroy(RTSPConnection);
			RTSPConnection = NULL;
			return -1;
		}
		return 0;*/
		smrtsp_pause(RTSPConnection);
		return 0;
	}
	return -1;
}

int rtsp_play(float scale)
{
	if (RTSPConnection != NULL)
	{
		return smrtsp_play_scale(RTSPConnection, scale);
	}
	return -1;
}

int rtsp_play_from_pos(float scale, float pos)
{
	if (RTSPConnection != NULL)
	{
		npt_range range;

		range.range_start = pos;
		range.range_end = -1.0;

		return smrtsp_play_range_scale(RTSPConnection, &range, scale);
	}
	return -1;
}

int rtsp_getLength()
{
	if (RTSPConnection != NULL && RTSPConnection->session.sessionServerState.nptEnd > 0.0f)
	{
		return RTSPConnection->session.sessionServerState.nptEnd-RTSPConnection->session.sessionServerState.nptStart;
	} else if (RTSPConnection != NULL && RTSPConnection->session.sessionSDP.range.range_end > 0.0f)
	{
		return RTSPConnection->session.sessionSDP.range.range_end-RTSPConnection->session.sessionServerState.nptStart;
	}
	return -1;
}

int rtsp_getTimes(double *nptStart, double *nptEnd, double *nptCurrent, double *serverScale)
{
	if (RTSPConnection != NULL)
	{
		*nptStart = RTSPConnection->session.sessionServerState.nptStart;
		if (RTSPConnection->session.sessionServerState.nptEnd <= 0.0f && RTSPConnection->session.sessionSDP.range.range_end > 0.0f)
		{
			*nptEnd = RTSPConnection->session.sessionSDP.range.range_end;
		} else
		{
			*nptEnd = RTSPConnection->session.sessionServerState.nptEnd;
		}
		*serverScale = RTSPConnection->session.sessionServerState.scale;

		if( RTSPConnection->compatibility == COMPAT_KASENNA )
		{
			smrtsp_get_position(RTSPConnection, nptCurrent);
		} else
		{
			*nptCurrent = RTSPConnection->session.sessionServerState.nptCurrent;
		}

		dprintf("%s: Times: %f ---- %f x %f >---- %f\n", __FUNCTION__, *nptStart, *nptCurrent, *serverScale, *nptEnd);

		return 0;
	}
	return -1;
}

int rtsp_deinit()
{

	if (RTSPConnection != NULL)
	{
		dprintf("%s: destroy RTSPConnection\n", __FUNCTION__);
		smrtsp_destroy(RTSPConnection);
		RTSPConnection = NULL;
	}

	return 0;
}

