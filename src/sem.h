#if !defined(__SEM_H)
	#define __SEM_H

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

	#include <pthread.h>

/*******************
* EXPORTED MACROS  *
********************/

/*********************
* EXPORTED TYPEDEFS  *
**********************/

typedef struct
{
	pthread_mutex_t mutex;
	pthread_cond_t  condition;
	int             semCount;
}mysem_t, *pmysem_t;

/*******************
* EXPORTED DATA    *
********************/

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif
/* Obtains the semaphore. */
extern int mysem_get(pmysem_t semaphore);

/* Releases the semaphore. */
extern int mysem_release(pmysem_t semaphore);

/* Creates a semaphore object. */
extern int mysem_create(pmysem_t* semaphore);

/* Destroys a semaphore object. */
extern int mysem_destroy(pmysem_t semaphore);

#ifdef STB225
int32_t event_send(pmysem_t semaphore);
int32_t event_wait(pmysem_t semaphore);
int32_t event_destroy(pmysem_t semaphore);
int32_t event_create(pmysem_t* semaphore);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __SEM_H      Do not add any thing below this line */
