
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

#include "downloader.h"
#include "defines.h"
#include "debug.h"
#include "output.h"
#include "sem.h"
#include <platform.h>

#define _FILE_OFFSET_BITS 64 /* for curl_off_t magic */
#include <curl/curl.h>

#include <directfb.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define CHUNK_SIZE           (10*1024)
#define DOWNLOAD_POOL_SIZE   (45)
//#define DOWNLOAD_POOL_SIZE   (10)	// orig
#define DNLD_PATH            "/tmp/XXXXXXXX/"
#define DNLD_PATH_LENGTH     (1+3+1+8+1)
#define DNLD_CONNECT_TIMEOUT (5)
#define DNLD_TIMEOUT         (20)

/***********************************************
* LOCAL TYPEDEFS                               *
************************************************/

typedef struct __curlDownloadInfo_t
{
	const char *url;
	int    timeout;
	char  *filename;
	size_t filename_size;       // capacity of filename buffer
	size_t quota;               // limit of downloaded data
	downloadCallback pCallback; // called after succesfull download
	void  *pArg;

	FILE  *out_file;
	size_t write_size;
	int    index;               // Index of download in pool
	threadHandle_t thread;
	int  stop_thread;
} curlDownloadInfo_t;

/******************************************************************
* EXPORTED DATA                                                   *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static curlDownloadInfo_t downloader_pool[DOWNLOAD_POOL_SIZE];
static int gstop_downloads = 0;
static pmysem_t  downloader_semaphore;

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static size_t downloader_headerCallback(char* ptr, size_t size, size_t nmemb, void* userp);
static size_t downloader_writeCallback(char *buffer, size_t size, size_t nmemb, void *userp);
static void downloader_acquireFileName(char *filename, curlDownloadInfo_t *info);
static DECLARE_THREAD_FUNC(downloader_thread_function);
static int downloader_exec(curlDownloadInfo_t* info);

static void downloader_free( int index );
int progress_func(void* ptr, double total_downloaded_size, double now_downloaded_size, double total_uploaded_size, double now_uploaded_size);

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

void downloader_init()
{
	mysem_create(&downloader_semaphore);
	system("rm -rf /tmp/XX*"); // clean up previous downloads in case of crash
}

static void downloader_acquireFileName(char *filename, curlDownloadInfo_t *info)
{
	char *name_end;
	size_t filename_length;
	/*int offset, i;
	char *dot = NULL;*/

	//dprintf("%s: '%s'\n", __FUNCTION__, filename);

	if( *filename == '"' )
		filename++;
	name_end = index(filename, '"');
	if( !name_end )
		name_end = index( filename, ';' );
	filename_length = name_end ? (size_t)(name_end - filename) : strlen(filename);
	if(filename_length+DNLD_PATH_LENGTH >= info->filename_size )
	{
		filename_length = info->filename_size - DNLD_PATH_LENGTH - 1;
		/*dot = rindex(filename,'.'); // moving file extension
		if( dot )
		{
			offset = name_end-dot;
			if( filename_length < offset )
			{
				filename_length = dot-filename;
			}
		}*/
	}
	/*if( dot )
	{
		strncpy( info->filename, filename, filename_length - offset );
		strncpy( &info->filename[filename_length - offset], dot, offset);
	} else */
	{
		strncpy( &info->filename[DNLD_PATH_LENGTH], filename, filename_length );
		dprintf("%s: Acquired '%s'\n", __FUNCTION__, info->filename);
	}
	info->filename[DNLD_PATH_LENGTH+filename_length] = 0;
}

static size_t downloader_headerCallback(char* buffer, size_t size, size_t nmemb, void* userp)
{
	curlDownloadInfo_t *info = (curlDownloadInfo_t *)userp;
	char *filename;

	//dprintf("%s %s\n", __FUNCTION__, buffer);
	if( info )
	{
		if( strncasecmp(buffer, "Content-Type: ", sizeof("Content-Type: ")-1) )
		{
			filename = strstr(buffer, "name=");
			if( filename )
			{
				filename += 5;
				downloader_acquireFileName(filename, info);
			}
		} else if( strncasecmp(buffer, "Content-Disposition: ", sizeof("Content-Disposition: ")-1) )
		{
			filename = strstr(buffer, "filename=");
			if( filename )
			{
				filename += 9;
				downloader_acquireFileName(filename, info);
			}
		}
	}
	return size*nmemb;
}

static size_t downloader_writeCallback(char *buffer, size_t size, size_t nmemb, void *userp)
{
	curlDownloadInfo_t *info = (curlDownloadInfo_t*)userp;
	size_t read_size = 0;
	size_t write_size;
	size_t offset;
	size_t chunk_size,chunk_offset;

	if( info != NULL && (info->quota == 0 || info->write_size < info->quota) )
	{
		read_size = size*nmemb;
		if( read_size + info->write_size > info->quota )
		{
			read_size = info->quota-info->write_size;
		}
		offset = 0;
		chunk_size	=	CHUNK_SIZE;
		while( offset < read_size )
		{
			chunk_offset	=	0;
			if(chunk_size>(read_size-offset))
				chunk_size	=	(read_size-offset);
			while(chunk_offset<chunk_size)
			{
				usleep(1000);//1ms
				write_size = fwrite(&buffer[offset], 1, chunk_size-chunk_offset, info->out_file);
				if(write_size>0)
				{
					offset				+=	write_size;
					chunk_offset		+=	write_size;
					info->write_size	+=	write_size;
				}
				if(gstop_downloads || info->stop_thread)
					break;
			}
			if(gstop_downloads || info->stop_thread)
				break;
		}
	}
	return read_size;
}

static int downlader_guessNameFromContentType(const char* content_type, char *filename, const size_t filename_size)
{
	if( strcmp(content_type, "image/jpeg") == 0 && filename_size > 5 )
	{
		strcpy(filename, "1.jpg");
		return 0;
	} else if( strcmp(content_type, "image/png") == 0 && filename_size > 5 )
	{
		strcpy(filename, "1.png");
		return 0;
	} else
		return -1;
}

static int downloader_exec(curlDownloadInfo_t* info)
{
	CURL 			*curl;
	CURLcode 		res;
	char proxy[32];
	char login[512];
	char *login_ptr;
	char errbuff[CURL_ERROR_SIZE];
	
	curl = curl_easy_init();
	if(!curl)
		return  -1;

	getParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", "", proxy);
	if( proxy[0] != 0 )
	{
		getParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", "", login);
		if( login[0] != 0 )
		{
			login_ptr = &login[strlen(login)+1];
			getParam(BROWSER_CONFIG_FILE, "HTTPProxyPasswd", "", login_ptr);
			if( *login_ptr != 0 )
			{
				login_ptr[-1] = ':';
			}
		}
	}

	if(*info->filename == 0)
	{
		strncpy( info->filename, DNLD_PATH, DNLD_PATH_LENGTH-1 );
		info->filename[DNLD_PATH_LENGTH-1] = 0;
		info->filename[DNLD_PATH_LENGTH]   = 0;
		if( mkdtemp( info->filename ) == NULL )
		{
			eprintf("downloader: Failed to create temp dir '%s': %s\n", info->filename, strerror(errno));
			goto failure;
		}
		info->filename[DNLD_PATH_LENGTH-1] = '/';
		//dprintf("%s: Downloading to temp dir: '%s'\n", __FUNCTION__, info->filename);		

		curl_easy_setopt(curl, CURLOPT_URL, info->url);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuff);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, info->timeout > 0 ? info->timeout : DNLD_CONNECT_TIMEOUT );
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, info->timeout > 0 ? info->timeout : DNLD_TIMEOUT );
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, downloader_headerCallback);
		if( proxy[0] != 0 )
		{
			curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
			if( login[0] != 0 )
			{
				curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, login);
			}
		}

		res = curl_easy_perform(curl);
		if(res != CURLE_OK && res != CURLE_WRITE_ERROR && proxy[0] != 0 )
		{
			dprintf("downloader: Retrying download without proxy (res=%d)\n", res);
			proxy[0] = 0;
			login[0] = 0;
			curl_easy_setopt(curl, CURLOPT_URL, info->url);
			curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuff);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
			curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
			curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, info->timeout > 0 ? info->timeout : DNLD_CONNECT_TIMEOUT );
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, info->timeout > 0 ? info->timeout : DNLD_TIMEOUT );
			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
			curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
			curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, downloader_headerCallback);
			curl_easy_setopt(curl, CURLOPT_PROXY, proxy);
			res = curl_easy_perform(curl);
		}
		if(res != CURLE_OK && res != CURLE_WRITE_ERROR)
		{
			eprintf("downloader: Failed to get header from '%s': %s\n", info->url, errbuff);
			goto failure;
		}
		if( info->filename[DNLD_PATH_LENGTH] == 0 )
		{
			login_ptr = rindex( info->url, '/')+1;
			if( *login_ptr != 0 )
			{
				size_t name_length = strlen( info->url ) - (login_ptr - info->url);
				char *ptr = index( login_ptr, '?' );
				if( ptr ) name_length = (ptr - login_ptr);
				if( (ptr = index(login_ptr, '.')) == NULL || ptr >= login_ptr+name_length )
				{
					char *content_type;
					if( CURLE_OK != curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type) ||
					    downlader_guessNameFromContentType(content_type, &info->filename[DNLD_PATH_LENGTH], info->filename_size - DNLD_PATH_LENGTH) != 0 )
					{
						info->filename[DNLD_PATH_LENGTH] = '0';
						info->filename[DNLD_PATH_LENGTH+1] = 0;
					}
				} else
				{
					if( DNLD_PATH_LENGTH + name_length >= info->filename_size )
					{
						int offset   = DNLD_PATH_LENGTH + name_length - info->filename_size + 1;
						login_ptr   += offset;
						name_length -= offset;
					}
					strncpy(&info->filename[DNLD_PATH_LENGTH], login_ptr, name_length);
					info->filename[DNLD_PATH_LENGTH+name_length] = 0;
				}
			} else
			{
				char *content_type;
				if( CURLE_OK != curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type) ||
				    downlader_guessNameFromContentType(content_type, &info->filename[DNLD_PATH_LENGTH], info->filename_size - DNLD_PATH_LENGTH) != 0 )
				{
					info->filename[DNLD_PATH_LENGTH] = '0';
					info->filename[DNLD_PATH_LENGTH+1] = 0;
				}
			}
		}
	}

	dprintf("downloader: Performing download to '%s'\n", info->filename);	
	if( !(info->out_file = fopen( info->filename, "w" )) )
	{
		eprintf("downloader: Failed to open temp file '%s' for writing!\n", info->filename);
		goto failure;
	}
	info->write_size = 0;

	curl_easy_setopt(curl, CURLOPT_URL, info->url);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuff);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, info->timeout > 0 ? info->timeout : DNLD_CONNECT_TIMEOUT);
	//curl_easy_setopt(curl, CURLOPT_TIMEOUT, info->timeout > 0 ? info->timeout : DNLD_TIMEOUT);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
	curl_easy_setopt(curl, CURLOPT_NOBODY, 0L);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, info);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, downloader_writeCallback);
	curl_easy_setopt(curl, CURLOPT_PROXY, proxy);

	if( login[0] != 0 )
	{
		curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, login);
	}

	res = curl_easy_perform(curl);
	if(res != CURLE_OK && res != CURLE_WRITE_ERROR)
	{
		eprintf("downloader: Failed to download '%s': %s\n", info->url, errbuff);
		goto failure;
	}
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res);
	if ( res != 200 )
	{
		eprintf("downloader: Failed to download '%s': wrong response code %d\n", info->url, res);
		goto failure;
	}
	//double 			contentLength = 0;
	//res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLength);
	//if(res)
	//{
	//	eprintf("downloader: Failed to download '%s': can't get content length (error %d)\n", info->url, res);
	//	goto failure;
	//}

	curl_easy_cleanup(curl);
	fclose(info->out_file);
	info->out_file = NULL;

	return 0;

failure:
	curl_easy_cleanup(curl);
	if( info->out_file )
	{
		fclose(info->out_file);
		info->out_file = NULL;
	}
	unlink( info->filename );

	return -1;
}

int downloader_get(const char* url, int timeout, char *filename, size_t fn_size, size_t quota)
{
	curlDownloadInfo_t info;
	int res;

	if( !url || !filename || fn_size <= DNLD_PATH_LENGTH )
		return -2;

	info.url = url;
	info.timeout = timeout;
	info.filename = filename;
	info.filename_size = fn_size;
	info.quota = quota;
	info.pCallback = NULL;
	info.stop_thread = 0;
	info.index = -1;

	res = downloader_exec(&info);
	return res;
}

#if 0
int downloader_start(char* url, size_t quota, char *filename, size_t fn_size, void *pArg, downloadCallback pCallback)
{
	curlDownloadInfo_t info;

	if( !url || !filename || fn_size <= DNLD_PATH_LENGTH)
		return -2;

	info.url = url;
	info.timeout = 0;
	info.filename = filename;
	info.filename_size = fn_size;
	info.quota = quota;
	info.pArg = pArg;
	info.pCallback = pCallback;
	info.stop_thread = 0;
	info.index = -1;

	CREATE_THREAD(info.thread, downloader_thread_function, (void*)&info);

	return info.thread != NULL ? 0 : -1;
}
#endif

void downloader_cleanupTempFile(char *file)
{
	unlink( file );
	file[DNLD_PATH_LENGTH-1] = 0;
	rmdir( file );
}

void downloader_cleanup()
{
	int i, bRunning;
	gstop_downloads = 1;
	do
	{
		usleep(10000);
		bRunning = 0;
		for( i = 0; i < DOWNLOAD_POOL_SIZE; i++ )
		{
			if( downloader_pool[i].thread != NULL )
			{
				bRunning = 1;
				break;
			}
		}
	} while(bRunning);
	mysem_destroy(downloader_semaphore);
}

static DECLARE_THREAD_FUNC(downloader_thread_function)
{
	curlDownloadInfo_t *info = (curlDownloadInfo_t *)pArg;
	if( !info )
		return NULL;

	downloader_exec(info);

	if( info->pCallback )
	{			
		info->pCallback( info->index, info->pArg );
		// info->filename and info->url shouldn't be used after this
	}

	downloader_free( info->index );

	DESTROY_THREAD(info->thread);
	return NULL;
}

/**
 * @brief Free download info from pool (stops download if it is still active).
 *
 * @param  index	I	Index of download in pool
 */
static void downloader_free( int index )
{
	mysem_get(downloader_semaphore);
	downloader_pool[index].url = NULL;
	downloader_pool[index].stop_thread = 1;
	mysem_release(downloader_semaphore);
}

int downloader_find(const char *url)
{
	int i;
	mysem_get(downloader_semaphore);
	for( i = 0; i < DOWNLOAD_POOL_SIZE; i++ )
	{
		if( downloader_pool[i].url != NULL && strcasecmp( url, downloader_pool[i].url ) == 0 )
		{
			mysem_release(downloader_semaphore);
			return i;
		}
	}
	mysem_release(downloader_semaphore);
	return -1;
}

int  downloader_push(const char *url, char *filename,  size_t fn_size, size_t quota, downloadCallback pCallback, void *pArg )
{
	int index;

	if( !url || !filename || fn_size <= DNLD_PATH_LENGTH)
		return -2;

	mysem_get(downloader_semaphore);
	for( index = 0; index < DOWNLOAD_POOL_SIZE; index++ )
	{
		if( downloader_pool[index].url == NULL )
		{
			//eprintf("downloader: pushing %d, '%s'\n", index, url);
			
			downloader_pool[index].url = url;
			downloader_pool[index].timeout = 0;
			downloader_pool[index].filename = filename;
			downloader_pool[index].filename_size = fn_size;
			downloader_pool[index].quota = quota;
			downloader_pool[index].pArg = pArg;
			downloader_pool[index].pCallback = pCallback;
			downloader_pool[index].index = index;
			downloader_pool[index].stop_thread = 0;
			CREATE_THREAD( downloader_pool[index].thread, downloader_thread_function, (void*)&downloader_pool[index] );
			if( downloader_pool[index].thread == NULL )
			{
				downloader_pool[index].url = NULL;
				mysem_release(downloader_semaphore);
				return -1;
			}
			mysem_release(downloader_semaphore);
			return index;
		}
	}
	mysem_release(downloader_semaphore);
	eprintf("downloader: Can't start download of '%s': pool is full\n", url);
	return -1;
}

int  downloader_getInfo( int index, char **url, char **filename, size_t *fn_size, size_t *quota)
{
	if( index < 0 || index >= DOWNLOAD_POOL_SIZE || downloader_pool[index].url == NULL )
		return -1;

	mysem_get(downloader_semaphore);
	if( url )
		*url = (char*)downloader_pool[index].url;
	if( filename )
		*filename = downloader_pool[index].filename;
	if( fn_size )
		*fn_size = downloader_pool[index].filename_size;
	if( quota )
		*quota = downloader_pool[index].quota;
	mysem_release(downloader_semaphore);

	return 0;
}
