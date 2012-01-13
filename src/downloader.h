#ifndef __DOWNLOADER_H
#define __DOWNLOADER_H

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

/****************
* INCLUDE FILES *
*****************/

#include <stdio.h>

/*******************
* EXPORTED MACROS  *
********************/

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

/** Callback to be executed after download.
  First param is index of download in pool, second is user data specified in downloader_push
*/
typedef void (*downloadCallback)(int,void*);

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/
 
/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#ifdef __cplusplus
extern "C" {
#endif

void downloader_init();

/**
 * @brief Download a file in blocking mode. Downloaded files saved by default to /tmp/XXXXXXXX/<filename>.
 *
 * @param[in]   url
 * @param[in]   timeout   Download timeout in seconds
 * @param[out]  filename  Pointer to buffer to store downloaded file name. If buffer is initiated with some string, it will be used as filename as is (no autodetection)
 * @param[in]   fn_size   Size of filename buffer
 * @param[in]   quota     If greater than 0, downloaded file size will be limited to specified value
 *
 * @retval int	0 - success, non-zero - failure
 */
int  downloader_get(const char* url, int timeout, char *filename, size_t fn_size, size_t quota);

/**
 * @brief Remove downloaded file and temporal directory.
 *
 * @param[out] file Path to downloaded file. Will be modified during cleanup!
 */
void downloader_cleanupTempFile(char *file);

/**
 * @brief Check URL for being in download pool.
 *
 * @param[in]  url
 *
 * @retval int  Index of download in pool, -1 if not found
 */
int downloader_find(const char *url);

/**
 * @brief Add new URL to download pool. Not checking for same URL in pool.
 *
 * @param[in]   url
 * @param[out]  filename   Pointer to buffer to store downloaded file name. If buffer is initiated with some string, it will be used as filename as is (no autodetection).
 * @param[in]   fn_size    Size of filename buffer
 * @param[in]   quota      If greater than 0, downloaded file size will be limited to specified value
 * @param[in]   pCallback  Callback function to be called after download. Callback function may (and should) free buffers for URL and filename.
 * @param[in]   pArg       User data to be passed to callback function
 *
 * @retval int Index of download in pool, -1 on error
 */
int  downloader_push(const char *url, char *filename,  size_t fn_size, size_t quota, downloadCallback pCallback, void *pArg );

/**
 * @brief Terminate all downloads and free data.
 */
void downloader_cleanup();

/**
 * @brief Get download info from pool.
 *
 * @param[in]   index     Index of download in pool
 * @param[out]  url       URL of downloading file. Should not be modified.
 * @param[out]  filename  Pointer to filename buffer
 * @param[out]  fn_size   Size of filename buffer
 * @param[out]  quota     File size limit
 *
 * @retval int	0 - success, non-zero - failure
 */
int  downloader_getInfo( int index, char **url, char **filename, size_t *fn_size, size_t *quota);

#ifdef __cplusplus
}
#endif

#endif // __DOWNLOADER_H
