
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

#include "defines.h"
#include "debug.h"
#include "interface.h"
#include "smil.h"
#include "output.h"
#include "media.h"
#include "l10n.h"

#include <curl/curl.h>

#include <string.h>
#include <stdio.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define DATA_BUFF_SIZE (10*1024)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static char* smil_getLastURL(int field, void* pArg);
static int smil_inputURL(interfaceMenu_t *pMenu, char *value, void* pArg);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static char smil_lasturl[MAX_URL] = { 0 };//"http://hwcdn.net/h8c3r4z3/fls/194083-antenai.smil";
static char smil_rtmpurl[MAX_URL] = { 0 };
static char errbuff[CURL_ERROR_SIZE];

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

int smil_enterURL(interfaceMenu_t *pMenu, void* pArg)
{

	interface_getText(pMenu,_T("ENTER_CUSTOM_URL"), "\\w+", smil_inputURL, smil_getLastURL, inputModeABC, pArg);

	return 0;
}

static char* smil_getLastURL(int field, void* pArg)
{
	return field == 0 ? smil_lasturl : NULL;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	int len = strlen((char*)userp);

	if (len+size*nmemb >= DATA_BUFF_SIZE)
	{
		size = 1;
		nmemb = DATA_BUFF_SIZE-len-1;
	}

	memcpy(&((char*)userp)[len], buffer, size*nmemb);
	((char*)userp)[len+size*nmemb] = 0;
	return size*nmemb;
}

static int smil_inputURL(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	CURLcode ret;
	CURL *hnd;
	char data[DATA_BUFF_SIZE];
	char *content_type;
	int code, length;
	char proxy[32];
	char login[512];
	char *login_ptr;
	data[0] = 0;
	errbuff[0] = 0;

	if( value == NULL)
		return 1;

	strcpy( smil_lasturl, value );

	if (strncasecmp(value, URL_HTTP_MEDIA,  sizeof(URL_HTTP_MEDIA)-1) != 0 &&
		strncasecmp(value, URL_HTTPS_MEDIA, sizeof(URL_HTTPS_MEDIA)-1) != 0)
	{
		interface_showMessageBox(_T("ERR_INCORRECT_PROTO"), thumbnail_error, 0);
		return -1;
	}

	hnd = curl_easy_init();

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, data);
	curl_easy_setopt(hnd, CURLOPT_URL, value);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, errbuff);
	curl_easy_setopt(hnd, CURLOPT_CONNECTTIMEOUT, 5);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 15);
	getParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", "", proxy);
	if( proxy[0] != 0 )
	{
		curl_easy_setopt(hnd, CURLOPT_PROXY, proxy);
		getParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", "", login);
		if( login[0] != 0 )
		{
			login_ptr = &login[strlen(login)+1];
			getParam(BROWSER_CONFIG_FILE, "HTTPProxyPasswd", "", login_ptr);
			if( *login_ptr != 0 )
			{
				login_ptr[-1] = ':';
			}
			curl_easy_setopt(hnd, CURLOPT_PROXYUSERPWD, login);
		}
	}

	ret = curl_easy_perform(hnd);

	if (ret == 0)
	{
		curl_easy_getinfo(hnd, CURLINFO_RESPONSE_CODE, &code);
		if (code != 200)
		{
			ret = code > 0 ? code : -1;
			interface_showMessageBox(_T("ERR_SERVICE_UNAVAILABLE"), thumbnail_error, 0);
		} else
		{
			length = strlen(data);
			if( CURLE_OK != curl_easy_getinfo(hnd, CURLINFO_CONTENT_TYPE, &content_type) )
			{
				content_type = NULL;
			}
			dprintf("%s: Content-Type: %s\n", __FUNCTION__, content_type);
			//dprintf("%s: streams data %d:\n%s\n\n", __FUNCTION__, length, data);
		}
	}
	else
		eprintf("SMIL: Failed(%d) to get '%s' with message:\n%s\n", ret, value, errbuff);

	if ( ret == 0 )
	{
		if((ret = smil_parseRTMPStreams((const char*)data, smil_rtmpurl, sizeof(smil_rtmpurl))) == 0 )
		{
			dprintf("%s: Playing '%s'\n", __FUNCTION__, smil_rtmpurl);
			media_playURL( screenMain, smil_rtmpurl, NULL, NULL );
		}
		else
		{
			eprintf("SMIL: Failed to parse SMIL at '%s'\n", value);
			interface_showMessageBox(_T("ERR_SERVICE_UNAVAILABLE"), thumbnail_error, 0);
		}
	} else
	{
		eprintf("SMIL: Failed to get SMIL playlist from '%s'\n", value);
		interface_showMessageBox(_T("ERR_SERVICE_UNAVAILABLE"), thumbnail_error, 0);
	}

	curl_easy_cleanup(hnd);

	return ret;
}
