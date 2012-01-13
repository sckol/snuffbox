
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


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <fcntl.h>

#include "debug.h"
#include "sem.h"

/***********************************************
 LOCAL MACROS                                  *
************************************************/

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

/**************************************************************************
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

/********************************************************************************/
int mysem_get(pmysem_t semaphore)
{
	if(semaphore == NULL)
	{
		eprintf("Error: while trying get semaphore\n");
		return -1;
	}
	int rc;
	if ( (rc = pthread_mutex_lock(&(semaphore->mutex)))!=0 )
	{
		return rc;
	}

	while ( semaphore->semCount <= 0 )
	{
		rc = pthread_cond_wait(&(semaphore->condition), &(semaphore->mutex));
		if ( (rc!=0) && (errno != EINTR) )
			break;
	}
	semaphore->semCount--;

	if ( (rc = pthread_mutex_unlock(&(semaphore->mutex)))!=0 )
	{
		return rc;
	}

	return 0;
}

/********************************************************************************/
int mysem_release(pmysem_t semaphore)
{
	int rc;
	if(semaphore == NULL)
	{
		eprintf("Error: while trying release semaphore\n");
		return -1;
	}
	if ( (rc = pthread_mutex_lock(&(semaphore->mutex)))!=0 )
	{
		return rc;
	}

	semaphore->semCount ++;

	if ( (rc = pthread_mutex_unlock(&(semaphore->mutex)))!=0 )
	{
		return rc;
	}

	if ( (rc = pthread_cond_signal(&(semaphore->condition)))!=0 )
	{
		return rc;
	}

	return 0;
}

/********************************************************************************/
int mysem_create(pmysem_t* semaphore)
{
	int rc;
	pmysem_t thisSemaphore;
	thisSemaphore = (pmysem_t) dmalloc(sizeof(mysem_t));

	if ( (rc = pthread_mutex_init(&(thisSemaphore->mutex), NULL))!=0 )
	{
		dfree(thisSemaphore);
		return rc;
	}

	if ( (rc = pthread_cond_init(&(thisSemaphore->condition), NULL))!=0 )
	{
		pthread_mutex_destroy( &(thisSemaphore->mutex) );
		dfree(thisSemaphore);
		return rc;
	}

	thisSemaphore->semCount = 1;
	*semaphore = thisSemaphore;
	return 0;
}

/********************************************************************************/
int mysem_destroy(pmysem_t semaphore)
{
	if(semaphore == NULL)
	{
		eprintf("Error: while trying release semaphore\n");
		return -1;
	}
	pthread_mutex_destroy(&(semaphore->mutex));
	pthread_cond_destroy(&(semaphore->condition));
	dfree(semaphore);
	return 0;
}

#ifdef STB225
/********************************************************************************/
int32_t event_send(pmysem_t semaphore)
{    
    int rc;

    rc = pthread_mutex_lock(&(semaphore->mutex));
    if (rc != 0)
    {
        //DBG_PRINT2(MY_DBG_UNIT, DBG_WARNING, "%s ERROR: pthread_mutex_lock => %d!\n", __FUNCTION__, rc);
        return rc;
    }

    semaphore->semCount = 1;
    if ((rc = pthread_cond_signal(&(semaphore->condition)))!=0)
    {
        //DBG_PRINT2(MY_DBG_UNIT, DBG_WARNING, "%s ERROR: pthread_cond_signal => %d!\n", __FUNCTION__, rc);
        pthread_mutex_unlock(&(semaphore->mutex));
        return rc;
    }

    rc = pthread_mutex_unlock(&(semaphore->mutex));
    if (rc != 0)
    {
        //DBG_PRINT2(MY_DBG_UNIT, DBG_WARNING, "%s ERROR: pthread_mutex_unlock => %d!\n", __FUNCTION__, rc);
        return rc;
    }

    return 0;
}

int32_t event_wait(pmysem_t semaphore)
{
    int32_t rc;
    int locked = 0;

    //DBG_PRINT1(MY_DBG_UNIT, DBG_LEVEL_1, "%s event wait\n", __FUNCTION__);
    if (pthread_mutex_trylock( &semaphore->mutex ) == 0) 
    {
        /* Check for a pending event */
        if (semaphore->semCount == 1) 
        {
            /* Clear the event */
            semaphore->semCount = 0;
            pthread_mutex_unlock ( &semaphore->mutex );
            return 0;
        }
        locked = 1;
    }

    if (!locked)
    {
        rc = pthread_mutex_lock(&(semaphore->mutex));
        if (rc != 0)
        {
            //DBG_PRINT2(MY_DBG_UNIT, DBG_WARNING, "%s ERROR: pthread_mutex_lock => %d!\n", __FUNCTION__, rc);
            return rc;
        }
    }

    if (semaphore->semCount == 0)
    {
        //DBG_PRINT1(MY_DBG_UNIT, DBG_LEVEL_1, "%s semCount==0, so waiting!\n", __FUNCTION__);

        /* Wait for the event to occur */
        rc = pthread_cond_wait( &semaphore->condition,
                                &semaphore->mutex);

        //DBG_PRINT1(MY_DBG_UNIT, DBG_LEVEL_1, "%s semCount==0, Got event\n", __FUNCTION__);
    }

    /* Clear the event */
    semaphore->semCount = 0;

    rc = pthread_mutex_unlock(&(semaphore->mutex));
    if (rc != 0)
    {
        //DBG_PRINT2(MY_DBG_UNIT, DBG_WARNING, "%s ERROR: pthread_mutex_unlock => %d!\n", __FUNCTION__, rc);
        return rc;
    }

    return 0;
}

/********************************************************************************/
int32_t event_create(pmysem_t* semaphore)
{
    int32_t retValue;

    retValue = mysem_create(semaphore);

    if (retValue == 0) 
    {
        (*semaphore)->semCount = 0;
    }

    return retValue;
}

/********************************************************************************/
int32_t event_destroy(pmysem_t semaphore)
{
    return mysem_destroy(semaphore);
}

#endif
