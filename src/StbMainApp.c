
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

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef STB225
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#endif

#include <directfb.h>
#include <pthread.h>
#include <wait.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/un.h>

#ifdef STB225
#include <phStbSbmSink.h>
#include "Stb225.h"
#endif
#ifdef STB82
#include <phStbSystemManager.h>
#endif

#include "rtp.h"
#include "rtsp.h"
#include "tools.h"
#include "rtp_func.h"
#include "list.h"
#include "debug.h"
#include "StbMainApp.h"
#include "app_info.h"
#include "sem.h"
#include "gfx.h"
#include "interface.h"
#include "l10n.h"
#include "sound.h"
#include "dvb.h"
#include "pvr.h"
#include "rtsp.h"
#include "rutube.h"
#include "off_air.h"
#include "output.h"
#include "voip.h"
#include "media.h"
#include "playlist.h"
#include "menu_app.h"
#include "watchdog.h"
#include "downloader.h"
#include "dlna.h"
#include "youtube.h"

#ifdef ENABLE_VIDIMAX
#include "vidimax.h" 
#endif

/***********************************************
 LOCAL MACROS                                  *
************************************************/

/** How many keypresses skip before starting repeating */
#define REPEAT_THRESHOLD (2)
/** Timeout in microseconds after which repeating threshold is activated */
#ifdef ENABLE_VIDIMAX
#define REPEAT_TIMEOUT (40000)
#else
#define REPEAT_TIMEOUT (400000)
#endif
/** Timeout in microseconds after which events are considered outdated and ignored */
#define OUTDATE_TIMEOUT  (50000)

#define ADD_HANDLER(key, command) case key: cmd = command; dprintf("%s: Got command: %s\n", __FUNCTION__, #command); break;
#define ADD_FP_HANDLER(key, command) case key: cmd = command; event->input.device_id = DID_FRONTPANEL; dprintf("%s: Got command (FP): %s\n", __FUNCTION__, #command); break;
#define ADD_FALLTHROUGH(key) case key:

/** MMIO application path */
#define INFO_MMIO_APP "/usr/lib/luddite/mmio"
/** MMIO address template */
#define INFO_ADDR_TEMPLATE " 0x"

/** Temp file for text processing */
#define INFO_TEMP_FILE "/tmp/info.txt"

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static int helper_confirmFirmwareUpdate(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

volatile int keepCommandLoopAlive;
volatile int keyThreadActive;

volatile int flushEventsFlag;

IDirectFBEventBuffer *appEventBuffer = NULL;
int term;
char startApp[PATH_MAX] = "";

interfaceCommand_t lastRecvCmd = interfaceCommandNone;
int captureInput = 0;

static int inStandbyActiveVideo;

#ifdef ENABLE_TEST_SERVER
	pthread_t server_thread;
#endif

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

int gAllowConsoleInput = 0;
int gAllowTextMenu = 0;



/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

int helperFileExists(const char* filename)
{
	int file;
#if (defined STB225)
	const char *filename_t = filename;
	const char *prefix = "file://";
	if(strncmp(filename, prefix, strlen(prefix))==0)
		filename_t = filename + strlen(prefix);
	file = open( filename_t, O_RDONLY);
//printf("%s[%d]: file=%d, filename_t=%s\n", __FILE__, __LINE__, file, filename_t);
#else
	file = open( filename, O_RDONLY);
#endif


	if ( file < 0 )
	{
		return 0;
	}

	close( file );

	return 1;
}


int helperReadLine(int file, char* buffer)
{
	if ( file )
	{
		int index = 0;
		while ( 1 )
		{
			char c;

			if ( read(file, &c, 1) < 1 )
			{
				if ( index > 0 )
				{
					buffer[index] = '\0';
					return 0;
				}
				return -1;
			}

			if ( c == '\n' )
			{
				buffer[index] = '\0';
				return 0;
			} else if ( c == '\r' )
			{
				continue;
			} else
			{
				buffer[index] = c;
				index++;
			}
		}
	}
	return -1;
}

int helperParseMmio(int addr)
{
	int file, num;
	char buf[BUFFER_SIZE];
	char *pos;

	sprintf(buf, INFO_MMIO_APP " 0x%X > " INFO_TEMP_FILE, addr);
	system(buf);

	num = 0;

	file = open(INFO_TEMP_FILE, O_RDONLY);
	if (file > 0)
	{
		if (helperReadLine(file, buf) == 0 && strlen(buf) > 0)
		{
			pos = strstr(buf, INFO_ADDR_TEMPLATE);
			if (pos != NULL)
			{
				pos += sizeof(INFO_ADDR_TEMPLATE)-1;
				num = strtol(pos, NULL, 16);
			}
		}
		close(file);
	}

	return num;
}

int helperCheckDirectoryExsists(const char *path)
{
	DIR *d;

	d = opendir(path);

	if (d == NULL)
	{
		return 0;
	}

	closedir(d);

	return 1;
}

int helperParseLine(const char *path, const char *cmd, const char *pattern, char *out, char stopChar )
{
	int file, res;
	char buf[BUFFER_SIZE];
	char *pos, *end;

	if (cmd != NULL)
	{
		/* empty output file */
		sprintf(buf, "echo -n > %s", path);
		system(buf);

		sprintf(buf, "%s > %s", cmd, path);
		system(buf);
	}

	res = 0;
	file = open(path, O_RDONLY);
	if (file > 0)
	{
		if (helperReadLine(file, buf) == 0 && strlen(buf) > 0)
		{
			if (pattern != NULL)
			{
				pos = strstr(buf, pattern);
			} else
			{
				pos = buf;
			}
			if (pos != NULL)
			{
				if (out == NULL)
				{
					res = 1;
				} else
				{
					if (pattern != NULL)
					{
						pos += strlen(pattern);
					}
					while(*pos == ' ')
					{
						pos++;
					}
					if ((end = strchr(pos, stopChar)) != NULL || (end = strchr(pos, '\r')) != NULL || (end = strchr(pos, '\n')) != NULL)
					{
						*end = 0;
					}
					strcpy(out, pos);
					res = 1;
				}
			}
		}
		close(file);
	}

	return res;
}


void helperCreatePipe(void)
{
#ifdef STB82
	//char pipeString[256];

	//sprintf(pipeString, "modprobe phStbDspPipe debug=0 wrap_allowed=1 buffer0_size=$((128*1024)) buffer_size=$((6400)) nr_devs=3");
	//system(pipeString);

	//	sprintf(pipeString, "ln -s /dev/dsppipe0 %s", PVR_PIPE_NAME);
	//	system(pipeString);
#endif
}

void helperRemovePipe(void)
{
#ifdef STB82
	//char pipeString[256];

	//	sprintf(pipeString, "rm -f %s", PVR_PIPE_NAME);
	//	system(pipeString);

	//sprintf(pipeString, "rmmod phStbDspPipe");
	//system(pipeString);
#endif
}



/* Handle any registered signal by requesting a graceful exit */
void signal_handler(int signal)
{
	static int caught = 0;

	//dprintf("%s: in\n", __FUNCTION__);

	if ( caught == 0 )
	{
		caught = 1;
		eprintf("App: Stopping Command Loop ... (signal %d)\n", signal);
		keepCommandLoopAlive = 0;
	}
}

static void stub_signal_handler(int signal)
{
	eprintf("App: %s Error (signal %d), ignore!\n", signal == SIGBUS ? "BUS" : "PIPE", signal);
}

static void usr1_signal_handler(int sig)
{
	eprintf("App: Got USR1 (signal %d), updating USB!\n", signal);
	media_storagesChanged();
#ifdef ENABLE_PVR
	pvr_storagesChanged();
#endif
}

static void hup_signal_handler(int sig)
{
	eprintf("App: Got HUP (signal %d), reloading config!\n", signal);
	loadAppSettings();
	loadVoipSettings();
}

static void parse_commandLine(int argc, char *argv[])
{
	int i;
	static char infoFiles[1][1024];

	for ( i=0;i<argc;i++ )
	{
		if ( strcmp(argv[i], "-stream_info_url") == 0 )
		{
			strcpy(appControlInfo.rtspInfo[0].streamInfoIP, argv[i+1]);
			i++;
		} else if ( strcmp(argv[i], "-stream_url") == 0 )
		{
			strcpy(appControlInfo.rtspInfo[0].streamIP, argv[i+1]);
			i++;
		} else if ( strcmp(argv[i], "-rtsp_port") == 0 )
		{
			appControlInfo.rtspInfo[0].RTSPPort = atoi(argv[i+1]);
			i++;
		} else if ( strcmp(argv[i], "-stream_info_file") == 0 )
		{
			strcpy(infoFiles[0], argv[i+1]);
			appControlInfo.rtspInfo[0].streamInfoFiles = (char**)infoFiles;
			i++;
		} else if ( strcmp(argv[i], "-stream_file") == 0 )
		{
			strcpy(appControlInfo.rtspInfo[0].streamFile, argv[i+1]);
			i++;
		} else if ( !strcmp(argv[i], "-i2s1") )
		{
			appControlInfo.soundInfo.rcaOutput = 1;
		} else if ( !strcmp(argv[i], "-console") )
		{
			gAllowConsoleInput = 1;
		} else if ( !strcmp(argv[i], "-text_menu") )
		{
			gAllowTextMenu = 1;
		}
	}
}

interfaceCommand_t parseEvent(DFBEvent *event)
{
	interfaceCommand_t cmd = interfaceCommandNone;
	if( event->input.device_id != DID_KEYBOARD )
#ifdef STB225
		switch ( event->input.key_id )
		{
			ADD_FP_HANDLER(DIKI_KP_4, interfaceCommandUp)
			ADD_FP_HANDLER(DIKI_KP_5, interfaceCommandDown)
			ADD_FP_HANDLER(DIKI_KP_0, interfaceCommandLeft)
			ADD_FP_HANDLER(DIKI_KP_6, interfaceCommandRight)
			ADD_FP_HANDLER(DIKI_KP_3, interfaceCommandEnter)
			ADD_FP_HANDLER(DIKI_KP_1, interfaceCommandToggleMenu)
			ADD_FP_HANDLER(DIKI_KP_9, interfaceCommandBack)
#else
		switch ( event->input.button )
		{
			ADD_FP_HANDLER(3, interfaceCommandUp)
			ADD_FP_HANDLER(4, interfaceCommandDown)
			ADD_FP_HANDLER(1, interfaceCommandLeft)
			ADD_FP_HANDLER(2, interfaceCommandRight)
			ADD_FP_HANDLER(5, interfaceCommandEnter)
#endif
		default:;
		}

	if (cmd == interfaceCommandNone)
	{
		//dprintf("%s: key symbol %d '%c'\n", __FUNCTION__, event->input.key_symbol, event->input.key_symbol);
		switch ( event->input.key_symbol )
		{
			//ADD_HANDLER(DIKS_CURSOR_UP, interfaceCommandUp)
			//ADD_HANDLER(DIKS_CURSOR_DOWN, interfaceCommandDown)
			//ADD_HANDLER(DIKS_CURSOR_LEFT, interfaceCommandLeft)
			//ADD_HANDLER(DIKS_CURSOR_RIGHT, interfaceCommandRight)
			//ADD_FALLTHROUGH(DIKS_ENTER)
			//ADD_HANDLER(DIKS_OK, interfaceCommandOk)
			//ADD_FALLTHROUGH(DIKS_SPACE)
			ADD_HANDLER(DIKS_MENU, interfaceCommandToggleMenu)
			/*ADD_HANDLER(DIKS_0, interfaceCommand0)
			ADD_HANDLER(DIKS_1, interfaceCommand1)
			ADD_HANDLER(DIKS_2, interfaceCommand2)
			ADD_HANDLER(DIKS_3, interfaceCommand3)
			ADD_HANDLER(DIKS_4, interfaceCommand4)
			ADD_HANDLER(DIKS_5, interfaceCommand5)
			ADD_HANDLER(DIKS_6, interfaceCommand6)
			ADD_HANDLER(DIKS_7, interfaceCommand7)
			ADD_HANDLER(DIKS_8, interfaceCommand8)
			ADD_HANDLER(DIKS_9, interfaceCommand9)*/
			//ADD_HANDLER(DIKS_EXIT, interfaceCommandExit)
			ADD_HANDLER(DIKS_EXIT, interfaceCommandExit)
			//ADD_FALLTHROUGH(DIKS_BACKSPACE)
			ADD_HANDLER(DIKS_BACK, interfaceCommandBack)

			//ADD_FALLTHROUGH(DIKS_HOME)
			//ADD_HANDLER(DIKS_ESCAPE, interfaceCommandMainMenu)
			//ADD_FALLTHROUGH(DIKS_PLUS_SIGN)
			ADD_HANDLER(DIKS_VOLUME_UP, interfaceCommandVolumeUp)
			//ADD_FALLTHROUGH(DIKS_MINUS_SIGN)
			ADD_HANDLER(DIKS_VOLUME_DOWN, interfaceCommandVolumeDown)
			//ADD_FALLTHROUGH(DIKS_ASTERISK)
			ADD_HANDLER(DIKS_MUTE, interfaceCommandVolumeMute)
			//ADD_FALLTHROUGH('z')
			ADD_HANDLER(DIKS_PREVIOUS, interfaceCommandPrevious)
			//ADD_FALLTHROUGH('x')
			ADD_HANDLER(DIKS_REWIND, interfaceCommandRewind)
			//ADD_FALLTHROUGH('c')
			ADD_HANDLER(DIKS_STOP, interfaceCommandStop)
#if !(defined STB225) // DIKS_PAUSE on STB225 mapped to wrong button
			//ADD_FALLTHROUGH('v')
			ADD_HANDLER(DIKS_PAUSE, interfaceCommandPause)
#endif
			//ADD_FALLTHROUGH('b')
			ADD_HANDLER(DIKS_PLAY, interfaceCommandPlay)
			//ADD_FALLTHROUGH('n')
			ADD_HANDLER(DIKS_FASTFORWARD, interfaceCommandFastForward)
			//ADD_FALLTHROUGH('m')
			ADD_HANDLER(DIKS_NEXT, interfaceCommandNext)
			//ADD_FALLTHROUGH(DIKS_TAB)
			ADD_HANDLER(DIKS_EPG, interfaceCommandChangeMenuStyle)
			//ADD_FALLTHROUGH('i')
			ADD_FALLTHROUGH(DIKS_CUSTOM1) // 'i' on stb remote
			ADD_HANDLER(DIKS_INFO, interfaceCommandServices)
			//ADD_HANDLER(DIKS_TV, interfaceCommandAudioTracks)
			ADD_HANDLER(DIKS_RED, interfaceCommandRed)
			ADD_HANDLER(DIKS_GREEN, interfaceCommandGreen)
			ADD_HANDLER(DIKS_YELLOW, interfaceCommandYellow)
			ADD_HANDLER(DIKS_BLUE, interfaceCommandBlue)
		default:
			//cmd = interfaceCommandNone;
			cmd = event->input.key_symbol;
		}
	}

	lastRecvCmd = cmd;

#ifdef ENABLE_TEST_MODE
	if (captureInput)
	{
		return interfaceCommandCount;
	}
#endif

	return cmd;
}

int checkPowerOff(DFBEvent event)
{
	int timediff;
	static struct timeval firstPress = {0, 0},
						  lastChange = {0, 0};

	struct timeval lastPress = {0, 0},
				   currentPress = {0, 0};

	int repeat = 0;

	gettimeofday(&currentPress, NULL);

	timediff = (currentPress.tv_sec-event.input.timestamp.tv_sec)*1000000+(currentPress.tv_usec-event.input.timestamp.tv_usec);

	if (timediff > OUTDATE_TIMEOUT)
	{
		dprintf("%s: ignore event, age %d\n", __FUNCTION__, timediff);
		return 0;
	}

	if (firstPress.tv_sec == 0 || lastPress.tv_sec == 0)
	{
		timediff = 0;
	} else
	{
		timediff = (currentPress.tv_sec-lastPress.tv_sec)*1000000+(currentPress.tv_usec-lastPress.tv_usec);
	}

	if (timediff == 0 || timediff > REPEAT_TIMEOUT)
	{
		//dprintf("%s: reset\n, __FUNCTION__");
		memcpy(&firstPress, &currentPress, sizeof(struct timeval));
	} else
	{
		//dprintf("%s: repeat detected\n", __FUNCTION__);
		repeat = 1;
	}

	memcpy(&lastPress, &currentPress, sizeof(struct timeval));

	//dprintf("%s: check power\n", __FUNCTION__);

	if ( event.input.key_symbol == DIKS_POWER ) // Power/Standby button. Go to standby.
	{
		//dprintf("%s: got DIKS_POWER\n", __FUNCTION__);
		if (repeat && currentPress.tv_sec-firstPress.tv_sec >= 3)
		{
			//dprintf("%s: repeat 3 sec - halt\n", __FUNCTION__);
			/* Standby button has been held for 3 seconds. Power off. */
			/*interface_showMessageBox(_T("POWER_OFF"), thumbnail_warning, 0);
			system("halt");*/
		} else if (!repeat && currentPress.tv_sec - lastChange.tv_sec >= 3)
		{
			interfaceCommandEvent_t cmd;

			//dprintf("%s: switch standby\n", __FUNCTION__);
			/* Standby button was pressed once. Switch standby mode.  */
			if (appControlInfo.inStandby == 0)
			{
				//dprintf("%s: go to standby\n", __FUNCTION__);
				appControlInfo.inStandby = 1;

#if (defined ENABLE_PVR && defined ENABLE_DVB)
				if( pvr_isPlayingDVB(screenMain) )
				{
					offair_stopVideo(screenMain, 1);
				}
#endif

				inStandbyActiveVideo = gfx_videoProviderIsActive(screenMain);

				if (inStandbyActiveVideo)
				{
					memset(&cmd, 0, sizeof(interfaceCommandEvent_t));
					cmd.source = DID_STANDBY;
					cmd.command = interfaceCommandStop;
					interface_processCommand(&cmd);
				}

				interface_displayMenu(1);
				system("standbyon");

#ifdef STB82
/*
				system("/usr/lib/luddite/mmio 0x047730 0x0"); //GPIO_CLK_Q0_CTL control
				system("/usr/lib/luddite/mmio 0x047734 0x0"); //GPIO_CLK_Q1_CTL control
				system("/usr/lib/luddite/mmio 0x047738 0x0"); //GPIO_CLK_Q2_CTL control
				system("/usr/lib/luddite/mmio 0x04773C 0x0"); //GPIO_CLK_Q3_CTL control
				system("/usr/lib/luddite/mmio 0x047740 0x0"); //GPIO_CLK_Q4_CTL control
				system("/usr/lib/luddite/mmio 0x047744 0x0"); //GPIO_CLK_Q5_CTL control
				system("/usr/lib/luddite/mmio 0x047A00 0x0"); //CLK_QVCP1_OUT_CTL control(+)
				system("/usr/lib/luddite/mmio 0x047A08 0x0"); //CLK_QVCP2_OUT_CTL control
				system("/usr/lib/luddite/mmio 0x047A04 0x0"); //CLK_QVCP1_PIX_CTL control
				system("/usr/lib/luddite/mmio 0x047A0C 0x0"); //CLK_QVCP2_PIX_CTL control
				system("/usr/lib/luddite/mmio 0x047500 0x0"); //clk_mbs control
				system("/usr/lib/luddite/mmio 0x047750 0x0"); //CLK_MBS2_CTL control
				system("/usr/lib/luddite/mmio 0x047800 0x0"); //CLK_VMSP1_CTL control
				system("/usr/lib/luddite/mmio 0x047804 0x0"); //CLK_VMSP2_CTL control
				system("/usr/lib/luddite/mmio 0x047728 0x0"); //CLK_SMART1_CTL control
				system("/usr/lib/luddite/mmio 0x04772C 0x0"); //CLK_SMART2_CTL control
				system("/usr/lib/luddite/mmio 0x04720C 0x38");//MIPS DCS bus clock control to 27Mhz
				system("/usr/lib/luddite/mmio 0x047210 0x38");//MIPS DTL bus clock control to 27Mhz
				system("/usr/lib/luddite/mmio 0x047214 0x38");//TriMedia DCS bus clock control
				system("/usr/lib/luddite/mmio 0x047218 0x38");//TriMedia DTL bus clock control
				system("/usr/lib/luddite/mmio 0x047B00 0x0"); //CLK_SPDO_CTL control
				system("/usr/lib/luddite/mmio 0x047B04 0x0"); //CLK_AI1_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B08 0x0"); //CLK_AO1_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B0C 0x0"); //CLK_AI2_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B10 0x0"); //CLK_AO2_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B18 0x0"); //CLK_AI1_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B1C 0x0"); //CLK_AO1_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B20 0x0"); //CLK_AI2_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B24 0x0"); //CLK_AO2_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B28 0x0"); //CLK_SPDI2_CTL control
				system("/usr/lib/luddite/mmio 0x047B2C 0x0"); //CLK_SPDI_CTL control
				system("/usr/lib/luddite/mmio 0x047B30 0x0"); //CLK_TSTAMP_CTL control
				system("/usr/lib/luddite/mmio 0x047B34 0x0"); //CLK_TSDMA_CTL control
				system("/usr/lib/luddite/mmio 0x047B3C 0x0"); //CLK_DVDD_CTL control
				system("/usr/lib/luddite/mmio 0x047B44 0x0"); //CLK_VPK_CTL control

				// ---------------------------------- powerdown ------------------------
				system("/usr/lib/luddite/mmio 0x063FF4 0x80000000");
				system("/usr/lib/luddite/mmio 0x04DFF4 0x80000000");
				system("/usr/lib/luddite/mmio 0x17FFF4 0x80000000");//CTL12 IN Power Down
				system("/usr/lib/luddite/mmio 0x040FF4 0x80000000");//SCIF_POWERDOWN IN Power Down
				//system("/usr/lib/luddite/mmio 0x104FF4 0x80000000");//GPIO
				system("/usr/lib/luddite/mmio 0x106FF4 0x80000000");//VIP1
				system("/usr/lib/luddite/mmio 0x10DFF4 0x80000000");//QNTR
				system("/usr/lib/luddite/mmio 0x173FF4 0x80000000");//VPK
				system("/usr/lib/luddite/mmio 0x116FF4 0x80000000");//TSDMA
				system("/usr/lib/luddite/mmio 0x114FF4 0x80000000");//DVI
				system("/usr/lib/luddite/mmio 0x105FF4 0x80000000");//Mpeg2
				system("/usr/lib/luddite/mmio 0x108FF4 0x80000000");//VLD
				system("/usr/lib/luddite/mmio 0x4AFF4 0x80000000");//UART
				system("/usr/lib/luddite/mmio 0x4BFF4 0x80000000");//UART2
				system("/usr/lib/luddite/mmio 0x10CFF4 0x80000000 "); //MBS1- control


				phStbSystemManager_Init();
				phStbSystemManager_SetPowerMode(1);
*/
#endif

				memcpy(&lastChange, &currentPress, sizeof(struct timeval));

				return 1;

			} else
			{
#ifdef STB82
/*				system("stmclient 4");//enable leds

				phStbSystemManager_SetPowerMode(0);

				// ---------------------------------- powerdown - up ------------------------
				system("/usr/lib/luddite/mmio 0x063FF4 0x0");
				system("/usr/lib/luddite/mmio 0x04DFF4 0x0");
				system("/usr/lib/luddite/mmio 0x17FFF4 0x0");//CTL12 IN Power Down
				system("/usr/lib/luddite/mmio 0x040FF4 0x0");//SCIF_POWERDOWN IN Power Down
				//system("/usr/lib/luddite/mmio 0x104FF4 0x0");//GPIO
				system("/usr/lib/luddite/mmio 0x106FF4 0x0");//VIP1
				system("/usr/lib/luddite/mmio 0x10DFF4 0x0");//QNTR
				system("/usr/lib/luddite/mmio 0x173FF4 0x0");//VPK
				system("/usr/lib/luddite/mmio 0x116FF4 0x0");//TSDMA
				system("/usr/lib/luddite/mmio 0x114FF4 0x0");//DVI
				system("/usr/lib/luddite/mmio 0x105FF4 0x0");//Mpeg2
				system("/usr/lib/luddite/mmio 0x108FF4 0x0");//VLD
				system("/usr/lib/luddite/mmio 0x4AFF4 0x0");//UART
				system("/usr/lib/luddite/mmio 0x4BFF4 0x0");//UART2
				system("/usr/lib/luddite/mmio 0x10CFF4 0x0"); //MBS1- control


				system("/usr/lib/luddite/mmio 0x04720C 0x2");//MIPS DCS bus clock control to 27Mhz
				system("/usr/lib/luddite/mmio 0x047210 0x2");//MIPS DTL bus clock control to 27Mhz
				system("/usr/lib/luddite/mmio 0x047214 0x4");//TriMedia DCS bus clock control
				system("/usr/lib/luddite/mmio 0x047218 0x4");//TriMedia DTL bus clock control
				system("/usr/lib/luddite/mmio 0x047500 0x3"); //clk_mbs control
				system("/usr/lib/luddite/mmio 0x047750 0x3"); //CLK_MBS2_CTL control
				system("/usr/lib/luddite/mmio 0x047A00 0x3"); //CLK_QVCP1_OUT_CTL control
				system("/usr/lib/luddite/mmio 0x047A08 0x3"); //CLK_QVCP2_OUT_CTL control
				system("/usr/lib/luddite/mmio 0x04721C 0x1"); //Tunnel clock control
				system("/usr/lib/luddite/mmio 0x047400 0x71"); //clk_vmpg control
				system("/usr/lib/luddite/mmio 0x047404 0x71"); //clk_vld control
				system("/usr/lib/luddite/mmio 0x047800 0x23"); //CLK_VMSP1_CTL control
				system("/usr/lib/luddite/mmio 0x047804 0x23"); //CLK_VMSP2_CTL control
				system("/usr/lib/luddite/mmio 0x047730 0x1"); //GPIO_CLK_Q0_CTL control
				system("/usr/lib/luddite/mmio 0x047734 0x1"); //GPIO_CLK_Q1_CTL control
				system("/usr/lib/luddite/mmio 0x047738 0x1"); //GPIO_CLK_Q2_CTL control
				system("/usr/lib/luddite/mmio 0x04773C 0x1"); //GPIO_CLK_Q3_CTL control
				system("/usr/lib/luddite/mmio 0x047740 0x1"); //GPIO_CLK_Q4_CTL control
				system("/usr/lib/luddite/mmio 0x047744 0x1"); //GPIO_CLK_Q5_CTL control
				system("/usr/lib/luddite/mmio 0x047728 0x1"); //CLK_SMART1_CTL control
				system("/usr/lib/luddite/mmio 0x04772C 0x1"); //CLK_SMART2_CTL control
				system("/usr/lib/luddite/mmio 0x047820 0x3"); //CLK_TS_S11_CTL control
				system("/usr/lib/luddite/mmio 0x047824 0x1"); //CLK_TS_S12_CTL control
				system("/usr/lib/luddite/mmio 0x047828 0x3"); //CLK_TS_S21_CTL control
				system("/usr/lib/luddite/mmio 0x04782c 0x1"); //CLK_TS_S22_CTL control
				system("/usr/lib/luddite/mmio 0x047830 0x3"); //CLK_TS_S31_CTL control
				system("/usr/lib/luddite/mmio 0x047834 0x1"); //CLK_TS_S32_CTL control
				system("/usr/lib/luddite/mmio 0x04783C 0x1"); //TSOUT_CLK0_OUT_CTL control
				system("/usr/lib/luddite/mmio 0x047B00 0x3"); //CLK_SPDO_CTL control
				system("/usr/lib/luddite/mmio 0x047B04 0x1"); //CLK_AI1_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B08 0x1"); //CLK_AO1_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B0C 0x1"); //CLK_AI2_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B10 0x3"); //CLK_AO2_OSCLK_CTL control
				system("/usr/lib/luddite/mmio 0x047B18 0x1"); //CLK_AI1_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B1C 0x1"); //CLK_AO1_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B20 0x1"); //CLK_AI2_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B24 0x3"); //CLK_AO2_SCK_O_CTL control
				system("/usr/lib/luddite/mmio 0x047B28 0x1"); //CLK_SPDI2_CTL control
				system("/usr/lib/luddite/mmio 0x047B2C 0x1"); //CLK_SPDI_CTL control
				system("/usr/lib/luddite/mmio 0x047B30 0x3"); //CLK_TSTAMP_CTL control
				system("/usr/lib/luddite/mmio 0x047B34 0x1"); //CLK_TSDMA_CTL control
				system("/usr/lib/luddite/mmio 0x047B3C 0x1"); //CLK_DVDD_CTL control
				system("/usr/lib/luddite/mmio 0x047B44 0x71"); //CLK_VPK_CTL control
				system("/usr/lib/luddite/mmio 0x047A04 0xB"); //CLK_QVCP1_PIX_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047A0C 0xB"); //CLK_QVCP2_PIX_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047A18 0x3B"); //CLK_QVCP1_PROC_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047A1C 0x39"); //CLK_QVCP2_PROC_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x04770C 0x3"); //CLK48_CTL control
				system("/usr/lib/luddite/mmio 0x047710 0x3"); //CLK12_CTL control
				system("/usr/lib/luddite/mmio 0x047504 0x73"); //CLK_QTNR_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047600 0x1"); //CLK_VIP1_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047704 0x71"); //CLK_D2D_CTL control - FIX
				system("/usr/lib/luddite/mmio 0x047840 0x1"); //TSOUT_SERIAL_CLK0_CTL control
*/
#endif

				interface_displayMenu(1);
				system("standbyoff");

				if (inStandbyActiveVideo)
				{
					memset(&cmd, 0, sizeof(interfaceCommandEvent_t));
					cmd.command = interfaceCommandPlay;
					interface_processCommand(&cmd);
				}

				//dprintf("%s: return from standby\n", __FUNCTION__);
				appControlInfo.inStandby = 0;

				memcpy(&lastChange, &currentPress, sizeof(struct timeval));

				if( helperCheckUpdates() )
				{
					interface_showConfirmationBox(_T("CONFIRM_FIRMWARE_UPDATE"), thumbnail_question, helper_confirmFirmwareUpdate, NULL);
				}

				return 2;
			}
		}
	} else if ( event.input.button == 9 ) // PSU button, just do power off
	{
		system("halt");
	}

	return 0;
}

void helperFlushEvents()
{
	flushEventsFlag = 1;
}

interfaceCommand_t helperGetEvent(int flush)
{
	IDirectFBEventBuffer *eventBuffer = appEventBuffer;
	DFBEvent event;

	if ( eventBuffer->HasEvent(eventBuffer) == DFB_OK )
	{
		eventBuffer->GetEvent(eventBuffer, &event);
		if (flush)
		{
			eventBuffer->Reset(eventBuffer);
		}

		if (checkPowerOff(event))
		{
			eventBuffer->Reset(eventBuffer);
			return interfaceCommandNone;
		}

		if ( event.clazz == DFEC_INPUT && (event.input.type == DIET_KEYPRESS || event.input.type == DIET_BUTTONPRESS) )
		{
			return parseEvent(&event);
		}

		return interfaceCommandCount;
	}

	return interfaceCommandNone;
}

#ifdef ENABLE_TEST_SERVER
#define SERVER_SOCKET "/tmp/app_server.sock"
#define CHANNEL_INFO_SET(screen, channel) ((screen << 16) | (channel))
void *testServerThread(void *pArg)
{
	int sock = -1, s, res, pos;
	unsigned int salen;
	char ibuf[2048], obuf[2048], *ptr, *ptr1;
#ifdef TEST_SERVER_INET
	struct sockaddr_in sa;
#else
	struct sockaddr_un sa;
#endif

	eprintf("App: Start test server receiver\n");

#ifdef TEST_SERVER_INET
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sock < 0)
	{
		eprintf("App: Failed to create socket!\n");
		return NULL;
	}

	res = 1;
	if ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&res, sizeof(res)) != 0 )
	{
		eprintf("App: Failed to set socket options!\n");
		close(sock);
		return NULL;
	}

	memset(&sa, 0, sizeof(sa));

	sa.sin_addr.s_addr = INADDR_ANY;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(12304);

	res = bind(sock, (struct sockaddr*)&sa, sizeof(sa));

	if (res < 0)
	{
		eprintf("App: Failed bind socket!\n");
		close(sock);
		return NULL;
	}

#else
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
	{
		eprintf("App: Failed to create socket");
		return NULL;
	}

	sa.sun_family = AF_UNIX;
	strcpy(sa.sun_path, SERVER_SOCKET);
	unlink(sa.sun_path);
	salen = strlen(sa.sun_path) + sizeof(sa.sun_family);
	if (bind(sock, (struct sockaddr *)&sa, salen) == -1)
	{
		eprintf("App: Failed to bind Unix socket");
		close(sock);
		return NULL;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
	{
		eprintf("App: Failed to set non-blocking mode");
		close(sock);
		return NULL;
	}
#endif

	if (listen(sock, 5) == -1)
	{
		eprintf("App: Failed to listen socket");
		close(sock);
		return NULL;
	}

	eprintf("App: Wait for server connection\n");

	while(keepCommandLoopAlive)
	{
		fd_set reads;
		struct timeval tv;

		tv.tv_sec = 0;
		tv.tv_usec = 100000;

		FD_ZERO(&reads);
		FD_SET(sock, &reads);

		if (select(sock+1, &reads, NULL, NULL, &tv) < 1)
		{
			continue;
		}

		salen = sizeof(sa);
		s = accept(sock, (struct sockaddr*)&sa, &salen);

		eprintf("App: New client connection!\n");

#ifdef TEST_SERVER_INET
		strcpy(obuf, "Elecard STB820 App Server\r\n");
		write(s, obuf, strlen(obuf));
#endif

		pos = 0;

		while(keepCommandLoopAlive)
		{
			tv.tv_sec = 0;
			tv.tv_usec = 100000;

			FD_ZERO(&reads);
			FD_SET(s, &reads);

			if (select(s+1, &reads, NULL, NULL, &tv) < 1)
			{
				continue;
			}

			res = read(s, &ibuf[pos], sizeof(ibuf)-pos-1);

			dprintf("%s: read %d bytes\n", __FUNCTION__, res);

			if (!res)
			{
				break;
			}

			pos += res;
			ibuf[pos] = 0;

			if ((ptr = strchr(ibuf, '\n')) != NULL)
			{
				pos = 0;

				obuf[0] = 0;

				ptr1 = strchr(ibuf, '\r');

				if (ptr1 != NULL && ptr1 < ptr)
				{
					ptr = ptr1;
				}

				*ptr = 0;

				if (strcmp(ibuf, "sysid") == 0)
				{
					if (!helperParseLine(INFO_TEMP_FILE, "stmclient 5", NULL, obuf, '\n'))
					{
						strcpy(obuf, "ERROR: Cannot get System ID");
					}
					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "serial") == 0)
				{
					systemId_t sysid;
					systemSerial_t serial;

					if (helperParseLine(INFO_TEMP_FILE, "cat /dev/sysid", "SERNO: ", obuf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
					{
						serial.SerialFull = strtoul(obuf, NULL, 16);
					} else {
						serial.SerialFull = 0;
					}

					if (helperParseLine(INFO_TEMP_FILE, NULL, "SYSID: ", obuf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
					{
						sysid.IDFull = strtoul(obuf, NULL, 16);
					} else {
						sysid.IDFull = 0;
					}

					get_composite_serial(sysid, serial, obuf);

					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "stmfw") == 0)
				{
					unsigned long stmfw;

					if (helperParseLine(INFO_TEMP_FILE, "cat /dev/sysid", "VER: ", obuf, ',')) // SYSID: 04044020, SERNO: 00000039, VER: 0107
					{
						stmfw = strtoul(obuf, NULL, 16);
					} else {
						stmfw = 0;
					}

					sprintf(obuf, "%lu.%lu", (stmfw >> 8)&0xFF, (stmfw)&0xFF);
					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "mac1") == 0)
				{
					if (!helperParseLine(INFO_TEMP_FILE, "stmclient 7", NULL, obuf, '\n'))
					{
						strcpy(obuf, "ERROR: Cannot get MAC 1");
					} else
					{
						int a = 0, b = 0;
						while(obuf[a] != 0)
						{
							if (obuf[a] != ':')
							{
								obuf[b] = obuf[a];
								b++;
							}
							a++;
						}
						obuf[b] = 0;
					}
					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "mac2") == 0)
				{
					if (!helperParseLine(INFO_TEMP_FILE, "stmclient 8", NULL, obuf, '\n'))
					{
						strcpy(obuf, "ERROR: Cannot get MAC 2");
					} else
					{
						int a = 0, b = 0;
						while(obuf[a] != 0)
						{
							if (obuf[a] != ':')
							{
								obuf[b] = obuf[a];
								b++;
							}
							a++;
						}
						obuf[b] = 0;
					}
					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "fwversion") == 0)
				{
					strcpy(obuf, RELEASE_TYPE);
					strcat(obuf, "\r\n");
				} else if (strcmp(ibuf, "getinput") == 0)
				{
					lastRecvCmd = interfaceCommandNone;

					captureInput = 1;

					while (lastRecvCmd != interfaceCommandBack)
					{
						if (lastRecvCmd != interfaceCommandNone)
						{
							switch (lastRecvCmd)
							{
								case interfaceCommandUp:
									ptr = "UP";
									break;
								case interfaceCommandDown:
									ptr = "DOWN";
									break;
								case interfaceCommandLeft:
									ptr = "LEFT";
									break;
								case interfaceCommandRight:
									ptr = "RIGHT";
									break;
								case interfaceCommandEnter:
									ptr = "OK";
									break;
								case interfaceCommandChannelDown:
									ptr = "CH DOWN";
									break;
								case interfaceCommandChannelUp:
									ptr = "CH UP";
									break;
								case interfaceCommandVolumeDown:
									ptr = "VOL DOWN";
									break;
								case interfaceCommandVolumeUp:
									ptr = "VOL UP";
									break;
								default:
									sprintf(ibuf, "MISC(%d)", lastRecvCmd);
									ptr = ibuf;
							}

							lastRecvCmd = interfaceCommandNone;

							sprintf(obuf, "EVENT: %s\r\n", ptr);
							write(s, obuf, strlen(obuf));
						} else
						{
							usleep(10000);
						}
					}

					captureInput = 0;

					strcpy(obuf, "Done with input\r\n");
				} else if (strstr(ibuf, "seterrorled ") == ibuf)
				{
					int todo = -1;

					ptr = ibuf+strlen("seterrorled ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					todo = atoi(ptr);

					if (todo >= 0 && todo <= 1)
					{
						sprintf(obuf, "stmclient %d", 11-todo);
						system(obuf);
						sprintf(obuf, "Error led is %s\r\n", todo == 1 ? "on" : "off");
					} else
					{
						sprintf(obuf, "ERROR: Error led cannot be set to %d\r\n", todo);
					}
				} else if (strstr(ibuf, "setstandbyled ") == ibuf)
				{
					int todo = -1;

					ptr = ibuf+strlen("setstandbyled ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					todo = atoi(ptr);

					if (todo >= 0 && todo <= 1)
					{
						sprintf(obuf, "stmclient %d", 4-todo);
						system(obuf);
						sprintf(obuf, "Standby led is %s\r\n", todo == 1 ? "on" : "off");
					} else
					{
						sprintf(obuf, "ERROR: Standby led cannot be set to %d\r\n", todo);
					}
				} else if (strstr(ibuf, "outputmode ") == ibuf)
				{
					int format = -1;

					ptr = ibuf+strlen("outputmode ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					if (strcmp(ptr, "cvbs") == 0 || strcmp(ptr, "composite") == 0)
					{
						format = DSOS_CVBS;
					} else if (strcmp(ptr, "s-video") == 0 || strcmp(ptr, "yc") == 0)
					{
						format = DSOS_YC;
					}

					if (format >= 0)
					{
						gfx_changeOutputFormat(format);
						appControlInfo.outputInfo.format = format;
						sprintf(obuf, "Output mode set to %s\r\n", ptr);
					} else
					{
						sprintf(obuf, "ERROR: Output mode cannot be set to %s\r\n", ptr);
					}
				}
#ifdef ENABLE_DVB
				else if (strcmp(ibuf, "gettunerstatus") == 0)
				{
					if ( appControlInfo.tunerInfo[0].status != tunerNotPresent ||
						appControlInfo.tunerInfo[1].status != tunerNotPresent)
					{
						strcpy(obuf, "Tuner present\r\n");
					} else
					{
						strcpy(obuf, "ERROR: Tuner not found\r\n");
					}
				} else if (strstr(ibuf, "dvbchannel ") == ibuf)
				{
					int channel = -1;

					ptr = ibuf+strlen("dvbchannel ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					channel = atoi(ptr);

					if (channel >= 0)
					{
						int index = 0, item = 0;
						int t = 0;

						while (item < dvb_getNumberOfServices())
						{
							EIT_service_t* service = dvb_getService(index);
							if (service == NULL)
							{
								break;
							}
							if (dvb_hasMedia(service))
							{
								if (item == channel)
								{
									while (offair_services[t].service != service && t < MAX_MEMORIZED_SERVICES)
									{
										t++;
									}
									if (t < MAX_MEMORIZED_SERVICES)
									{
										offair_channelChange(interfaceInfo.currentMenu, (void*)CHANNEL_INFO_SET(screenMain, t));
										sprintf(obuf, "DVB Channel set to %s - %s\r\n", ptr, dvb_getServiceName(service));
									} else
									{
										sprintf(obuf, "ERROR: DVB Channel cannot be set to %s\r\n", ptr);
									}
									break;
								}
								item++;
							}
							index++;
						}
					} else
					{
						sprintf(obuf, "ERROR: DVB Channel cannot be set to %s\r\n", ptr);
					}
				} else if (strstr(ibuf, "dvbclear ") == ibuf)
				{
					int todo = -1;

					ptr = ibuf+strlen("dvbclear ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					todo = atoi(ptr);

					if (todo > 0)
					{
						offair_clearServiceList(0);
						sprintf(obuf, "DVB Service list cleared\r\n");
					} else
					{
						sprintf(obuf, "ERROR: Incorrect value %d\r\n", todo);
					}
				} else if (strstr(ibuf, "dvbscan ") == ibuf)
				{
					unsigned long from, to, step, speed;

					ptr = ibuf+strlen("dvbscan ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					if (sscanf(ptr, "%lu %lu %lu %lu", &from, &to, &step, &speed) == 4 &&
						from > 1000 && to > 1000 && step >= 1000 && speed <= 10)
					{
						int index = 0, item = 0;

						appControlInfo.dvbtInfo.fe.lowFrequency = from;
						appControlInfo.dvbtInfo.fe.highFrequency = to;
						appControlInfo.dvbtInfo.fe.frequencyStep = step;
						appControlInfo.dvbCommonInfo.adapterSpeed = speed;

						gfx_stopVideoProviders(screenMain);
						offair_serviceScan(interfaceInfo.currentMenu, NULL);
						sprintf(obuf, "DVB Channels scanned with %s\r\n", ptr);

						while (item < dvb_getNumberOfServices())
						{
							EIT_service_t* service = dvb_getService(index);
							if (service == NULL)
							{
								break;
							}
							if (dvb_hasMedia(service))
							{
								sprintf(&obuf[strlen(obuf)], "%d: %s%s\r\n", item, dvb_getServiceName(service), dvb_getScrambled(service) ? " (scrambled)" : "");
								item++;
							}
							index++;
						}

					} else
					{
						sprintf(obuf, "ERROR: DVB Channels cannot be scanned with %s\r\n", ptr);
					}
				} else if (strstr(ibuf, "dvblist") == ibuf)
				{
					int index = 0, item = 0;

					while (item < dvb_getNumberOfServices())
					{
						EIT_service_t* service = dvb_getService(index);
						if (service == NULL)
						{
							break;
						}
						if (dvb_hasMedia(service))
						{
							sprintf(&obuf[strlen(obuf)], "%d: %s%s\r\n", item, dvb_getServiceName(service), dvb_getScrambled(service) ? " (scrambled)" : "");
							item++;
						}
						index++;
					}
					sprintf(&obuf[strlen(obuf)], "Total %d Channels\r\n", dvb_getNumberOfServices());
				} else if (strstr(ibuf, "dvbcurrent") == ibuf)
				{
					if (appControlInfo.dvbInfo[screenMain].active)
					{
						int index = 0, item = 0;

						while (item < dvb_getNumberOfServices())
						{
							EIT_service_t* service = dvb_getService(index);
							if (service == NULL)
							{
								break;
							}
							if (dvb_hasMedia(service))
							{
								if (service == offair_services[appControlInfo.dvbInfo[screenMain].channel].service)
								{
									sprintf(obuf, "%d: %s%s\r\n", item, dvb_getServiceName(service), dvb_getScrambled(service) ? " (scrambled)" : "");
									break;
								}
								item++;
							}
							index++;
						}
					} else
					{
						sprintf(obuf, "Not playing\r\n");
					}
				}
#endif /* ENABLE_DVB */
				else if (strstr(ibuf, "playvod ") == ibuf)
				{
					ptr = ibuf+strlen("playvod ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					if (strstr(ptr, "rtsp://") != NULL && rtsp_playURL(screenMain, ptr, NULL, NULL) == 0)
					{
						sprintf(obuf, "VOD URL set to %s\r\n", ptr);
					} else
					{
						sprintf(obuf, "ERROR: VOD URL cannot be set to %s\r\n", ptr);
					}
				} else if (strstr(ibuf, "playrtp ") == ibuf)
				{
					ptr = ibuf+strlen("playrtp ");

					while (*ptr != 0 && *ptr == ' ')
					{
						ptr++;
					}

					if (strstr(ptr, "://") != NULL && rtp_playURL(screenMain, ptr, NULL, NULL) == 0)
					{
						sprintf(obuf, "RTP URL set to %s\r\n", ptr);
					} else
					{
						sprintf(obuf, "ERROR: RTP URL cannot be set to %s\r\n", ptr);
					}
				} else if (strcmp(ibuf, "stop") == 0 || strcmp(ibuf, "quit") == 0)
				{
					gfx_stopVideoProviders(screenMain);
					strcpy(obuf, "stopped\r\n");
					interface_showMenu(1, 1);
				} else if (strcmp(ibuf, "exit") == 0 || strcmp(ibuf, "quit") == 0)
				{
					break;
				}
#ifdef ENABLE_MESSAGES
				else if (strncmp(ibuf, "newmsg", 6) == 0)
				{
					appControlInfo.messagesInfo.newMessages = 1;
					interface_displayMenu(1);
				}
#endif
				else
				{
#ifdef TEST_SERVER_INET
					strcpy(obuf, "ERROR: Unknown command\r\n");
#endif
				}

				write(s, obuf, strlen(obuf)+1);
			} else if (pos >= (int)sizeof(ibuf)-1)
			{
				pos = 0;
			}
		}

		eprintf("App: Server disconnect client\n");
		close(s);
	}

	eprintf("App: Server close socket\n");
	close(sock);

	dprintf("%s: Server stop\n", __FUNCTION__);

	return NULL;
}
#endif // #ifdef ENABLE_TEST_SERVER

void *keyThread(void *pArg)
{
	IDirectFBEventBuffer *eventBuffer = (IDirectFBEventBuffer*)pArg;
	DFBInputDeviceKeySymbol lastsym;
	int allow_repeat = 0;
	int timediff,res_standby;
	struct timeval lastpress, currentpress, currenttime;
	interfaceCommand_t lastcmd;
	interfaceCommandEvent_t curcmd;

	keyThreadActive = 1;

	lastsym = 0;
	lastcmd = interfaceCommandNone;
	curcmd.command = interfaceCommandNone;
	curcmd.repeat = 0;
	curcmd.source = DID_KEYBOARD;
	memset(&lastpress, 0, sizeof(struct timeval));

//#ifndef DEBUG
	if(appControlInfo.playbackInfo.bResumeAfterStart)
	{
		int res = 0;
		switch( appControlInfo.playbackInfo.streamSource )
		{
#ifdef ENABLE_DVB
			case streamSourceDVB:
				eprintf("App: Autostart %d DVB channel\n", appControlInfo.dvbInfo[screenMain].channel);
				res = offair_channelChange((interfaceMenu_t*)&interfaceMainMenu,
					(void*)CHANNEL_INFO_SET(screenMain,appControlInfo.dvbInfo[screenMain].channel));
				break;
#endif
			case streamSourceIPTV:
				{
					char desc[MENU_ENTRY_INFO_LENGTH];
					char thumb[MAX_URL];

					eprintf("App: Autostart '%s' from IPTV\n", appControlInfo.rtpMenuInfo.lastUrl);
					rtp_getPlaylist(screenMain);
					getParam(SETTINGS_FILE, "LASTDESCRIPTION", "", desc);
					getParam(SETTINGS_FILE, "LASTTHUMBNAIL", "", thumb);
					res = rtp_playURL(screenMain, appControlInfo.rtpMenuInfo.lastUrl, desc[0] ? desc : NULL, thumb[0] ? thumb : 0);
				}
				break;
			case streamSourceUSB:
				{
					eprintf("App: Autostart '%s' from media\n", appControlInfo.mediaInfo.lastFile );
					strcpy( appControlInfo.mediaInfo.filename, appControlInfo.mediaInfo.lastFile );
					getParam(SETTINGS_FILE, "LASTDESCRIPTION", "", appControlInfo.playbackInfo.description);
					getParam(SETTINGS_FILE, "LASTTHUMBNAIL", "",   appControlInfo.playbackInfo.thumbnail);
					appControlInfo.playbackInfo.playingType = media_getMediaType(appControlInfo.mediaInfo.filename);
					interface_showMenu( 0, 0 );
					res = media_streamStart();
				}
				break;
			case streamSourceFavorites:
				eprintf("App: Autostart '%s' from Favorites\n", playlist_getLastURL());
				res = playlist_streamStart();
				break;
#ifdef ENABLE_YOUTUBE
			case streamSourceYoutube:
				eprintf("App: Autostart '%s' from YouTube\n", appControlInfo.mediaInfo.lastFile );
				getParam(SETTINGS_FILE, "LASTDESCRIPTION", "", appControlInfo.playbackInfo.description);
				getParam(SETTINGS_FILE, "LASTTHUMBNAIL", "",   appControlInfo.playbackInfo.thumbnail);
				res = youtube_streamStart();
				break;
#endif
			default: ;
		}
		if( res != 0 )
		{
			appControlInfo.playbackInfo.streamSource = streamSourceNone;
			saveAppSettings();
			interface_showMenu(1, 1);
			//interface_showSplash(0, 0, interfaceInfo.screenWidth, interfaceInfo.screenHeight, 1, 1);
		}
	}
//#endif

	while ( keepCommandLoopAlive )
	{
		DFBEvent event;

		//dprintf("%s: WaitForEventWithTimeout\n", __FUNCTION__);

		if (flushEventsFlag)
		{
			eventBuffer->Reset(eventBuffer);
		}

		eventBuffer->WaitForEventWithTimeout(eventBuffer, 1, 0);

		flushEventsFlag = 0;

		if ( eventBuffer->HasEvent(eventBuffer) == DFB_OK )
		{
			eventBuffer->GetEvent(eventBuffer, &event);
			eventBuffer->Reset(eventBuffer);

			gettimeofday(&currenttime, NULL);

			timediff = (currenttime.tv_sec-event.input.timestamp.tv_sec)*1000000+(currenttime.tv_usec-event.input.timestamp.tv_usec);

			if (timediff > OUTDATE_TIMEOUT)
			{
				dprintf("%s: ignore event, age %d\n", __FUNCTION__, timediff);
				continue;
			}

			dprintf("%s: got event, age %d\n", __FUNCTION__, timediff);

			//dprintf("%s: dev_id %d\n", __FUNCTION__, event.input.device_id);

			if ( event.clazz == DFEC_INPUT && (event.input.type == DIET_KEYPRESS || event.input.type == DIET_BUTTONPRESS) )
			{
				/*if ( event.input.key_symbol == DIKS_ESCAPE )
				{
					eprintf("App: User abort. Exiting.\n");
					keepCommandLoopAlive = 0;
					break;
				}*/

				res_standby = checkPowerOff(event);
				if(res_standby)
				{
					//clear event
					eventBuffer->Reset(eventBuffer);
					continue;
				}

				memcpy(&currentpress, &event.input.timestamp, sizeof(struct timeval));

				if ( event.input.button == 9) // PSU
				{
					continue;
				}

				dprintf("%s: Char: %d (%d) %d, '%c' ('%c') did=%d\n", __FUNCTION__, event.input.key_symbol, event.input.key_id, event.input.button, event.input.key_symbol, event.input.key_id, event.input.device_id);

				interfaceCommand_t cmd = parseEvent(&event);

				if (cmd == interfaceCommandCount)
				{
					continue;
				}

				curcmd.original = cmd;
				curcmd.command = cmd;
				curcmd.source = event.input.device_id == DIDID_ANY ? DID_KEYBOARD : event.input.device_id;
				//curcmd.repeat = 0;

				dprintf("%s: event %d\n", __FUNCTION__, cmd);

				timediff = (currentpress.tv_sec-lastpress.tv_sec)*1000000+(currentpress.tv_usec-lastpress.tv_usec);

				dprintf("%s: timediff %d\n", __FUNCTION__, timediff);

				if (cmd == lastcmd && timediff<REPEAT_TIMEOUT*2)
				{
					curcmd.repeat++;
				} else
				{
					curcmd.repeat = 0;
				}

				if (appControlInfo.inputMode == inputModeABC &&
					event.input.device_id != DID_KEYBOARD && event.input.device_id != DIDID_ANY && /* not keyboard */
					cmd >= interfaceCommand0 &&
					cmd <= interfaceCommand9 )
				{
					int num = cmd-interfaceCommand0;
					static int offset = 0;

					dprintf("%s: DIGIT: %d = %d repeat=%d offset=%d\n", __FUNCTION__, cmd, num, curcmd.repeat, offset);

					curcmd.command = interface_symbolLookup( num, curcmd.repeat, &offset );
				}
				else if (cmd != interfaceCommandVolumeUp &&
					cmd != interfaceCommandVolumeDown &&
					cmd != interfaceCommandUp &&
					cmd != interfaceCommandDown &&
					cmd != interfaceCommandLeft &&
					cmd != interfaceCommandRight
					)
				{
					if ((event.input.key_symbol == lastsym && cmd == lastcmd) && timediff<REPEAT_TIMEOUT && allow_repeat < REPEAT_THRESHOLD)
					{
						dprintf("%s: skip %d\n", __FUNCTION__, allow_repeat);
						allow_repeat++;
						memcpy(&lastpress, &currentpress, sizeof(struct timeval));
						continue;
					} else if ((event.input.key_symbol != lastsym || cmd == lastcmd) || timediff>=REPEAT_TIMEOUT) {
						dprintf("%s: new key %d\n", __FUNCTION__, allow_repeat);
						allow_repeat = 0;
					}
				} else {
					eventBuffer->Reset(eventBuffer);
				}

				memcpy(&lastpress, &currentpress, sizeof(struct timeval));
				lastsym = event.input.key_symbol;
				lastcmd = cmd;

				dprintf("%s: ---> Char: %d, Command %d\n", __FUNCTION__, event.input.key_symbol, cmd);

				interface_processCommand(&curcmd);
			} else if ( (event.input.device_id == DID_KEYBOARD || event.input.device_id == DIDID_ANY /* 0 is keyboard */) && event.clazz == DFEC_INPUT && event.input.type == DIET_KEYRELEASE )
			{
				//dprintf("%s: release!\n", __FUNCTION__);
				lastsym = lastcmd = 0;
			}
		}
	}

	dprintf("%s: Command loop stopped\n", __FUNCTION__);

	keyThreadActive = 0;

	return NULL;
}

static void set_to_raw(int tty_fd)
{
	struct termios tty_attr;

	tcgetattr(tty_fd,&tty_attr);
	tty_attr.c_lflag &= ~ICANON;
	tcsetattr(tty_fd,TCSANOW,&tty_attr);
}

static void set_to_buffered(int tty_fd)
{
	struct termios tty_attr;

	tcgetattr(tty_fd,&tty_attr);
	tty_attr.c_lflag |= ICANON;
	tcsetattr(tty_fd,TCSANOW,&tty_attr);
}

#ifdef STB225
static void setupFBResolution(char* fbName, int32_t width, int32_t height)
{
	int32_t fd;
	struct fb_var_screeninfo vinfo;
	int32_t error;
	fd = open(fbName, O_RDWR);

	if (fd >= 0)
	{
		/* Get variable screen information */
		error = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
		if (error != 0)
		{
			eprintf("App: Error reading variable information for '%s' - error code %d\n", fbName, error);
		}

		vinfo.xres = (uint32_t)width;
		vinfo.yres = (uint32_t)height;
		vinfo.activate = FB_ACTIVATE_FORCE;

		/* Set up the framebuffer resolution */
		error = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
		if (error != 0)
		{
			eprintf("App: Error setting variable information for '%s' - error code %d\n", fbName, error);
		}

		(void)close(fd);
	}
}
static void setupFramebuffers(void)
{
	int32_t fd;
	int32_t width = 0;
	int32_t height = 0;
	int32_t multipleFrameBuffers;

	if (appControlInfo.restartResolution)
	{
		multipleFrameBuffers = helperFileExists("/dev/fb1");

		if (multipleFrameBuffers)
		{
			switch (appControlInfo.restartResolution)
			{
				case(DSOR_720_480) :
					{
						width  = 512;
						height = 480;
					}
					break;
				case(DSOR_720_576) :
					{
						width  = 512;
						height = 576;
					}
					break;
				case(DSOR_1280_720) :
					{
						width  = 720;
						height = 720;
					}
					break;
				case(DSOR_1920_1080) :
					{
						width  = 960;
						height = 540;
					}
					break;
				default :
					{
						width  = 0;
						height = 0;
					}
					break;
			}
		}
		else
		{
			switch (appControlInfo.restartResolution)
			{
				case(DSOR_720_480) :
					{
						width  = 720;
						height = 480;
					}
					break;
				case(DSOR_720_576) :
					{
						width  = 720;
						height = 576;
					}
					break;
				case(DSOR_1280_720) :
					{
						width  = 1280;
						height = 720;
					}
					break;
				case(DSOR_1920_1080) :
					{
						width  = 1920;
						height = 1080;
					}
					break;
				default :
					{
						width  = 0;
						height = 0;
					}
					break;
			}
		}

		/* Set up the Directfb display mode file */
		fd = open("/etc/directfbrc", O_RDWR);
		if (fd >= 0)
		{
			char text[128];

			(void)sprintf(text, "mode=%dx%d\n", width, height);
			(void)write(fd, text, strlen(text));
			(void)strcpy(text, "pixelformat=ARGB\n");
			(void)write(fd, text, strlen(text));
			(void)strcpy(text, "depth=32\n");
			(void)write(fd, text, strlen(text));
			(void)strcpy(text, "no-vt-switch\n");
			(void)write(fd, text, strlen(text));
			(void)strcpy(text, "\n");
			(void)write(fd, text, strlen(text));

			(void)close(fd);
		}

		setupFBResolution("/dev/fb0", width, height);

		setupFBResolution("/dev/fb1", width, height);

	}
	{
		char text[256];

		getParam("/etc/init.d/S35pnxcore.sh", "resolution", "1280x720x60p", text);
		if (strstr(text, "1920x1080")!=0) {
			width = 1920; height = 1080;
//printf("%s[%d]: %dx%d\n", __FILE__, __LINE__, width, height);
			Stb225ChangeDestRect("/dev/fb1", width/2, 0, width/2, height);
			Stb225ChangeDestRect("/dev/fb0", 0, 0, width, height);
			interfaceInfo.enable3d = 1;
		} /*else 
		if (strstr(text, "1280x720")!=0) {
			width = 1280; height = 720;
		}*/
	}

}
#endif

void initialize(int argc, char *argv[])
{
	DFBResult ret;
	pthread_t thread;

	tzset();

	//dprintf("%s: wait framebuffer\n", __FUNCTION__);

	/* Wait for the framebuffer to be installed */
	while ( !helperFileExists(FB_DEVICE) )
	{
		sleep(1);
	}

	//dprintf("%s: open terminal\n", __FUNCTION__);

	flushEventsFlag = 0;
	keepCommandLoopAlive = 1;

	l10n_init(l10n_currentLanguage);

	parse_commandLine(argc, argv);

#ifdef STB225
	Stb225initIR();
	setupFramebuffers();
#endif

#ifdef ENABLE_DVB
	dvb_init();
#endif

	downloader_init();

	gfx_init(argc, argv);

	helperCreatePipe();

	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);
	//signal(SIGSEGV, signal_handler);

	signal(SIGBUS, stub_signal_handler);
	signal(SIGPIPE, stub_signal_handler);
	signal(SIGHUP, hup_signal_handler);

#ifdef STB82
	if(appControlInfo.watchdogEnabled)
	{
		watchdog_init();
	}
#endif
	sound_init();

#ifdef ENABLE_DLNA
	dlna_start();
#endif

#ifdef PNX8550
	//gfx_startVideoProvider("/media/Racecar.mpg", gfx_getMainVideoLayer(), 0, "");
#endif
	interface_init();
	menu_init();

	// Menus should already be initialized
	signal(SIGUSR1, usr1_signal_handler);

	ret = pgfx_dfb->CreateInputEventBuffer( pgfx_dfb, DICAPS_KEYS|DICAPS_BUTTONS|DICAPS_AXES, DFB_TRUE, &appEventBuffer);

#ifdef ENABLE_PVR
	pvr_init();
#endif

	pthread_create(&thread, NULL, keyThread, (void*)appEventBuffer);
	pthread_detach(thread);

#ifdef ENABLE_TEST_SERVER
	pthread_create(&server_thread, NULL, testServerThread, NULL);
#endif

	{
		FILE *f = fopen(APP_LOCK_FILE, "w");
		if (f != NULL)
		{
			fclose(f);
		}
	}

#ifdef ENABLE_SECUREMEDIA
	SmPlugin_Init("");
#endif

#ifdef ENABLE_VOIP
	voip_init();
#endif

	if (gAllowConsoleInput)
	{
		term = open("/dev/fd/0", O_RDWR);
		set_to_raw(term);
	}
}

void cleanup()
{
#ifdef ENABLE_TEST_SERVER
	pthread_join(server_thread, NULL);
#endif

	interfaceInfo.cleanUpState	=	1;

	dprintf("%s: Show main menu\n", __FUNCTION__);

	interface_menuActionShowMenu(interfaceInfo.currentMenu, (void*)&interfaceMainMenu);

	dprintf("%s: release event buffer\n", __FUNCTION__);

	appEventBuffer->Release(appEventBuffer);

	dprintf("%s: close video providers\n", __FUNCTION__);

	gfx_stopVideoProviders(screenMain);
	gfx_stopVideoProviders(screenPip);
	media_slideshowStop(1);

#ifdef PNX8550
	//gfx_stopVideoProvider(gfx_getMainVideoLayer(), 1, 0);
#endif

	/*
	gfx_clearSurface(pgfx_frameBuffer, interfaceInfo.screenWidth, interfaceInfo.screenHeight);
	gfx_flipSurface(pgfx_frameBuffer);
	gfx_clearSurface(pgfx_frameBuffer, interfaceInfo.screenWidth, interfaceInfo.screenHeight);
	gfx_flipSurface(pgfx_frameBuffer);
	*/
#ifdef ENABLE_VOIP
	voip_cleanup();
#endif

#ifdef ENABLE_SECUREMEDIA
	SmPlugin_Finit();
#endif

	menu_cleanup();

#ifdef ENABLE_DLNA
	dlna_stop();
#endif

	dprintf("%s: terminate gfx\n", __FUNCTION__);

	gfx_terminate();

	dprintf("%s: terminate interface\n", __FUNCTION__);

	interface_destroy();

	dprintf("%s: remove pipe\n", __FUNCTION__);

	helperRemovePipe();

#ifdef ENABLE_PVR
	pvr_cleanup();
#endif

#ifdef ENABLE_DVB
	dprintf("%s: terminate dvb\n", __FUNCTION__);
	dvb_terminate();
#endif

#ifdef ENABLE_VIDIMAX
	eprintf ("vidimax_cleanup\n");
	vidimax_cleanup();	
#endif

	l10n_cleanup();

	downloader_cleanup();

	dprintf("%s: close console\n", __FUNCTION__);
	if (gAllowConsoleInput)
	{
		set_to_buffered(term);
		close(term);
	}

#ifdef STB82
	dprintf("%s: stop watchdog\n", __FUNCTION__);
	if(appControlInfo.watchdogEnabled)
	{
		eprintf("App: Stopping watchdog ...\n");
		watchdog_deinit();
	}
#endif

	unlink(APP_LOCK_FILE);
}

void process()
{
	DFBEvent event;

	event.clazz = DFEC_INPUT;
	event.input.type = DIET_KEYPRESS;
	event.input.flags = DIEF_KEYSYMBOL;
	event.input.key_symbol = 0;

	while (keepCommandLoopAlive)
	{
		//dprintf("%s: process...\n", __FUNCTION__);
		if (!gAllowConsoleInput)
		{
			sleep(1);
		} else
		{
			int value;
			fd_set rdesc;
			struct timeval tv;


			FD_ZERO(&rdesc);
			FD_SET(0, &rdesc); // stdin
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			//dprintf("%s: Waiting for terminal input...\n", __FUNCTION__);

			value = select(1, &rdesc, 0, 0, &tv);

			//dprintf("%s: Select returned %d, keepCommandLoopAlive is %d and 0 isset: %d\n", __FUNCTION__, value, keepCommandLoopAlive, FD_ISSET(0, &rdesc));

			if (value <= 0)
			{
				continue;
			}

			value = getchar();
			value = tolower((char)value);
			event.input.key_symbol = 0;

			//dprintf("%s: Got char: %X\n", __FUNCTION__, value);

			switch (value)
			{
			case(0x1B) : /* escape! */
				{
					int escapeValue[2];

					/* Get next 2 values! */
					escapeValue[0] = getchar();
					if( escapeValue[0] == 0x1B )
					{
						event.input.key_symbol = DIKS_ESCAPE;
						break;
					}
					escapeValue[1] = getchar();

					//dprintf("%s: ESC: %X %X\n", __FUNCTION__, escapeValue[0], escapeValue[1]);

					if (escapeValue[0] == 0x5B)
					{
						switch (escapeValue[1])
						{
						case(0x31) : event.input.key_symbol = DIKS_HOME; break;
						case(0x35) : event.input.key_symbol = DIKS_PAGE_UP; break;
						case(0x36) : event.input.key_symbol = DIKS_PAGE_DOWN; break;
						case(0x41) : event.input.key_symbol = DIKS_CURSOR_UP; break;
						case(0x42) : event.input.key_symbol = DIKS_CURSOR_DOWN; break;
						case(0x43) : event.input.key_symbol = DIKS_CURSOR_RIGHT; break;
						case(0x44) : event.input.key_symbol = DIKS_CURSOR_LEFT; break;
						}
					} else if (escapeValue[0] == 0x4F)
					{
						switch (escapeValue[1])
						{
						case(0x50) : event.input.key_symbol = DIKS_F1; break;
						case(0x51) : event.input.key_symbol = DIKS_F2; break;
						case(0x52) : event.input.key_symbol = DIKS_F3; break;
						case(0x53) : event.input.key_symbol = DIKS_F4; break;
						}
					}
					break;
				}
			case(0x8) :
			case(0x7F): event.input.key_symbol = DIKS_BACKSPACE; break;
			case(0xA) : event.input.key_symbol = DIKS_ENTER; break;
			case(0x20): event.input.key_symbol = DIKS_MENU; break;
			case('+') : event.input.key_symbol = DIKS_VOLUME_UP; break;
			case('-') : event.input.key_symbol = DIKS_VOLUME_DOWN; break;
			case('*') : event.input.key_symbol = DIKS_MUTE; break;
			case('i') : event.input.key_symbol = DIKS_INFO; break;
			case('r') : event.input.key_symbol = DIKS_RECORD; break;
			case('0') : event.input.key_symbol = DIKS_0; break;
			case('1') : event.input.key_symbol = DIKS_1; break;
			case('2') : event.input.key_symbol = DIKS_2; break;
			case('3') : event.input.key_symbol = DIKS_3; break;
			case('4') : event.input.key_symbol = DIKS_4; break;
			case('5') : event.input.key_symbol = DIKS_5; break;
			case('6') : event.input.key_symbol = DIKS_6; break;
			case('7') : event.input.key_symbol = DIKS_7; break;
			case('8') : event.input.key_symbol = DIKS_8; break;
			case('9') : event.input.key_symbol = DIKS_9; break;
			}
			if (event.input.key_symbol != 0)
			{
				dprintf("%s: '%c' [%d]\n", __FUNCTION__, value, value);
				event.input.flags = DIEF_KEYSYMBOL;
				gettimeofday(&event.input.timestamp, 0);
				appEventBuffer->PostEvent(appEventBuffer, &event);
			}
		}
	}

	dprintf("%s: process finished\n", __FUNCTION__);
}

int helperStartApp(const char* filename)
{
	strcpy(startApp, filename);

	raise(SIGINT);

	return 0;
}

int is_open_browserAuto()
{
	char buf[MENU_ENTRY_INFO_LENGTH];

	getParam(DHCP_VENDOR_FILE, "A_homepage", "", buf);

	if(strstr(buf,"://") != NULL)
	{
		return 1;
	}

	getParam(BROWSER_CONFIG_FILE, "AutoLoadingMW", "OFF", buf);
	if(buf[0]==0)
		return 0;
	if(strcmp(buf,"ON"))
		return 0;
	return 1;
}

int open_browserAuto()
{
	char buf[MENU_ENTRY_INFO_LENGTH];
	char open_link[MENU_ENTRY_INFO_LENGTH];

	getParam(DHCP_VENDOR_FILE, "A_homepage", "", buf);
	
	if(strstr(buf,"://") == NULL)
	{
		getParam(BROWSER_CONFIG_FILE, "AutoLoadingMW", "OFF", buf);
		if(buf[0]==0)
			return 0;
		if(strcmp(buf,"ON"))
			return 0;

		getParam(BROWSER_CONFIG_FILE, "HomeURL", "", buf);
	}

	if(buf[0]!=0)
	{
		strcpy(&buf[strlen(buf)]," LoadInMWMode");
		sprintf(open_link,"while [ ! -f /usr/local/webkit/_start.sh ]; do echo wait userfs; sleep 1; done; /usr/local/webkit/_start.sh \"%s\"",buf);
	} else
	{
		sprintf(open_link,"while [ ! -f /usr/local/webkit/_start.sh ]; do echo wait userfs; sleep 1; done; /usr/local/webkit/_start.sh");
	}
	eprintf("App: Browser cmd: '%s' \n", open_link);

	{
		FILE *f = fopen(APP_LOCK_FILE, "w");
		if (f != NULL)
		{
			fclose(f);
		}
	}

	system(open_link);
	return 1;
}

void tprintf(const char *str, ...)
{
	if (gAllowTextMenu)
	{
		va_list va;
		va_start(va, str);
		vprintf(str, va);
		va_end(va);
	}
}

int main(int argc, char *argv[])
{
	int restart, i;
	FILE *pidfile;

	pidfile = fopen("/var/run/mainapp.pid", "w");
	if (pidfile)
	{
		fprintf(pidfile, "%d", getpid());
		fclose(pidfile);
	}

	appInfo_init();

#ifdef ENABLE_VOIP
	/* logging in to server when perfoming socket connect */
	voip_setBuzzer();
#endif

	open_browserAuto();
/*
	npt_range range = { 5.0f, 10.0f };
	prtspConnection connection;
	if (smrtsp_easy_init(&connection, "192.168.200.1", 554, "football2.mpg") == 0)
	{
		smrtsp_play_range_scale(connection, &range, 2.0f);
	}
*/
	/* By default send all output to stdout, except for error and warning output */
	for (i=0;i<errorLevelCount; i++)
	{
		setoutput((errorLevel_t)i, (i == errorLevelError || i == errorLevelWarning) ? stderr : stdout);
	}
#ifdef DEBUG
	setoutputlevel(errorLevelDebug);
#else
	setoutputlevel(errorLevelNormal);
#endif

	do {
		restart = 0;

		//dprintf("%s: initialize\n", __FUNCTION__);

		initialize(argc, argv);

		//dprintf("%s: process\n", __FUNCTION__);

		process();

		//dprintf("%s: Wait for key thread\n", __FUNCTION__);

		while (keyThreadActive != 0)
		{
			usleep(100000);
		}

		//dprintf("%s: Do cleanup\n", __FUNCTION__);

		cleanup();

		if (startApp[0] != 0)
		{
			eprintf("App: Starting %s\n", startApp);
			//sleep(1);
			if(!is_open_browserAuto())
				system(startApp);
			startApp[0] = 0;
			/*restart = 1;
			eprintf("App: Resuming...\n");
			sleep(1);*/
		}

	} while (restart);

	unlink("/var/run/mainapp.pid");

	eprintf("App: Goodbye.\n");

	return 0;
}

char *helperEthDevice(int i)
{
	static char temp[64];

	switch( i )
	{
#ifdef ENABLE_PPP
		case ifacePPP:
			strcpy(temp, "ppp0");
			break;
#endif
#ifdef ENABLE_WIFI
		case ifaceWireless:
			strcpy(temp, "wlan0");
			break;
#endif
		case ifaceWAN:
			getParam(STB_CONFIG_FILE, "CONFIG_GATEWAY_MODE", "OFF", temp);
			if (strcmp(temp, "BRIDGE") == 0)
			{
				sprintf(temp, "br%d", i);
				break;
			}
		default:
			sprintf(temp, "eth%d", i);
			break;
	}
	return temp;
}

char *helperStrCpyTrimSystem(char *dst, char *src)
{
	char *ptr = dst;
	while( *src )
	{
		if( (unsigned char)(*src) > 127 )
		{
			*ptr++ = *src;
		} else if( *src >= ' ' )
			switch(*src)
			{
				case '"': case '*': case '/': case ':': case '|':
				case '<': case '>': case '?': case '\\': case 127: break;
				default: *ptr++ = *src;
			}
		src++;
	}
	*ptr = 0;
	return dst;
}

int helperSafeStrCpy( char** dest, const char* src )
{
	size_t src_length;
	if( NULL == dest )
		return -2;
	if( NULL == src )
	{
		if( NULL != *dest )
		{
			free( *dest );
			*dest = NULL;
		}
	}
	src_length = strlen(src);
	if( (NULL == *dest && NULL == (*dest = (char*)dmalloc( src_length+1 )))
	 || (strlen( *dest ) < src_length && NULL == (*dest = (char*)drealloc( *dest, src_length+1))) )
		return 1;
	memcpy( *dest, src, src_length+1 );
	return 0;
}

int helperCheckUpdates()
{
	char cmd[MAX_URL];
	char *str, *url, *ptr;
	FILE *file = NULL;
	int ret;
	url_desc_t url_desc;

	str = cmd + sprintf(cmd, "clientUpdater -cs");
	if( str < cmd )
		return 1;

	file = popen("hwconfigManager a -1 UPURL 2>/dev/null | grep \"VALUE:\" | sed 's/VALUE: \\(.*\\)/\\1/'","r");
	if( file )
	{
		url = str + (sizeof(" -h ")-1);
		if ( fgets(url, MAX_URL-sizeof("clientUpdater -cs -h "), file) != NULL && url[0] != 0 && url[0] != '\n' )
		{
			for( ptr = &url[strlen(url)]; *ptr <= ' '; *ptr-- = 0 );
			if( parseURL( url, &url_desc ) == 0 && (url_desc.protocol == mediaProtoFTP || url_desc.protocol == mediaProtoHTTP) )
			{
				strncpy(str, " -h ", sizeof(" -h ")-1);
			}
		}
		dprintf("%s: Update url: '%s'\n", __FUNCTION__, url);
		fclose(file);
	}

	dprintf("%s: Checking updates: '%s'\n", __FUNCTION__, cmd);
	ret = system(cmd);

	return WIFEXITED(ret) == 1 && WEXITSTATUS(ret) == 1;
}

static int helper_confirmFirmwareUpdate(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	if (cmd->command == interfaceCommandRed || cmd->command == interfaceCommandExit || cmd->command == interfaceCommandLeft)
	{
		return 0;
	} else if (cmd->command == interfaceCommandGreen || cmd->command == interfaceCommandEnter || cmd->command == interfaceCommandOk)
	{
		system("sync");
		system("reboot");
		return 0;
	}
	return 1;
}

inline time_t gmktime(struct tm *t)
{
	time_t local_time;
	struct tm *pt;
	time(&local_time);
	pt = localtime(&local_time);
	return mktime(t) - timezone + (pt && pt->tm_isdst > 0 ? 3600 : 0); // mktime interprets it's argument as local time, to get UTC we must apply timezone fix
}
