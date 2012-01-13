
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

#include <fcntl.h>

#include "app_info.h"
#include "StbMainApp.h"
#include "trick_mode.h"
#include "phStbMpegTsTrickMode.h"
#include "debug.h"

/***********************************************
 LOCAL MACROS                                  *
************************************************/

#define MAX_ERROR_RETRIES       (100)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static int* gpCurrentFileIndex;
static char* gpCurrentDirectory;
static int gOutputFile;

/**************************************************************************
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

static int trickMode_endOfFileBackwards(int file, int* pNewFile, int userData)
{
    char filename[PATH_MAX];
    int index;
    int retVal = 0;

    (void)userData;

    close(file);

    index = (*gpCurrentFileIndex)-1;
    if(index >= 0)
    {
        sprintf(filename, "%s/part%02d.spts", gpCurrentDirectory, index);

        *pNewFile = open(filename, O_RDONLY);
        if(*pNewFile<0) 
        {
            dprintf("%s: Unable to open next file backwards\n", __FUNCTION__);
            retVal = -1;
            goto FuncReturn;
        }
        else
        {
            dprintf("%s: Opened next file backwards %s\n", __FUNCTION__, filename);
            *gpCurrentFileIndex = index;
        }
    }
    else
    {
        retVal = -1;
        goto FuncReturn;
    }

FuncReturn:
    return retVal;
}

static int trickMode_endOfFileForwards(int file, int* pNewFile, int userData)
{
    char filename[PATH_MAX];
    int retVal = 0;

    (void)userData;

    close(file);

    sprintf(filename, "%s/part%02d.spts", gpCurrentDirectory, ((*gpCurrentFileIndex)+1));

    *pNewFile = open(filename, O_RDONLY);
    if(*pNewFile<0) 
    {
        dprintf("%s: Unable to open next file forwards\n", __FUNCTION__);
        retVal = -1;
        goto FuncReturn;
    }
    else
    {
        dprintf("%s: Opened next file forwards %s\n", __FUNCTION__, filename);
        (*gpCurrentFileIndex)++;
    }

FuncReturn:
    return retVal;
}

static int trickMode_dataAvailable(unsigned char* pData, int size, int userData)
{
    int totalBytesWritten = 0;
    int bytesWritten = 0;
    int bytesToWrite = 0;

    dprintf("%s: pData:%p size:%d userData:%d to output:%d\n", __FUNCTION__,
        pData, size, userData, gOutputFile);

    (void)userData;
    bytesToWrite = size;
    do
    {
        bytesWritten = write(gOutputFile, &pData[totalBytesWritten], bytesToWrite);
        if(bytesWritten > 0)
        {
            totalBytesWritten += bytesWritten;
        }
        else
        {
            return -1;
        }
        bytesToWrite -= bytesWritten;
    }
    while(totalBytesWritten < size);

    dprintf("%s: done\n", __FUNCTION__);

    return 0;
}

void trickMode_stop(void)
{
    dprintf("%s: in\n", __FUNCTION__);
    phStbMpegTsTrickMode_Stop();
    dprintf("%s: out\n", __FUNCTION__);
}

int trickMode_play(char* directory, int *pFileIn, int *pWhichIndex, int *pOffset, int fileOut, stb810_trickModeDirection direction, stb810_trickModeSpeed speed, int videoPid)
{
    int retValue;
    phStbMpegTsTrickMode_EndOfFileCb_t endOfFileCb;
    int complete = 0;
    int numErrorRetries = 0;
    int errorOffset = 0;

    dprintf("%s: speed:%d fileIndex:%d\n", __FUNCTION__, speed, *pWhichIndex);

    /* Assign the globals. */
    gpCurrentFileIndex = pWhichIndex;
    gOutputFile = fileOut;
    gpCurrentDirectory = directory;

    if(direction == direction_forwards)
    {
        endOfFileCb = trickMode_endOfFileForwards;
    }
    else
    {
        endOfFileCb = trickMode_endOfFileBackwards;
    }

    while(!complete)
    {
        retValue = phStbMpegTsTrickMode_Play(pFileIn,
                                             pOffset,
                                             (phStbMpegTsTrickMode_Direction_t)direction,
                                             (phStbMpegTsTrickMode_Speed_t)speed,
                                             videoPid,
                                             trickMode_dataAvailable,
                                             endOfFileCb,
                                             0);
        if(retValue == -2)
        {
            if(errorOffset == *pOffset)
            {
                numErrorRetries++;
            }
            else
            {
                numErrorRetries = 0;
            }
            errorOffset = *pOffset;
            if(numErrorRetries <= MAX_ERROR_RETRIES)
            {
                char filename[PATH_MAX];

                close(*pFileIn);
                sprintf(filename, "%s/part%02d.spts", gpCurrentDirectory, *gpCurrentFileIndex);
                *pFileIn = open(filename, O_RDONLY);
                if(*pFileIn < 0)
                {
                    /* Just return. */
                    complete = 1;
                }
                else
                {
                    int offset;
                    
                    /* Seek to the current position +- 1 TS 188 byte packet. */
                    if(direction == direction_forwards)
                    {
                        (*pOffset) += 188;
                    }
                    else
                    {
                        (*pOffset) -= 188;
                        if(*pOffset < 0)
                        {
                            *pOffset = 0;
                        }
                    }
                    offset = (int)lseek(*pFileIn, *pOffset, SEEK_SET);
                    eprintf("Trick Mode: Trick Mode data error, reopening %s at %d and retry\n",
                        filename, *pOffset);
                }
            }
            else
            {
                eprintf("Trick Mode: Trick Mode hit maximum retries, exiting\n");
                complete = 1;
            }
        }
        else
        {
            complete = 1;
        }
    }

    dprintf("%s: returned:%d offset:%d fileIndex:%d\n", __FUNCTION__, retValue, *pOffset, *pWhichIndex);
    return retValue;
}

void trickMode_setSpeed(stb810_trickModeSpeed speed)
{
    dprintf("%s: speed:%d\n", __FUNCTION__, speed);
    phStbMpegTsTrickMode_SetSpeed((phStbMpegTsTrickMode_Speed_t)speed);
    dprintf("%s: done\n", __FUNCTION__);
}

#endif // #ifdef STB82

