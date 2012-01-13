
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

#ifdef STB82

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/time.h>

#include <fcntl.h>

#include "app_info.h"
#include "watchdog.h"
#include "sem.h"
#include "phStbEvent.h"
#include "phStbIAmAlive.h"

/***********************************************
 LOCAL MACROS                                  *
************************************************/

#define RESET_COUNT         (15)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static pthread_t watchdogThread;
static pthread_t eventReceiverThread;

static phStbEvent_Client_t* pTMIsAliveEvent = 0;

static UInt32 resetCounter = 0;

static pmysem_t resetCountSem;

/**************************************************************************
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

static void watchdog_resetActions(void)
{
    char dateString[256];
    char timeString[256];
    char commandString[512];
    struct timeval time;
    struct tm *pTime;

    appControlInfo.playbackInfo.streamSource = streamSourceNone;
    saveAppSettings();

    gettimeofday(&time, NULL);
    pTime = localtime(&time.tv_sec);

    strftime(dateString, 256, "%Y-%m-%d", pTime);
    strftime(timeString, 256, "%H-%M", pTime);

    /* Create a /debug directory. */
    system("mkdir -p /config/debug");

    /* Dump the TM0 debug buffer */
    sprintf(commandString, "cat /proc/nxp/dp0 > /config/debug/TM0_%s_%s.txt", dateString, timeString);
    system(commandString);

    /* Dump the TM1 debug buffer */
    sprintf(commandString, "cat /proc/nxp/dp1 > /config/debug/TM1_%s_%s.txt", dateString, timeString);
    system(commandString);

    /* Dump the RPC0 debug buffer */
    sprintf(commandString, "cat /proc/nxp/rpc0 > /config/debug/RPC0_%s_%s.txt", dateString, timeString);
    system(commandString);

    /* Dump the RPC1 debug buffer */
    sprintf(commandString, "cat /proc/nxp/rpc1 > /config/debug/RPC1_%s_%s.txt", dateString, timeString);
    system(commandString);

    /* Dump the Kernel Messages */
    sprintf(commandString, "cat /var/log/messages > /config/debug/VarLogMsg_%s_%s.txt", dateString, timeString);
    system(commandString);

	/* Dump the StbMainApp output */
	sprintf(commandString, "cat /var/log/mainapp.log > /config/debug/StbMainApp_%s_%s.txt", dateString, timeString);
	system(commandString);

    /* Change the permissions on the directory and files so that they can be deleted. */
    system("chmod -R a+w /config/debug");

    /* Reboot. */
    fprintf(stderr, "WATCHDOG TRIGGERED - NO DSP ACTIVITY - REBOOTING!!!!\n");
	printf("WATCHDOG TRIGGERED - NO DSP ACTIVITY - REBOOTING!!!!\n");
    system("reboot watchdog");
}

/********************************************************************************/
/* Function called just before thread exit. */
static void wathchdog_eventReceiverThreadTerm(void* pArg)
{
    if(pTMIsAliveEvent != 0)
    {
        phStbEvent_UnRegisterClient(pTMIsAliveEvent, 0);
    }
    eventReceiverThread = 0;
}

/********************************************************************************/
/* This thread periodically attempts to get an event from the DSP. If it receives
an event it resets the counter. By default the DSP sends an event every 2 seconds.
We poll every second to make sure we recieve the event in good time. */
static void *watchdog_eventReceiverThread(void *pArg)
{
    tmErrorCode_t err;
    UInt32 eventId, data1, data2;
    Int32 timeout = 0;
    UInt32 sourceId;

    pthread_cleanup_push(wathchdog_eventReceiverThreadTerm, pArg);

    err = phStbEvent_RegisterClient(&pTMIsAliveEvent, 0);
    if(err == 0)
    {
        err = phStbEvent_GetSourceId(PH_STBIAMALIVE_EVENT_SOURCE_NAME, &sourceId);
        if(err == 0)
        {
            err = phStbEvent_SetClientFilter(pTMIsAliveEvent, sourceId,
                phStbIAmAlive_NotificationTypes_Alive, 0, 0);
        }
    }

    do
    {
        err = phStbEvent_GetEvent(pTMIsAliveEvent, &sourceId, &eventId,
            &data1, &data2, timeout);
        if((err == 0) && (eventId == phStbIAmAlive_NotificationTypes_Alive))
        {
            mysem_get(resetCountSem);
            resetCounter = 0;
            mysem_release(resetCountSem);
        }

        pthread_testcancel();
        sleep(1);
        pthread_testcancel();
    } while (1);

    pthread_cleanup_pop(1);
    return NULL;
}

/********************************************************************************/
/* Function called just before thread exit. */
static void wathchdog_watchdogThreadTerm(void* pArg)
{
    watchdogThread = 0;
}

/********************************************************************************/
/* This thread attempts to increment the resetCounter, whilst the other
sets it to zero.  If the other thread hangs because the DSP has hung then
this thread will continue incrementing until it reaches the reset threshold
and performs a reboot.*/
static void *watchdog_thread(void *pArg)
{
    pthread_cleanup_push(wathchdog_watchdogThreadTerm, pArg);

    do
    {
        mysem_get(resetCountSem);
        resetCounter++;
        if(resetCounter >= RESET_COUNT)
        {
            watchdog_resetActions();
        }
        mysem_release(resetCountSem);

        pthread_testcancel();
        sleep(1);
        pthread_testcancel();
    } while (1);

    pthread_cleanup_pop(1);
    return NULL;
}

/********************************************************************************/
void watchdog_init(void)
{
    int st;

    mysem_create(&resetCountSem);
    pTMIsAliveEvent = 0;
    resetCounter = 0;
    st = pthread_create (&watchdogThread, NULL,
                         watchdog_thread,
                         NULL);

    if(st == -1)
    {
        fprintf(stderr, "Error during pthread_create (%d)\n", st);
    }
    else
    {
        /* Detach the pthread to allow resources to be freed off */
        pthread_detach(watchdogThread);
    }
    st = pthread_create (&eventReceiverThread, NULL,
                         watchdog_eventReceiverThread,
                         NULL);
    if(st == -1)
    {
        fprintf(stderr, "Error during pthread_create (%d)\n", st);
    }
    else
    {
        /* Detach the pthread to allow resources to be freed off */
        pthread_detach(eventReceiverThread);
    }
}

/********************************************************************************/
void watchdog_deinit(void)
{
    /* Signal that we want to kill the thread. */
    pthread_cancel (watchdogThread);
    /*Now make sure thread has exited*/
    while(watchdogThread)
    {
        usleep(10000);
    }
    /* Signal that we want to kill the thread. */
    pthread_cancel (eventReceiverThread);
    /*Now make sure thread has exited*/
    while(eventReceiverThread)
    {
        usleep(10000);
    }
    mysem_destroy(resetCountSem);
}

#endif
