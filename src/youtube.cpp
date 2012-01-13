
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

#include "youtube.h"
#include "debug.h"
#include "app_info.h"
#include "StbMainApp.h"
#include "output.h"
#include "stb_resource.h"
#include "l10n.h"
#include "xmlconfig.h"
#include "media.h"
#if (defined STB225)
#include "sem.h"
#endif
#include "gfx.h"
#include "interface.h"
#include "playlist.h"

#define _FILE_OFFSET_BITS 64 /* for curl_off_t magic */
#include <curl/curl.h>
#include <tinyxml.h>

#include <common.h>
#include <pthread.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define YOUTUBE_ID_LENGTH         (12)
#define YOUTUBE_LINK_SIZE         (48)
#define YOUTUBE_LIST_BUFFER_SIZE  (32*1024)
#define YOUTUBE_INFO_BUFFER_SIZE  (16*1024)
#define YOUTUBE_MAX_LINKS (32)
#define YOUTUBE_LINKS_PER_PAGE (4)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

typedef struct
{
	char video_id[YOUTUBE_ID_LENGTH];
	char thumbnail[MAX_URL];
} youtubeVideo_t;

typedef struct
{
	char   *data;
	size_t  size;
	size_t  pos;
} curlBufferInfo_t;

typedef struct
{
	char search[MAX_FIELD_PATTERN_LENGTH]; /**< Search request in readable form. If empty, standard feed is used. */
	youtubeVideo_t videos[YOUTUBE_MAX_LINKS];
	char current_id[YOUTUBE_ID_LENGTH];
	size_t count;
	size_t index;

	pthread_t search_thread;
} youtubeInfo_t;

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static youtubeInfo_t youtubeInfo;

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceListMenu_t YoutubeMenu;

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static int youtube_streamChange(interfaceMenu_t *pMenu, void *pArg);
static int youtube_fillMenu( interfaceMenu_t* pMenu, void *pArg );
static int youtube_exitMenu( interfaceMenu_t* pMenu, void *pArg );
static char *youtube_getLastSearch(int index, void* pArg);
static int youtube_videoSearch(interfaceMenu_t *pMenu, void* pArg);
static int youtube_startVideoSearch(interfaceMenu_t *pMenu, char *value, void* pArg);
static int youtube_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
static void youtube_menuDisplay(interfaceMenu_t *pMenu);
static void *youtube_MenuVideoSearchThread(void* pArg);

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	curlBufferInfo_t *info = (curlBufferInfo_t*)userp;
	size_t wholesize = size*nmemb;

	if (info->pos+wholesize >= info->size)
	{
		wholesize = info->size - info->pos - 1;
	}

	memcpy(&info->data[info->pos], buffer, wholesize);
	info->pos += wholesize;
	info->data[info->pos] = 0;
	return wholesize;
}

int youtube_getVideoList(const char *url, youtubeEntryHandler pCallback, int page)
{
	CURLcode ret;
	CURL *hnd;
	char proxy[32];
	char login[512];
	char *str;
	xmlConfigHandle_t list;
	xmlConfigHandle_t item;
	TiXmlNode *entry;
	TiXmlNode *child;
	TiXmlElement *elem;
	const char *video_name, *video_type, *video_url, *thumbnail;
	static char curl_data[YOUTUBE_LIST_BUFFER_SIZE];
	static char err_buff[CURL_ERROR_SIZE];
	static curlBufferInfo_t buffer;
	buffer.data = curl_data;
	buffer.size = sizeof(curl_data);
	buffer.pos  = 0;

	if( url == 0 || pCallback == 0 )
		return -2;

	curl_data[0] = 0;

	hnd = curl_easy_init();

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, err_buff);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 2);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 1);
	curl_easy_setopt(hnd, CURLOPT_CONNECTTIMEOUT, 15);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 15);
	getParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", "", proxy);
	if( proxy[0] != 0 )
	{
		curl_easy_setopt(hnd, CURLOPT_PROXY, proxy);
		getParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", "", login);
		if( login[0] != 0 )
		{
			str = &login[strlen(login)+1];
			getParam(BROWSER_CONFIG_FILE, "HTTPProxyPasswd", "", str);
			if( *str != 0 )
			{
				str[-1] = ':';
			}
			curl_easy_setopt(hnd, CURLOPT_PROXYUSERPWD, login);
		}
	}

	if (page == 1)
	{
		interface_showLoading();
		interface_displayMenu(1);
	}

	ret = curl_easy_perform(hnd);

	dprintf("%s: page %d of '%s' acquired (length %d)\n", __FUNCTION__, page, youtubeInfo.search[0] ? youtubeInfo.search : "standard feeds", buffer.pos);

	curl_easy_cleanup(hnd);

	if (ret != 0)
	{
		eprintf("Youtube: Failed to get video list from '%s': %s\n", url, err_buff);
		if (page == 1) interface_hideLoading();
		return -1;
	}
	list = xmlConfigParse( curl_data );
	if (list == NULL
		|| (item = xmlConfigGetElement(list, "feed", 0)) == NULL)
	{
		if (list)
		{
			eprintf("Youtube: parse error %s\n", ((TiXmlDocument*)list)->ErrorDesc());
			xmlConfigClose(list);
		}
		if (page == 1)interface_hideLoading();
		eprintf("Youtube: Failed to parse video list %d\n", page);
		return -1;
	}
	for ( entry = ((TiXmlNode *)item)->FirstChild(); entry != 0; entry = entry->NextSibling() )
	{
		if (entry->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(entry->Value(), "entry") == 0)
		{
			item = xmlConfigGetElement(entry, "media:group", 0);
			if( item != NULL )
			{
				video_name = (char *)xmlConfigGetText(item, "media:title");
				video_url = NULL;
				thumbnail = NULL;
				for ( child = ((TiXmlNode *)item)->FirstChild();
					child != 0 && (video_url == NULL || thumbnail == NULL);
					child = child->NextSibling() )
				{
					if (child->Type() == TiXmlNode::TINYXML_ELEMENT )
					{
						if(strcasecmp(child->Value(), "media:content") == 0)
						{
							elem = (TiXmlElement *)child;
							video_type = elem->Attribute("type");
							if( strcmp( video_type, "application/x-shockwave-flash" ) == 0)
							{
								video_url = elem->Attribute("url");
							}
						} else if(thumbnail== 0 && strcasecmp(child->Value(), "media:thumbnail") == 0)
						{
							elem = (TiXmlElement *)child;
							thumbnail = elem->Attribute("url");
						}
					}
				}
				if( video_url != NULL )
				{
					//dprintf("%s: Adding Youtube video '%s' url='%s' thumb='%s'\n", __FUNCTION__,video_name,video_url,thumbnail);
					pCallback(video_url,video_name,thumbnail);
				}
			}
		}
	}
	xmlConfigClose(list);
	if (page == 1) interface_hideLoading();
	return ret;
}

void youtube_addMenuEntry(const char *url, const char *name, const char *thumbnail)
{
	const char *str;
	if( youtubeInfo.count >= YOUTUBE_MAX_LINKS || youtubeInfo.count < 0)
		return;

	str = strstr(url, "/v/");
	if( str == NULL )
	{
		dprintf("%s: Can't find Youtube VIDEO_ID in '%s'\n", __FUNCTION__, url);
		return;
	}
	strncpy( youtubeInfo.videos[youtubeInfo.count].video_id, &str[3], 11 );
	youtubeInfo.videos[youtubeInfo.count].video_id[11] = 0;
	str = name != NULL ? name : url;
	if( thumbnail != NULL )
		strcpy( youtubeInfo.videos[youtubeInfo.count].thumbnail, thumbnail );
	else
		youtubeInfo.videos[youtubeInfo.count].thumbnail[0] = 0;

	interface_addMenuEntry( (interfaceMenu_t*)&YoutubeMenu, str, youtube_streamChange, (void*)youtubeInfo.count, thumbnail_internet );

	youtubeInfo.count++;
}

int youtube_streamStart()
{
	return youtube_streamChange( (interfaceMenu_t*)&YoutubeMenu, (void*)CHANNEL_CUSTOM );
}

static int youtube_streamChange(interfaceMenu_t *pMenu, void *pArg)
{
	CURLcode ret;
	CURL *hnd;
	char proxy[32];
	char login[512];
	char *str;
	static char url[MAX_URL];
	static char url_tmp[MAX_URL];
	static char curl_data[YOUTUBE_INFO_BUFFER_SIZE];
	static char err_buff[CURL_ERROR_SIZE];
	static curlBufferInfo_t buffer;
	char *fmt_url_map;
	//int supported_formats[] = { 18, 17, 34, 5, 0 };
	int supported_formats[] = { 34, 18, 0 };
	/* 37/1920x1080/9/0/115
	   22/1280x720/9/0/115
	   35/854x480/9/0/115
	   34/640x360/9/0/115
	   5/320x240/7/0/0 */
	char *fmt_url;
	int i;
	int len_str;
	int len_url = 0;
	int videoIndex = (int)pArg;

	if( videoIndex == CHANNEL_CUSTOM )
	{
		str = strstr( appControlInfo.mediaInfo.lastFile, "watch?v=" );
		if( str == NULL )
		{
			eprintf("%s: can't file YouTube ID in %s\n", __FUNCTION__, appControlInfo.mediaInfo.lastFile);
			interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);
			return -1;
		}
		str += 8;
		str[sizeof(youtubeInfo.current_id)-1] = 0;
		if( strlen(str) != YOUTUBE_ID_LENGTH-1 )
		{
			eprintf("%s: invalid YouTube ID %s\n", __FUNCTION__, str);
			interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);
			return -1;
		}
		memcpy( youtubeInfo.current_id, str, sizeof(youtubeInfo.current_id) );
	} else if( videoIndex < 0 || videoIndex >= youtubeInfo.count )
	{
		eprintf("%s: there is no stream %d\n", __FUNCTION__, videoIndex);
		interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);
		return -1;
	} else
	{
		memcpy( youtubeInfo.current_id, youtubeInfo.videos[videoIndex].video_id, sizeof(youtubeInfo.current_id) );
	}

	buffer.data = curl_data;
	buffer.size = sizeof(curl_data);
	buffer.pos  = 0;

	curl_data[0] = 0;
	sprintf(url,"http://www.youtube.com/get_video_info?video_id=%s%s",youtubeInfo.current_id, "&eurl=&el=detailpage&ps=default&gl=US&hl=en");
	hnd = curl_easy_init();

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(hnd, CURLOPT_URL, url);
	curl_easy_setopt(hnd, CURLOPT_ERRORBUFFER, err_buff);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 2);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(hnd, CURLOPT_SSL_VERIFYHOST, 1);
	curl_easy_setopt(hnd, CURLOPT_CONNECTTIMEOUT, 15);
	curl_easy_setopt(hnd, CURLOPT_TIMEOUT, 15);
	getParam(BROWSER_CONFIG_FILE, "HTTPProxyServer", "", proxy);
	if( proxy[0] != 0 )
	{
		curl_easy_setopt(hnd, CURLOPT_PROXY, proxy);
		getParam(BROWSER_CONFIG_FILE, "HTTPProxyLogin", "", login);
		if( login[0] != 0 )
		{
			str = &login[strlen(login)+1];
			getParam(BROWSER_CONFIG_FILE, "HTTPProxyPasswd", "", str);
			if( *str != 0 )
			{
				str[-1] = ':';
			}
			curl_easy_setopt(hnd, CURLOPT_PROXYUSERPWD, login);
		}
	}

	ret = curl_easy_perform(hnd);

	eprintf("%s: video info for %s acquired (length %d)\n", __FUNCTION__, youtubeInfo.current_id, buffer.pos);
	eprintf("%s:  YouTube URL %s\n", __FUNCTION__, url);

	curl_easy_cleanup(hnd);

	if (ret != 0)
	{
		eprintf("Youtube: Failed to get video info from '%s': %s\n", url, err_buff);
		interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);
		return 1;
	}
/*
	fmt_url_map = strstr(curl_data, "fmt_list=");
	if( fmt_url_map )
	{
		fmt_url_map += sizeof("fmt_list=")-1;
		for( str = fmt_url_map; *str && *str != '&'; str++ );
		if( *str != '&' )
			str = NULL;
		else
			*str = 0;

		if( utf8_urltomb(fmt_url_map, strlen(fmt_url_map)+1, url, sizeof(url)-1 ) > 0 )
		{
			eprintf("%s: fmt_list='%s'\n", __FUNCTION__, url);
		}
	}
*/
	//fmt_url_map = strstr(curl_data, "fmt_url_map=");
    fmt_url_map = strstr(curl_data, "url_encoded_fmt_stream_map=");
	if( fmt_url_map )
	{
		//fmt_url_map += sizeof("fmt_url_map=")-1;
		fmt_url_map += sizeof("url_encoded_fmt_stream_map=")-1;
		for( str = fmt_url_map; *str && *str != '&'; str++ );
		if( *str != '&' )
			str = NULL;
		else
			*str = 0;

		for( i = 0; supported_formats[i] != 0; i++ )
		{
			//sprintf(proxy, "%d%%7C", supported_formats[i]);
			sprintf(proxy, "itag%%3D%d", supported_formats[i]); // find format tag field
			if( (fmt_url = strstr( fmt_url_map, proxy )) != NULL )
			{
				fmt_url += strlen(proxy) + 9;// skip "%2Curl%3D"
				str = strstr( fmt_url, "%26quality" ); // find end url
				if( str )
					*str = 0;
				eprintf("%s:  URL %s\n", __FUNCTION__, fmt_url);
				if( utf8_urltomb(fmt_url, strlen(fmt_url)+1, url_tmp, sizeof(url_tmp)-1 ) < 0 )
				{
					eprintf("%s: Failed to decode '%s'\n", __FUNCTION__, fmt_url);
				} else
				{
					if( utf8_urltomb(url_tmp, strlen(url_tmp)+1, url, sizeof(url)-1 ) < 0 )
					{
						eprintf("%s: Failed to decode '%s'\n", __FUNCTION__, url_tmp);
					} else {
						eprintf("%s: selected format %d\n", __FUNCTION__, supported_formats[i]);
						break;
					}
				}
			}
		}
		if( supported_formats[i] == 0 )
		{
			interface_showMessageBox(_T("ERR_STREAM_NOT_SUPPORTED"), thumbnail_warning, 0);
			return 1;
		}
	}

	eprintf("Youtube: Playing '%s'\n", url);

	char *descr     = NULL;
	char *thumbnail = NULL;
	if( videoIndex != CHANNEL_CUSTOM )
	{
		youtubeInfo.index = videoIndex;
		if( interface_getMenuEntryInfo( (interfaceMenu_t*)&YoutubeMenu, videoIndex+1, login, sizeof(login) ) == 0 )
			descr = login;
		thumbnail = youtubeInfo.videos[videoIndex].thumbnail[0] ? youtubeInfo.videos[videoIndex].thumbnail : NULL;
		appControlInfo.playbackInfo.channel = videoIndex+1;
		appControlInfo.playbackInfo.playlistMode = playlistModeYoutube;
	} else
	{
		youtubeInfo.index = 0;
		descr     = appControlInfo.playbackInfo.description;
		thumbnail = appControlInfo.playbackInfo.thumbnail;
	}
	appControlInfo.playbackInfo.streamSource = streamSourceYoutube;
	media_playURL(screenMain, url, descr, thumbnail != NULL ? thumbnail : resource_thumbnails[thumbnail_youtube] );

	return 0;
}

int  youtube_startNextChannel(int direction, void* pArg)
{
	if( youtubeInfo.count == 0 )
		return 1;

	int indexChange = (direction?-1:1);
	int newIndex = (youtubeInfo.index + indexChange + youtubeInfo.count)%youtubeInfo.count;

	return youtube_streamChange((interfaceMenu_t*)&YoutubeMenu, (void*)newIndex);
}

int  youtube_setChannel(int channel, void* pArg)
{
	if( channel < 1 || channel > youtubeInfo.count )
		return 1;

	return youtube_streamChange((interfaceMenu_t*)&YoutubeMenu, (void*)(channel-1));
}

char *youtube_getCurrentURL()
{
	static char url[YOUTUBE_LINK_SIZE];

	if( youtubeInfo.current_id[0] == 0 )
		url[0] = 0;
	else
		snprintf(url, sizeof(url), "http://www.youtube.com/watch?v=%s", youtubeInfo.current_id);
	return url;
}

void youtube_buildMenu(interfaceMenu_t* pParent)
{
	int youtube_icons[4] = { 0, 
		statusbar_f2_info,
#ifdef ENABLE_FAVORITES
		statusbar_f3_add,
#else
		0,
#endif
		0 };

	createListMenu(&YoutubeMenu, "YouTube", thumbnail_youtube, youtube_icons, pParent,
		interfaceListMenuIconThumbnail, youtube_fillMenu, youtube_exitMenu, (void*)1);
	interface_setCustomKeysCallback((interfaceMenu_t*)&YoutubeMenu, youtube_keyCallback);
	interface_setCustomDisplayCallback((interfaceMenu_t*)&YoutubeMenu, youtube_menuDisplay);

	memset( &youtubeInfo, 0, sizeof(youtubeInfo) );
}

static int youtube_fillMenu( interfaceMenu_t* pMenu, void *pArg )
{
	int i;
	char *str;
	int page = (int) pArg;
	char url[64+MAX_FIELD_PATTERN_LENGTH];
	
	if(page == 1)
	{
		interface_clearMenuEntries( (interfaceMenu_t*)&YoutubeMenu );

		str = _T("VIDEO_SEARCH");
		interface_addMenuEntry( (interfaceMenu_t*)&YoutubeMenu, str, youtube_videoSearch, NULL, thumbnail_search );

		youtubeInfo.count = 0;
		youtubeInfo.index = 0;
	}

	if( youtubeInfo.search[0] == 0 )
		//snprintf(url, sizeof(url), "http://gdata.youtube.com/feeds/api/standardfeeds/recently_featured?format=5&max-results=%d&start-index=%d", YOUTUBE_LINKS_PER_PAGE, (page-1)*YOUTUBE_LINKS_PER_PAGE+1);
		snprintf(url, sizeof(url), "http://gdata.youtube.com/feeds/api/standardfeeds/top_rated?time=today&format=5&max-results=%d&start-index=%d", YOUTUBE_LINKS_PER_PAGE, (page-1)*YOUTUBE_LINKS_PER_PAGE+1);
	else
		//snprintf(url, sizeof(url), "http://gdata.youtube.com/feeds/api/videos?format=5&max-results=%d&start-index=%d&vq=%s", YOUTUBE_LINKS_PER_PAGE, (page-1)*YOUTUBE_LINKS_PER_PAGE+1, youtubeInfo.search);
		snprintf(url, sizeof(url), "http://gdata.youtube.com/feeds/api/videos?format=5&max-results=%d&start-index=%d&q=%s", YOUTUBE_LINKS_PER_PAGE, (page-1)*YOUTUBE_LINKS_PER_PAGE+1, youtubeInfo.search);

	youtube_getVideoList(url, youtube_addMenuEntry, page);

	if(( youtubeInfo.count == 0 ) && ( page == 1 ))
	{
		str = _T("NO_MOVIES");
		interface_addMenuEntryDisabled( (interfaceMenu_t*)&YoutubeMenu, str, thumbnail_info );
	}

	if(( youtubeInfo.search[0] == 0 ) && ( page == 1 ))
	{
		pthread_create(&youtubeInfo.search_thread, NULL, youtube_MenuVideoSearchThread, NULL);
		pthread_detach(youtubeInfo.search_thread);
	}

	return 0;
}

static int youtube_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int selectedIndex = interface_getSelectedItem((interfaceMenu_t*)&YoutubeMenu);
	char url[YOUTUBE_LINK_SIZE];
	if (selectedIndex > 0 )
	{
		switch( cmd->command )
		{
			case interfaceCommandGreen:
				snprintf(url, sizeof(url), "http://www.youtube.com/watch?v=%s", youtubeInfo.videos[selectedIndex-1].video_id);
				eprintf("Youtube: Stream %02d: '%s'\n", selectedIndex-1, url);
				interface_showMessageBox(url, -1, 0);
				return 0;
#ifdef ENABLE_FAVORITES
			case interfaceCommandYellow:
				char description[MENU_ENTRY_INFO_LENGTH];
				interface_getMenuEntryInfo( (interfaceMenu_t*)&YoutubeMenu, selectedIndex, description, sizeof(description) );
				snprintf(url, sizeof(url), "http://www.youtube.com/watch?v=%s", youtubeInfo.videos[selectedIndex-1].video_id);
				eprintf("Youtube: Add to Playlist '%s'\n", url);
				playlist_addUrl( url, description );
				return 0;
				
				break;
#endif
			default:;
		}
	}
	return 1;
}

static int youtube_exitMenu( interfaceMenu_t* pMenu, void *pArg )
{
	youtubeInfo.search[0] = 0;
	pthread_cancel( youtubeInfo.search_thread );
	return 0;
}

static int youtube_videoSearch(interfaceMenu_t *pMenu, void* pArg)
{
	interface_getText(pMenu, _T("ENTER_TITLE"), "\\w+", youtube_startVideoSearch, youtube_getLastSearch, inputModeABC, pArg);
	return 0;
}

static char *youtube_getLastSearch(int index, void* pArg)
{
	if( index == 0 )
	{
		static char last_search[sizeof(youtubeInfo.search)];
		int search_length;

		search_length = utf8_uritomb(youtubeInfo.search, strlen(youtubeInfo.search), last_search, sizeof(last_search)-1 );
		if( search_length < 0 )
			return youtubeInfo.search;
		else
		{
			last_search[search_length] = 0;
			return last_search;
		}
	} else
		return NULL;
}

static void *youtube_MenuVideoSearchThread(void* pArg)
{
	int page;
	char *str;
	interfaceMenu_t *pMenu = (interfaceMenu_t *) pArg;
	int old_count = -1;

	if(youtubeInfo.search[0] == 0)
		page = 2;
	else
		page = 1;

	for(; (( page < YOUTUBE_MAX_LINKS/YOUTUBE_LINKS_PER_PAGE + 1 ) && 
	       ( old_count != youtubeInfo.count )); page++)
	{
		pthread_testcancel();
		old_count = youtubeInfo.count;
		youtube_fillMenu( pMenu, (void *)page );
		interface_displayMenu(1);
	}
}

static int youtube_startVideoSearch(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	int search_length;
	char buf[sizeof(youtubeInfo.search)];
	int thread_create = -1;
    
	if( value == NULL )
		return 1;
	interface_hideMessageBox();
	search_length = utf8_mbtouri(value, strlen(value), buf, sizeof(buf)-1 );
	if( search_length < 0 )
	{
		interface_showMessageBox(_T("ERR_INCORRECT_URL"), thumbnail_error, 5000);
		return 1;
	}
	buf[search_length] = 0;
	strncpy(youtubeInfo.search, buf, search_length+1);
	pthread_create(&youtubeInfo.search_thread, NULL, youtube_MenuVideoSearchThread, (void *) pMenu);
	pthread_detach(youtubeInfo.search_thread);
	return 0;
}

static void youtube_menuDisplay(interfaceMenu_t *pMenu)
{
	interfaceListMenu_t *pListMenu = (interfaceListMenu_t*)pMenu;
	int i, x, y, index;
	int fh;
	int itemHeight, maxVisibleItems, itemOffset, itemDisplayIndex, maxWidth;
	int r, g, b, a;
	char *second_line;
	char entryText[MENU_ENTRY_INFO_LENGTH];

	//dprintf("%s: in\n", __FUNCTION__);

	//DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_NOFX) );

	//	gfx_drawRectangle(DRAWING_SURFACE, 0, 0, 0, 0, interfaceInfo.clientX, interfaceInfo.clientY, interfaceInfo.clientWidth, interfaceInfo.clientHeight);
	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
	DFBCHECK( DRAWING_SURFACE->SetColor(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA) );

	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX, interfaceInfo.clientY+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientWidth, interfaceInfo.clientHeight-2*INTERFACE_ROUND_CORNER_RADIUS);
	// top left corner
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientY, interfaceInfo.clientWidth-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", interfaceInfo.clientX, interfaceInfo.clientY, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 0, DSBLIT_BLEND_COLORALPHA, (interfaceAlign_t)(interfaceAlignLeft|interfaceAlignTop));
	// bottom left corner
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientY+interfaceInfo.clientHeight-INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientWidth-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", interfaceInfo.clientX, interfaceInfo.clientY+interfaceInfo.clientHeight-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 1, 0, DSBLIT_BLEND_COLORALPHA, (interfaceAlign_t)(interfaceAlignLeft|interfaceAlignTop));


	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_NOFX) );

	/* Show menu logo if needed */
	if (interfaceInfo.currentMenu->logo > 0 && interfaceInfo.currentMenu->logoX > 0)
	{
		interface_drawImage(DRAWING_SURFACE, resource_thumbnails[interfaceInfo.currentMenu->logo], 
			interfaceInfo.currentMenu->logoX, interfaceInfo.currentMenu->logoY, interfaceInfo.currentMenu->logoWidth, interfaceInfo.currentMenu->logoHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0);
	}

	pgfx_font->GetHeight(pgfx_font, &fh);

	//dprintf("%s: go through entries\n", __FUNCTION__);

	interface_listMenuGetItemInfo(pListMenu,&itemHeight,&maxVisibleItems);

	if ( pListMenu->baseMenu.selectedItem > maxVisibleItems/2 )
	{
		itemOffset = pListMenu->baseMenu.selectedItem - maxVisibleItems/2;
		itemOffset = itemOffset > (pListMenu->baseMenu.menuEntryCount-maxVisibleItems) ? pListMenu->baseMenu.menuEntryCount-maxVisibleItems : itemOffset;
	} else
	{
		itemOffset = 0;
	}
	if ( pListMenu->listMenuType == interfaceListMenuIconThumbnail )
	{
			maxWidth = interfaceInfo.clientWidth - interfaceInfo.paddingSize*5/* - INTERFACE_ARROW_SIZE*/ - interfaceInfo.thumbnailSize - INTERFACE_SCROLLBAR_WIDTH;
	} else
	{
			maxWidth = interfaceInfo.clientWidth - interfaceInfo.paddingSize*4/*- INTERFACE_ARROW_SIZE*/ - INTERFACE_SCROLLBAR_WIDTH;
	}
	for ( i=itemOffset; i<itemOffset+maxVisibleItems; i++ )
	{
		itemDisplayIndex = i-itemOffset;
		if ( i == pListMenu->baseMenu.selectedItem )
		{
			DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
			// selection rectangle
			x = interfaceInfo.clientX+interfaceInfo.paddingSize;
			y = pListMenu->listMenuType == interfaceListMenuNoThumbnail
				? interfaceInfo.clientY+(interfaceInfo.paddingSize+fh)*itemDisplayIndex + interfaceInfo.paddingSize
				: interfaceInfo.clientY+(interfaceInfo.paddingSize+pListMenu->baseMenu.thumbnailHeight)*itemDisplayIndex + interfaceInfo.paddingSize;
			gfx_drawRectangle(DRAWING_SURFACE, interface_colors[interfaceInfo.highlightColor].R, interface_colors[interfaceInfo.highlightColor].G, interface_colors[interfaceInfo.highlightColor].B, interface_colors[interfaceInfo.highlightColor].A, x, y, interfaceInfo.clientWidth - 2 * interfaceInfo.paddingSize - (maxVisibleItems < pListMenu->baseMenu.menuEntryCount ? INTERFACE_SCROLLBAR_WIDTH + interfaceInfo.paddingSize : 0), pListMenu->listMenuType == interfaceListMenuNoThumbnail ? fh + interfaceInfo.paddingSize : pListMenu->baseMenu.thumbnailHeight);
		}
		if ( pListMenu->listMenuType == interfaceListMenuIconThumbnail && pListMenu->baseMenu.menuEntry[i].thumbnail > 0 )
		{
			index = (int)pListMenu->baseMenu.menuEntry[i].pArg;
			x = interfaceInfo.clientX+interfaceInfo.paddingSize*2+/*INTERFACE_ARROW_SIZE +*/ pListMenu->baseMenu.thumbnailWidth/2;
			y = interfaceInfo.clientY+(interfaceInfo.paddingSize+pListMenu->baseMenu.thumbnailHeight)*(itemDisplayIndex+1) - pListMenu->baseMenu.thumbnailHeight/2;
			
			if( pListMenu->baseMenu.menuEntry[i].thumbnail != thumbnail_internet ||
			    index < 0 || youtubeInfo.videos[index].thumbnail[0] == 0 ||
			    interface_drawImage(DRAWING_SURFACE, youtubeInfo.videos[index].thumbnail, x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0) != 0 )
			interface_drawImage(DRAWING_SURFACE, resource_thumbnails[pListMenu->baseMenu.menuEntry[i].thumbnail], x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0);
			//interface_drawIcon(DRAWING_SURFACE, pListMenu->baseMenu.menuEntry[i].thumbnail, x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, 0, DSBLIT_BLEND_ALPHACHANNEL, interfaceAlignCenter|interfaceAlignMiddle);
		}
		//dprintf("%s: draw text\n", __FUNCTION__);
		if ( pListMenu->baseMenu.menuEntry[i].isSelectable )
		{
			//r = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_RED : INTERFACE_BOOKMARK_RED;
			//g = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_GREEN : INTERFACE_BOOKMARK_GREEN;
			//b = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_BLUE : INTERFACE_BOOKMARK_BLUE;
			//a = i == pListMenu->baseMenu.selectedItem ? INTERFACE_BOOKMARK_SELECTED_ALPHA : INTERFACE_BOOKMARK_ALPHA;
			r = INTERFACE_BOOKMARK_RED;
			g = INTERFACE_BOOKMARK_GREEN;
			b = INTERFACE_BOOKMARK_BLUE;
			a = INTERFACE_BOOKMARK_ALPHA;
		} else
		{
			r = INTERFACE_BOOKMARK_DISABLED_RED;
			g = INTERFACE_BOOKMARK_DISABLED_GREEN;
			b = INTERFACE_BOOKMARK_DISABLED_BLUE;
			a = INTERFACE_BOOKMARK_DISABLED_ALPHA;
		}
		second_line = strchr(pListMenu->baseMenu.menuEntry[i].info, '\n');
		if ( pListMenu->listMenuType == interfaceListMenuBigThumbnail || pListMenu->listMenuType == interfaceListMenuNoThumbnail )
		{
			x = interfaceInfo.clientX+interfaceInfo.paddingSize*2;//+INTERFACE_ARROW_SIZE;
			y = interfaceInfo.clientY+(interfaceInfo.paddingSize+fh)*(itemDisplayIndex+1);
		} else
		{
			x = interfaceInfo.clientX+interfaceInfo.paddingSize*3/*+INTERFACE_ARROW_SIZE*/+pListMenu->baseMenu.thumbnailWidth;
			if (second_line) 
				y = interfaceInfo.clientY+(interfaceInfo.paddingSize+pListMenu->baseMenu.thumbnailHeight)*(itemDisplayIndex+1) - pListMenu->baseMenu.thumbnailHeight/2 - 2; // + fh/4;
			else
				y = interfaceInfo.clientY+(interfaceInfo.paddingSize+pListMenu->baseMenu.thumbnailHeight)*(itemDisplayIndex+1) - pListMenu->baseMenu.thumbnailHeight/2 + fh/4;
		}

		tprintf("%c%c%s\t\t\t|%d\n", i == pListMenu->baseMenu.selectedItem ? '>' : ' ', pListMenu->baseMenu.menuEntry[i].isSelectable ? ' ' : '-', pListMenu->baseMenu.menuEntry[i].info, pListMenu->baseMenu.menuEntry[i].thumbnail);

		if (second_line) 
		{
			char *info_second_line = entryText + (second_line - pListMenu->baseMenu.menuEntry[i].info) + 1;
			int length;

			interface_getMenuEntryInfo(pMenu,i,entryText,MENU_ENTRY_INFO_LENGTH);

			*second_line = 0;
			gfx_drawText(DRAWING_SURFACE, pgfx_font, r, g, b, a, x, y, pListMenu->baseMenu.menuEntry[i].info, 0, i == pListMenu->baseMenu.selectedItem);

			length = getMaxStringLengthForFont(pgfx_smallfont, info_second_line, maxWidth-fh);
			info_second_line[length] = 0;
			gfx_drawText(DRAWING_SURFACE, pgfx_smallfont, r, g, b, a, x+fh, y+fh, info_second_line, 0, i == pListMenu->baseMenu.selectedItem);
			*second_line = '\n';
		} else 
		{
			gfx_drawText(DRAWING_SURFACE, pgfx_font, r, g, b, a, x, y, pListMenu->baseMenu.menuEntry[i].info, 0, i == pListMenu->baseMenu.selectedItem);
		}
	}

	/* draw scroll bar if needed */
	if ( maxVisibleItems < pListMenu->baseMenu.menuEntryCount )
	{
		int width, height;
		float step;

		step = pListMenu->listMenuType == interfaceListMenuBigThumbnail
			? (float)(interfaceInfo.clientHeight - interfaceInfo.paddingSize*2 - INTERFACE_SCROLLBAR_WIDTH*2)/(float)(pListMenu->infoAreaWidth * pListMenu->infoAreaHeight)
			: (float)(interfaceInfo.clientHeight - interfaceInfo.paddingSize*2 - INTERFACE_SCROLLBAR_WIDTH*2)/(float)(pListMenu->baseMenu.menuEntryCount);

		x = interfaceInfo.clientX + interfaceInfo.clientWidth - interfaceInfo.paddingSize - INTERFACE_SCROLLBAR_WIDTH;
		y = interfaceInfo.clientY + interfaceInfo.paddingSize + INTERFACE_SCROLLBAR_WIDTH + step*itemOffset;
		width = INTERFACE_SCROLLBAR_WIDTH;
		height = step * (maxVisibleItems);

		//dprintf("%s: step = %f\n", __FUNCTION__, step);

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );

		//gfx_drawRectangle(DRAWING_SURFACE, 0, 0, 200, 200, x, interfaceInfo.clientY + interfaceInfo.paddingSize*2 + INTERFACE_SCROLLBAR_WIDTH, width, step*pListMenu->baseMenu.menuEntryCount);

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_RED, INTERFACE_SCROLLBAR_COLOR_GREEN, INTERFACE_SCROLLBAR_COLOR_BLUE, INTERFACE_SCROLLBAR_COLOR_ALPHA, x, y, width, height);
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideTop|interfaceBorderSideLeft));
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideBottom|interfaceBorderSideRight));

		y = interfaceInfo.clientY + interfaceInfo.paddingSize;
		height = INTERFACE_SCROLLBAR_WIDTH;

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_RED, INTERFACE_SCROLLBAR_COLOR_GREEN, INTERFACE_SCROLLBAR_COLOR_BLUE, INTERFACE_SCROLLBAR_COLOR_ALPHA, x, y, width, height);
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideTop|interfaceBorderSideLeft));
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideBottom|interfaceBorderSideRight));

		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "arrows.png", x+INTERFACE_SCROLLBAR_WIDTH/2, y+INTERFACE_SCROLLBAR_WIDTH/2, INTERFACE_SCROLLBAR_ARROW_SIZE, INTERFACE_SCROLLBAR_ARROW_SIZE, 0, 0, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle));

		y = interfaceInfo.clientY + interfaceInfo.clientHeight - interfaceInfo.paddingSize - INTERFACE_SCROLLBAR_WIDTH;

		gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_RED, INTERFACE_SCROLLBAR_COLOR_GREEN, INTERFACE_SCROLLBAR_COLOR_BLUE, INTERFACE_SCROLLBAR_COLOR_ALPHA, x, y, width, height);
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_LT_RED, INTERFACE_SCROLLBAR_COLOR_LT_GREEN, INTERFACE_SCROLLBAR_COLOR_LT_BLUE, INTERFACE_SCROLLBAR_COLOR_LT_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideTop|interfaceBorderSideLeft));
		interface_drawInnerBorder(DRAWING_SURFACE, INTERFACE_SCROLLBAR_COLOR_DK_RED, INTERFACE_SCROLLBAR_COLOR_DK_GREEN, INTERFACE_SCROLLBAR_COLOR_DK_BLUE, INTERFACE_SCROLLBAR_COLOR_DK_ALPHA, x, y, width, height, interfaceInfo.borderWidth, (interfaceBorderSide_t)(interfaceBorderSideBottom|interfaceBorderSideRight));

		interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "arrows.png", x+INTERFACE_SCROLLBAR_WIDTH/2, y+INTERFACE_SCROLLBAR_WIDTH/2, INTERFACE_SCROLLBAR_ARROW_SIZE, INTERFACE_SCROLLBAR_ARROW_SIZE, 0, 1, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle));

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_NOFX) );
	}
}
