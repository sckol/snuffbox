#if !defined(__RTP_FUNC_H)
#define __RTP_FUNC_H

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

#define RTP_MAX_STREAM_COUNT (256)
#define RTP_SDP_TTL          (7000)
/*
sdp_desc
{
	char source[128];
	char info[128];
	struct in_addr addr;
	short port;
	int transport_type;
	int is_mp2t;
};
*/
struct rtp_list
{
	int count;
	sdp_desc items[RTP_MAX_STREAM_COUNT];
};

typedef struct
{
	int ItemsCnt;
	int ProgramCnt;
	int TotalProgramCnt;
	struct StrmInfo
	{
		int ProgNum;
		int ProgID;
		int PCR_PID;
		int stream_type;
		int elementary_PID;
	} *pStream;
} StreamsPIDs, *pStreamsPIDs;

struct rtp_session_t;

#ifdef __cplusplus
extern "C" {
#endif
void rtp_change_eng(struct rtp_session_t *session, int eng);
void rtp_get_last_data_timestamp(struct rtp_session_t *session, struct timeval *ts);
int rtp_engine_supports_transport(int eng, int transport);
void rtp_flush_receive_buffers(struct rtp_session_t *session);
int rtp_start_output(struct rtp_session_t *session, int fd, int dmx);
pStreamsPIDs rtp_get_streams(struct rtp_session_t *session);
void rtp_clear_streams(struct rtp_session_t *session);
int rtp_start_receiver(struct rtp_session_t *session, sdp_desc *desc, int media_no, int delayFeeding, int verimatrix);
int rtp_stop_receiver(struct rtp_session_t *session);
int rtp_sdp_start_collecting(struct rtp_session_t *session);
int rtp_sdp_set_collecting_state(struct rtp_session_t *session, int startFlag);
int rtp_sdp_stop_collecting(struct rtp_session_t *session);
int rtp_get_found_streams(struct rtp_session_t *session, struct rtp_list *found_streams);
int rtp_session_init(struct rtp_session_t **psession);
void rtp_session_destroy(struct rtp_session_t *psession);
int rtp_set_rtcp(struct rtp_session_t *session, unsigned long server_address, int server_port, unsigned long client_address, int client_port);
#ifdef ENABLE_VERIMATRIX
int rtp_enable_verimatrix(struct rtp_session_t *session, char *inipath);
#endif
#ifdef ENABLE_SECUREMEDIA
int rtp_enable_securemedia(struct rtp_session_t *session);
int SmPlugin_Init(const char *args);
void SmPlugin_Finit(void);
#endif
#ifdef __cplusplus
}
#endif

#endif //__RTP_FUNC_H
