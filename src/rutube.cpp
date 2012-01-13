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

#include "app_info.h"
#include "debug.h"
#include "downloader.h"
#include "gfx.h"
#include "interface.h"
#include "l10n.h"
#include "media.h"
#include "rutube.h"
#include "StbMainApp.h"
#include "xmlconfig.h"


#include <common.h>
#include <cstring>
#include <stdio.h>
#include <tinyxml.h>
#include <time.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define RUTUBE_FILENAME_LENGTH 48
#define RUTUBE_INFO_BUFFER_SIZE  (512*2)
#define RUTUBE_FILESIZE_MAX    (4*1024*1024)
#define MENU_ENTRY_INFO_LENGTH 256
#define KEYWORDS_MAX_NUMBER 10
#define SECONDS_PER_DAY (24*60*60)

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

/**
 A list, which consists of categories' names.
 Each menuEntry of RutubeCategories uses pArg as pointer to first asset in this category.
 Each asset has pointer to next asset in it's category.
 */
interfaceListMenu_t RutubeCategories;

/**
 A list, keeping subcategories.
 Each menuEntry uses pArg as pointer to first asset in this subcategory.
 */
interfaceListMenu_t RutubeSubCategories;

/**
 A list, which consists of movies' titles.
 It's a one menu for all categories. It redraws for each category.
 Also uses as menu for found movies.
 */
interfaceListMenu_t MoviesMenu;

static time_t start_time;

/**
 Index of a selected RutubeCategories element.
*/
static int selectedCategory;

/**
 Keeps information about movie, which is playing on the background.
*/
static rutube_asset_t *playingMovie;

/**************************************************************************
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

extern char *rutube_url = "http://rutube.ru/export/xml/elecard.xml";
// extern char *rutube_url = "http://rutube.ru/export/xml/mp4h264.xml";
// extern char *rutube_url = "http://rutube.ru/export/sony/";

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static int rutube_enterCategoriesMenu( interfaceMenu_t* pMenu, void *pArg );
static int rutube_enterSubCategoriesMenu( interfaceMenu_t* pMenu, void *pArg );
static int rutube_fillMoviesMenu( interfaceMenu_t* pMenu, void *pArg );
static int rutube_playMovie( interfaceMenu_t* pMenu, void *pArg  );
static int rutube_exitMenu( interfaceMenu_t* pMenu, void *pArg );
/**
 Shows user information about chosen movie.
*/
static int movie_infoCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);
/**
 Parses a xml file, memorizes data of movies (title, url, description, thumbnail).
 Fulls RutubeCategories menu with categories' names.
*/
static void rutube_playlist_parser( int index, void *pArg );
/**
 Gives user a text field for typing a search string.
*/
static int rutube_videoSearch(interfaceMenu_t *pMenu, void* pArg);
static int rutube_startVideoSearch(interfaceMenu_t *pMenu, char *value, void* pArg);
/**
 Shows user the example of search string.
*/
static char *rutube_getLastSearch(int index, void* pArg);
/**
 Draws menu. Function allows to show thumbnails (*.jpg images) of menu elements.
*/
static void rutube_displayMenu(interfaceMenu_t *pMenu);

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<WoTrd>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

static void rutube_playlist_parser( int index, void *pArg)
{
	xmlConfigHandle_t rt_xml;
	xmlConfigHandle_t trebuchet_node;
	rutube_asset_t *asset, *tail_asset;
	TiXmlNode *asset_node;
	TiXmlNode *category_node;
	TiXmlElement *elem;
	const char *str_title, *str_url, *str_des, *str_thumb, *str_category_thumb;
	char *str;
	char *filename = (char*)pArg;

	
	interface_clearMenuEntries( (interfaceMenu_t*)&RutubeCategories );
	str = _T("COLLECTING_INFO");
	interface_addMenuEntry( (interfaceMenu_t*)&RutubeCategories, str, rutube_videoSearch, NULL, thumbnail_loading );
	interface_displayMenu(1);/* quick show menu */

	if(filename == NULL)
	{
		eprintf("%s: Can't start parse - there is no rutube file!\n", __FUNCTION__);
		interface_showMessageBox(_T("ERR_PLAY_FILE"), thumbnail_error, 3000);
		return;
	}

/* write xml from file to rt_xml object */
	eprintf("%s start parsing...\n", __FUNCTION__);

	rt_xml = xmlConfigOpen(filename);
// rt_xml = xmlConfigParse(data);
	if ( rt_xml == NULL
		|| (trebuchet_node = xmlConfigGetElement(rt_xml, "trebuchet", 0)) == NULL )
	{
		eprintf("%d\n", __LINE__);
		if (rt_xml)
		{
			eprintf("%d\n", __LINE__);
			xmlConfigClose(rt_xml);
		}
		interface_showMessageBox(_T("ERR_STREAM_UNAVAILABLE"), thumbnail_error, 0);
		eprintf("%s: Can't parse %s!\n", __FUNCTION__, filename);
		return;
	}

	interface_clearMenuEntries( (interfaceMenu_t*)&RutubeCategories );
/* first element of menu is video search */
	str = _T("VIDEO_SEARCH");
	interface_addMenuEntry( (interfaceMenu_t*)&RutubeCategories, str, rutube_videoSearch, NULL, thumbnail_search );

/* moving along assets, getting movie information: title, url, description, thumbnail, category */
	for ( asset_node = ((TiXmlNode *)trebuchet_node)->FirstChild(); asset_node != 0; asset_node = asset_node->NextSibling() )
	{
		if ( asset_node->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(asset_node->Value(), "asset") == 0 )
		{
			str_title = (char*)xmlConfigGetText(asset_node, "title");
			str_url = (char*)xmlConfigGetText(asset_node, "assetUrl");
			str_thumb = (char*)xmlConfigGetText(asset_node, "imageUrl");
			if( str_thumb == NULL || !*str_thumb )
				str_thumb = (const char*)thumbnail_internet;
			str_des = (char*)xmlConfigGetText(asset_node, "description");
			if( str_des == NULL || !*str_des )
				str_des = _T("ERR_NO_DATA");

			if( (str_title != NULL && *str_title) && ( str_url != NULL && *str_url) )
			{
			/* search for a category/saving category's name */
				for ( category_node = ((TiXmlNode *)asset_node)->FirstChild(); 
				      category_node != 0;  category_node = category_node->NextSibling() )
				{
					if (category_node->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(category_node->Value(), "category") == 0)
					{
						TiXmlNode *subcategory_node;
						const char*  str_category;

						elem = (TiXmlElement *)category_node;
						str_category = elem->Attribute("name");
						str_category_thumb = elem->Attribute("image_std");
						if( str_category_thumb == NULL || !*str_category_thumb )
							str_category_thumb = (const char*)thumbnail_folder;
						if( str_category != NULL && *str_category )
						{
							int i;
							asset = (rutube_asset_t*) dmalloc( sizeof(rutube_asset_t) );
							if(asset != NULL)
							{
								asset->title = NULL;
								asset->url = NULL;
								asset->description = NULL;
								asset->thumbnail = NULL;
								asset->nextInCategory = NULL;

							/* write data to asset's elements */
								helperSafeStrCpy( &( asset->title ), str_title );
								helperSafeStrCpy( &( asset->url ), str_url );
								helperSafeStrCpy( &( asset->thumbnail ), str_thumb );
								helperSafeStrCpy( &( asset->description ), str_des );

							/* checks is there any subcategory */
								if( ( subcategory_node = ((TiXmlNode *)category_node)->FirstChild() ) != NULL )
								{
									const char *subcategory, *subcategory_thumb;
							/* fills menu with elements */
									elem = (TiXmlElement *)subcategory_node;
									subcategory = elem->Attribute("name");
									subcategory_thumb = elem->Attribute("image_std");
									if( subcategory_thumb == NULL || !*subcategory_thumb )
										subcategory_thumb = (const char*)thumbnail_folder;

									if(RutubeSubCategories.baseMenu.menuEntryCount != 0)
									{
										int k;
							/* wchecking for being the same subcategory in RutubeSubCategories */
										for(k = 0; k < RutubeSubCategories.baseMenu.menuEntryCount; k++ )
										{
											if(strcmp(RutubeSubCategories.baseMenu.menuEntry[k].info, subcategory) == 0)
												break;
										}
							/* adding new subcategory it in the menu */
										if(k >= RutubeSubCategories.baseMenu.menuEntryCount)
										{
											interface_addMenuEntry( (interfaceMenu_t*)&RutubeSubCategories, subcategory,  rutube_fillMoviesMenu, (void*)asset, thumbnail_folder ) - 1;
											memcpy(RutubeSubCategories.baseMenu.menuEntry[RutubeSubCategories.baseMenu.menuEntryCount-1].image, (char*)subcategory_thumb, strlen(subcategory_thumb)+1);
										}
									}
									else
									{
										interface_addMenuEntry( (interfaceMenu_t*)&RutubeSubCategories, subcategory,  rutube_fillMoviesMenu, (void*)asset, thumbnail_folder ) - 1;
										memcpy(RutubeSubCategories.baseMenu.menuEntry[RutubeSubCategories.baseMenu.menuEntryCount-1].image, (char*)subcategory_thumb, strlen(subcategory_thumb)+1);
									}
								}

						/* wchecking for being the same category in RutubeCategories */
								for( i = 1; i < RutubeCategories.baseMenu.menuEntryCount; i++ )
								{
									if( strcmp(RutubeCategories.baseMenu.menuEntry[i].info , str_category) == 0 )
									{
										tail_asset->nextInCategory = asset;
										tail_asset = asset;
										break;
									}
								}
						/* found a new category; adding it in category menu */
								if( i >= RutubeCategories.baseMenu.menuEntryCount )
								{
									tail_asset = asset;
									if(subcategory_node != NULL)/* if there is a subcategory, pActivate is enter to subcategory menu */
										interface_addMenuEntry( (interfaceMenu_t*)&RutubeCategories, str_category, rutube_enterSubCategoriesMenu, (void*)tail_asset, thumbnail_folder ) - 1;
									else
										interface_addMenuEntry( (interfaceMenu_t*)&RutubeCategories, str_category,  rutube_fillMoviesMenu, (void*)tail_asset, thumbnail_folder ) - 1;
									memcpy(RutubeCategories.baseMenu.menuEntry[RutubeCategories.baseMenu.menuEntryCount-1].image, (char*)str_category_thumb, strlen(str_category_thumb)+1);
								}
							}
							else
							{
								eprintf("%s: Memory error!\n", __FUNCTION__);
								break;
							}
						}
						break;
					}
				}
			} else
			{
				dprintf("%s: Can't find informations - url or title is NULL!\n", __FUNCTION__);
			}
		}
	}
	if(rt_xml)
		xmlConfigClose(rt_xml);
	eprintf("%s done parsing\n", __FUNCTION__);
	interface_displayMenu(1);/* quick show menu */
	return;
}

void rutube_buildMenu(interfaceMenu_t* pParent)
{
	int rutube_icons[4] = { 0, statusbar_f2_info, 0, 0 };

/* creates menu of categories */
	createListMenu(&RutubeCategories, _T("VIDEO_CATEGORIES"), thumbnail_rutube, NULL, pParent,
		interfaceListMenuIconThumbnail,  rutube_enterCategoriesMenu, rutube_exitMenu, (void*)1);

	interface_setCustomDisplayCallback((interfaceMenu_t*)&RutubeCategories, rutube_displayMenu);
/* creates menu of subcategories */
	createListMenu(&RutubeSubCategories, _T("TITLE"), thumbnail_rutube, NULL, (interfaceMenu_t*)&RutubeCategories,
		interfaceListMenuIconThumbnail,  (menuActionFunction)menuDefaultActionShowMenu, rutube_exitMenu, (void*)1);

	interface_setCustomDisplayCallback((interfaceMenu_t*)&RutubeSubCategories, rutube_displayMenu);
/* creates menu of movies */
	createListMenu(&MoviesMenu, _T("TITLE"), thumbnail_rutube, rutube_icons, (interfaceMenu_t*)&RutubeCategories,
		interfaceListMenuIconThumbnail, (menuActionFunction)menuDefaultActionShowMenu, rutube_exitMenu, (void*)1);

	interface_setCustomKeysCallback((interfaceMenu_t*)&MoviesMenu, movie_infoCallback);
	interface_setCustomDisplayCallback((interfaceMenu_t*)&MoviesMenu, rutube_displayMenu);
}

static int rutube_enterCategoriesMenu( interfaceMenu_t* pMenu, void *pArg )
{

	time_t current_time;
	double time_after_start;
	char *str;
	static char rt_filename[RUTUBE_FILENAME_LENGTH] = { 0 };
	int url_index;

/* get current time and count difference in seconds between start_time and current_time */
	time(&current_time);
	if(start_time > 0)
		time_after_start = difftime(current_time, start_time);

/* download xml from rutube, if it's the first entry into menu or if the last download was more than twenty-four hours ago */
	if(RutubeCategories.baseMenu.menuEntryCount <= 1 || time_after_start > SECONDS_PER_DAY)
	{
	/* memorize time of the first entry */
		time(&start_time);

		interface_clearMenuEntries((interfaceMenu_t *)&RutubeCategories);

		str = _T("LOADING");
		interface_addMenuEntryDisabled((interfaceMenu_t *)&RutubeCategories, str, thumbnail_loading);
		eprintf("%s: rutube url %s\n", __FUNCTION__, rutube_url);
		eprintf("%s: loading rutube xml file...\n", __FUNCTION__);

		snprintf(rt_filename, sizeof(rt_filename), RUTUBE_XML);
		url_index = downloader_find(rutube_url); /* search url in pool */
		if ( url_index < 0 )
		{
			if ( downloader_push(rutube_url, rt_filename, sizeof(rt_filename), RUTUBE_FILESIZE_MAX, rutube_playlist_parser, (void*)rt_filename ) < 0 )
			{
				eprintf("%s: Can't start download: pool is full!\n", __FUNCTION__);
				interface_showMessageBox(_T("ERR_DEFAULT_STREAM"), thumbnail_error, 3000);
				return 1;
			}
		}
//		rutube_playlist_parser( 0, (void*)"rutube_export.txt" );
	}
	return 0;
}

static int rutube_enterSubCategoriesMenu( interfaceMenu_t* pMenu, void *pArg )
{
	eprintf("%s: enter subcategories menu\n", __FUNCTION__);
/* set menu name, when enter from RutubeCategories menu */
	if(interfaceInfo.currentMenu == (interfaceMenu_t*)&RutubeCategories)
	{
		int selectedIndex = interfaceInfo.currentMenu->selectedItem;
		char* name = interfaceInfo.currentMenu->menuEntry[selectedIndex].info;

		interface_setMenuName((interfaceMenu_t*)&RutubeSubCategories, name, MENU_ENTRY_INFO_LENGTH);
	}

	interfaceInfo.currentMenu = (interfaceMenu_t*)&RutubeSubCategories;
	interfaceInfo.currentMenu->selectedItem = 0; /* set focus on the first menu element */
	return 0;
}

static int rutube_fillMoviesMenu( interfaceMenu_t* pMenu, void *pArg  )
{
	eprintf("%s: start fill movies menu\n", __FUNCTION__);

	int selectedIndex;
	char* name;
/* clears last list menu */
	interface_clearMenuEntries( (interfaceMenu_t*)&MoviesMenu );
	rutube_asset_t *asset, *check_asset;
/* fills list menu with new elements */
	selectedIndex = interfaceInfo.currentMenu->selectedItem;

	if(selectedIndex >=0)
	{
		asset = (rutube_asset_t *)interfaceInfo.currentMenu->menuEntry[selectedIndex].pArg;
		name = interfaceInfo.currentMenu->menuEntry[selectedIndex].info;
		interface_setMenuName((interfaceMenu_t*)&MoviesMenu, name, MENU_ENTRY_INFO_LENGTH);

		if(interfaceInfo.currentMenu == (interfaceMenu_t*)&RutubeSubCategories)
		{
	/* an element, which allows to return to RutubeSubCategoriesMenu */
			interface_addMenuEntry((interfaceMenu_t*)&MoviesMenu, "..", rutube_enterSubCategoriesMenu, NULL, thumbnail_folder);
			memcpy(MoviesMenu.baseMenu.menuEntry[0].image, "", strlen("")+1);

	/* if selected element is not the first element of subcategories menu */
			if(selectedIndex != interfaceInfo.currentMenu->menuEntryCount-1)
			{
				check_asset = (rutube_asset_t *)interfaceInfo.currentMenu->menuEntry[selectedIndex+1].pArg;
		/* comparison between current asset and the fisrt asset of next subcategory */
				while(asset->url != check_asset->url)
				{
					interface_addMenuEntry( (interfaceMenu_t*)&MoviesMenu, asset->title, rutube_playMovie, (void*)asset, thumbnail_internet ) - 1;
					memcpy(MoviesMenu.baseMenu.menuEntry[MoviesMenu.baseMenu.menuEntryCount-1].image, asset->thumbnail, strlen(asset->thumbnail)+1);
					asset = (rutube_asset_t *)asset->nextInCategory;
				}
			}
	/* if selected element is the first element of subcategories menu */
			else
			{
		/* add elements until the end of assets */
				while(asset != NULL) 
				{
					interface_addMenuEntry( (interfaceMenu_t*)&MoviesMenu, asset->title, rutube_playMovie, (void*)asset, thumbnail_internet ) - 1;
					memcpy(MoviesMenu.baseMenu.menuEntry[MoviesMenu.baseMenu.menuEntryCount-1].image, asset->thumbnail, strlen(asset->thumbnail)+1);
					asset = (rutube_asset_t *)asset->nextInCategory;
				}
			}
		}
		else
		{
			while(asset != NULL)
			{
				interface_addMenuEntry( (interfaceMenu_t*)&MoviesMenu, asset->title, rutube_playMovie, (void*)asset, thumbnail_internet ) - 1;
				memcpy(MoviesMenu.baseMenu.menuEntry[MoviesMenu.baseMenu.menuEntryCount-1].image, asset->thumbnail, strlen(asset->thumbnail)+1);
				asset = (rutube_asset_t *)asset->nextInCategory;
			}
		}
	}
	interfaceInfo.currentMenu = (interfaceMenu_t*)&MoviesMenu;
	interfaceInfo.currentMenu->selectedItem = 0; /* set focus on the first menu element */
	eprintf("%s: filling movies menu is done\n", __FUNCTION__);
	return 0;
}

char *rutube_getCurrentURL()
{
	static char url[MENU_ENTRY_INFO_LENGTH];
	rutube_asset_t *asset;
	int selectedIndex = MoviesMenu.baseMenu.selectedItem;
	asset = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[selectedIndex].pArg;

	if( asset == NULL)
		url[0] = 0;
	else
		snprintf(url, sizeof(url), asset->url);

	return url;
}

int rutube_startNextChannel(int direction, void* pArg)
{
	eprintf("%s[%d]: next\n", __FUNCTION__, __LINE__);
	int selectedIndex = MoviesMenu.baseMenu.selectedItem;
	int inSubmenu = 0; /* if movie menu is from subcategories, is 0; if movie menu is from categories menu, is 1 */
	int indexChange = (direction?-1:1);

/* if it's another MoviesMenu or if another movie is selected */
	if( MoviesMenu.baseMenu.pParentMenu->selectedItem == selectedCategory &&
		strcmp(MoviesMenu.baseMenu.menuEntry[selectedIndex].info, playingMovie->title) == 0 )
	{
		eprintf("%s[%d]: you are in playing movie's menu\n", __FUNCTION__, __LINE__);
		if( strcmp(MoviesMenu.baseMenu.menuEntry[0].info, "..") == 0 )
			inSubmenu = 1;

	/* if this movie is the first in the menu list and 'prev' button is chosen */
		if(selectedIndex == inSubmenu && indexChange == -1)
			return 1;

	/* if this movie is the last in the menu list and 'next' button is chosen */
		if(selectedIndex == MoviesMenu.baseMenu.menuEntryCount-1 && indexChange == 1)
			return 1;

		playingMovie = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[selectedIndex + indexChange].pArg;
		MoviesMenu.baseMenu.selectedItem = selectedIndex + indexChange;/* do prev/next menu element be selected */
	}
	else
		playingMovie = playingMovie->nextInCategory;

	if( playingMovie == NULL)
		return 1;

	appControlInfo.playbackInfo.playlistMode = playlistModeRutube;
	appControlInfo.playbackInfo.streamSource = streamSourceRutube;
	media_playURL(screenMain, playingMovie->url, playingMovie->title, playingMovie->thumbnail);
	return 0;
}

int  rutube_setChannel(int channel, void* pArg)
{
	if( channel < 1 || channel > MoviesMenu.baseMenu.menuEntryCount )
		return 1;

	int n = 1; /* if movie menu is from categories menu */

	if( strcmp(MoviesMenu.baseMenu.menuEntry[0].info, "..") == 0 )
		n = 0; /* if movie menu is from subcategories menu (the zero element is way back to subcategories menu) */

	rutube_asset_t *asset = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[channel-n].pArg;
	if(asset == NULL)
		return 1;

	appControlInfo.playbackInfo.playlistMode = playlistModeRutube;
	appControlInfo.playbackInfo.streamSource = streamSourceRutube;
	media_playURL(screenMain, asset->url, asset->title, asset->thumbnail);
	return 0;
}

static int rutube_playMovie( interfaceMenu_t* pMenu, void *pArg  )
{
	int index = (int)pArg;
	rutube_asset_t *asset;
	int selectedIndex = MoviesMenu.baseMenu.selectedItem;
/* memorize index of selected category */
	selectedCategory = MoviesMenu.baseMenu.pParentMenu->selectedItem;

	if(selectedIndex >=0)
	{
		asset = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[selectedIndex].pArg;
		appControlInfo.playbackInfo.playlistMode = playlistModeRutube;
		appControlInfo.playbackInfo.streamSource = streamSourceRutube;

	/* memorize link to this movie */
		playingMovie = asset;

		media_playURL(screenMain, asset->url, asset->title, asset->thumbnail);
	}
	return 1;
}

static int movie_infoCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	int selectedIndex = MoviesMenu.baseMenu.selectedItem;
	rutube_asset_t *asset;

	char info[RUTUBE_INFO_BUFFER_SIZE];
	if (selectedIndex >= 0 && cmd->command == interfaceCommandGreen)
	{
		asset = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[selectedIndex].pArg;
		if(asset !=NULL)
			sprintf(info, asset->description);
		else
			sprintf(info, _T("ERR_NO_DATA"));
		interface_showMessageBox(info, 0, 0);
		return 0;
	}
	return 1;
}

static int rutube_videoSearch(interfaceMenu_t *pMenu, void* pArg)
{
	eprintf("%s: user typing something...\n", __FUNCTION__);
	interface_getText(pMenu, _T("ENTER_TITLE"), "\\w+", rutube_startVideoSearch, rutube_getLastSearch, inputModeABC, pArg);
	return 0;
}

static int rutube_startVideoSearch(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	eprintf("%s: start video search...\n", __FUNCTION__);
	rutube_asset_t *asset;
	bool found = false;
	char *keywords[KEYWORDS_MAX_NUMBER], *result_str;
   
	if( value == NULL )
	{
		eprintf("%s: exit\n", __FUNCTION__);
		return 1;
	}

	interface_clearMenuEntries( (interfaceMenu_t*)&MoviesMenu );
/* split string into tokens, which will be key words */
	keywords[0] = strtok(value, " ,.-=+_?*!@/^:;#");
	//printf("[%d]: keyword[0] is %s\n", __LINE__, keywords[0]);
	for(int i = 1; i < KEYWORDS_MAX_NUMBER; i++)
	{
		keywords[i] = strtok(NULL, " ,.-=+_?*!@/^:;#");
		//printf("[%d]: keyword[%d] is %s\n", __LINE__, i, keywords[i]);
	}

	interface_setMenuName((interfaceMenu_t*)&MoviesMenu, _T("SEARCH_RESULT"), MENU_ENTRY_INFO_LENGTH);
/* finding movies... */
	int i = 0;
	while(keywords[i] != NULL && *keywords[i])
	{
		for( int j = 1; j < RutubeCategories.baseMenu.menuEntryCount; j++)
		{
			asset = (rutube_asset_t*)RutubeCategories.baseMenu.menuEntry[j].pArg;
		/* moving along assets in current category */
			while(asset != NULL)
			{
				result_str = strcasestr(asset->title, keywords[i]); /* unfortunately, the search function is case sensitive in almost all cases, except English language */
			/*...and filling MoviesMenu with found movies if exists */
				if(result_str != NULL)
				{
				/* test for existing of the same element in menu */
					if(MoviesMenu.baseMenu.menuEntryCount != 0)
					{
						int k = 0;
						bool find = false;
						rutube_asset_t *tmp_asset;
						while(find == false && k != MoviesMenu.baseMenu.menuEntryCount-1)/* search an element with the same title in MoviesMenu */
						{
							tmp_asset = (rutube_asset_t *)MoviesMenu.baseMenu.menuEntry[k].pArg;
							if(tmp_asset != NULL)
							{
								if(strcmp(asset->title, tmp_asset->title) == 0)
								{
									find = true;
								}
							}
							k++;
						}
						if(find == false)/* if the elenment wasn't found, add it to menu */
						{
							interface_addMenuEntry( (interfaceMenu_t*)&MoviesMenu, asset->title, rutube_playMovie, (void*)asset, thumbnail_internet ) - 1;
							memcpy(MoviesMenu.baseMenu.menuEntry[MoviesMenu.baseMenu.menuEntryCount-1].image, asset->thumbnail, strlen(asset->thumbnail)+1);
							//printf("[%d]: added an element %s\n", __LINE__,  asset->title);
						}
					}
					else
					{
						interface_addMenuEntry( (interfaceMenu_t*)&MoviesMenu, asset->title, rutube_playMovie, (void*)asset, thumbnail_internet ) - 1;
						memcpy(MoviesMenu.baseMenu.menuEntry[MoviesMenu.baseMenu.menuEntryCount-1].image, asset->thumbnail, strlen(asset->thumbnail)+1);
						//printf("[%d]: added an element %s\n", __LINE__, asset->title);
					}
				}
				asset = (rutube_asset_t *)asset->nextInCategory;/* go to the next asset element */
			}
		}
		i++;
	}

	if(MoviesMenu.baseMenu.menuEntryCount == 0)
	{
		interface_addMenuEntryDisabled((interfaceMenu_t*)&MoviesMenu, _T("NO_FILES"), thumbnail_info);
		memcpy(MoviesMenu.baseMenu.menuEntry[0].image, "", strlen("")+1);
		eprintf("%s: no results\n", __FUNCTION__);
	}
	else
		eprintf("%s: done\n", __FUNCTION__);

	interfaceInfo.currentMenu = (interfaceMenu_t*)&MoviesMenu;
	interface_displayMenu(1);/* quick show menu */
	return 0;
}

static char *rutube_getLastSearch(int index, void* pArg)
{
	char *example = "...";/* you may put here example string */
	return example;
}

static void rutube_displayMenu(interfaceMenu_t *pMenu)
{
	interfaceListMenu_t *pListMenu = (interfaceListMenu_t*)pMenu;
	int i, x, y, index;
	int fh;
	int itemHeight, maxVisibleItems, itemOffset, itemDisplayIndex, maxWidth;
	int r, g, b, a;
	char *second_line;
	char entryText[MENU_ENTRY_INFO_LENGTH];

	DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );
	DFBCHECK( DRAWING_SURFACE->SetColor(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA) );
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX, interfaceInfo.clientY+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientWidth, interfaceInfo.clientHeight-2*INTERFACE_ROUND_CORNER_RADIUS);	
	/* top left corner */
	gfx_drawRectangle(DRAWING_SURFACE, INTERFACE_BACKGROUND_RED, INTERFACE_BACKGROUND_GREEN, INTERFACE_BACKGROUND_BLUE, INTERFACE_BACKGROUND_ALPHA, interfaceInfo.clientX+INTERFACE_ROUND_CORNER_RADIUS, interfaceInfo.clientY, interfaceInfo.clientWidth-INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS);
	interface_drawIcon(DRAWING_SURFACE, IMAGE_DIR "black_circle.png", interfaceInfo.clientX, interfaceInfo.clientY, INTERFACE_ROUND_CORNER_RADIUS, INTERFACE_ROUND_CORNER_RADIUS, 0, 0, DSBLIT_BLEND_COLORALPHA, (interfaceAlign_t)(interfaceAlignLeft|interfaceAlignTop));
	/* bottom left corner */
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
			/* selection rectangle */
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
			if(pListMenu->baseMenu.menuEntry[i].image == NULL || index < 0 || pListMenu->baseMenu.menuEntry[i].image[0] == 0)
			{
				interface_drawImage(DRAWING_SURFACE, resource_thumbnails[pListMenu->baseMenu.menuEntry[i].thumbnail], x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0);
			}
			else
			{
				if(interface_drawImage(DRAWING_SURFACE, pListMenu->baseMenu.menuEntry[i].image, x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0) != 0)
					interface_drawImage(DRAWING_SURFACE, resource_thumbnails[pListMenu->baseMenu.menuEntry[i].thumbnail], x, y, pMenu->thumbnailWidth, pMenu->thumbnailHeight, 0, NULL, DSBLIT_BLEND_ALPHACHANNEL, (interfaceAlign_t)(interfaceAlignCenter|interfaceAlignMiddle), 0, 0);
			}
		}
		if ( pListMenu->baseMenu.menuEntry[i].isSelectable )
		{
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
		//printf("%s[%d]: maxVisibleItems = %d, menuCount = %d\n", __FILE__, __LINE__, maxVisibleItems, pListMenu->baseMenu.menuEntryCount);
		int width, height;
		float step;

		step = pListMenu->listMenuType == interfaceListMenuBigThumbnail
			? (float)(interfaceInfo.clientHeight - interfaceInfo.paddingSize*2 - INTERFACE_SCROLLBAR_WIDTH*2)/(float)(pListMenu->infoAreaWidth * pListMenu->infoAreaHeight)
			: (float)(interfaceInfo.clientHeight - interfaceInfo.paddingSize*2 - INTERFACE_SCROLLBAR_WIDTH*2)/(float)(pListMenu->baseMenu.menuEntryCount);

		//printf("%s[%d]: ******\n", __FILE__, __LINE__);

		x = interfaceInfo.clientX + interfaceInfo.clientWidth - interfaceInfo.paddingSize - INTERFACE_SCROLLBAR_WIDTH;
		y = interfaceInfo.clientY + interfaceInfo.paddingSize + INTERFACE_SCROLLBAR_WIDTH + step*itemOffset;
		width = INTERFACE_SCROLLBAR_WIDTH;
		height = step * (maxVisibleItems);

		DFBCHECK( DRAWING_SURFACE->SetDrawingFlags(DRAWING_SURFACE, DSDRAW_BLEND) );

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
	//eprintf("%s[%d]: rutube menu is displayed\n", __FUNCTION__, __LINE__);
}

void clean_list(rutube_asset_t *asset)
{
/* recursively clears linked list */
	if ( asset->nextInCategory!=NULL )
	{
		clean_list((rutube_asset_t *) asset->nextInCategory);
	}

	FREE(asset->title); //printf("%s[%d]: free title\n", __FILE__, __LINE__);
	FREE(asset->url); //printf("%s[%d]: free url\n", __FILE__, __LINE__);
	FREE(asset->description); //printf("%s[%d]: free description\n", __FILE__, __LINE__);
	FREE(asset->thumbnail); //printf("%s[%d]: free image\n", __FILE__, __LINE__);
	dfree(asset); //printf("%s[%d]: free asset\n", __FILE__, __LINE__);
}

static void rutube_freeAssets()
{
	rutube_asset_t *tmp;
	eprintf("%s: start function...\n", __FUNCTION__);
/* the first element of RutubeCategories menu is search */
	for(int i = 1; i < RutubeCategories.baseMenu.menuEntryCount; i++)
	{
		tmp = (rutube_asset_t *)RutubeCategories.baseMenu.menuEntry[i].pArg;
		clean_list(tmp);
	}
	eprintf("%s: done\n", __FUNCTION__);
}

void rutube_cleanupMenu()
{
	rutube_freeAssets();
}

static int rutube_exitMenu( interfaceMenu_t* pMenu, void *pArg )
{
	eprintf("%s: exit\n", __FUNCTION__);
	return 0;
}
