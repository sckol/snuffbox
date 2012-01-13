
/*

Elecard STB820 Demo Application
Copyright (C) 2009  Elecard Devices

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
#include "m3u.h"

#include <stdio.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#define TMPFILE "/tmp/m3u.tmp"

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

char m3u_description[MENU_ENTRY_INFO_LENGTH];
char m3u_url[MAX_URL];

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

int m3u_readEntry(FILE *f)
{
	int state = 2;
	char* str;
	if( f == NULL )
	{
		return -1;
	}
	
	while( state > 0 && fgets( m3u_url, sizeof(m3u_url), f ) != NULL )
	{
		switch( m3u_url[0] )
		{
			case '#':
				if( state == 1)
				{
					break; // comment?
				}
				if(strncmp(m3u_url, EXTINF, 7) == 0)
				{
					str = index(m3u_url, ',');
					if(str != NULL)
					{
						strncpy( m3u_description, &str[1], sizeof(m3u_description) );
						m3u_description[sizeof(m3u_description)-1] = 0;
						str = index(m3u_description, '\n');
						if( str != NULL)
						{
							*str = 0;
						}
						state = 1;
					} else
					{
						dprintf("%s: Invalid EXTINF format '%s'\n", __FUNCTION__, m3u_url);
					}
				}
				break;
			case '\t':
			case '\n':
			case '\r':
				break; // empty string
			default:
				if(state != 1)
				{
					//dprintf(%s: EXTINF " section not found!\n", __FUNCTION__);
					strncpy( m3u_description, m3u_url, sizeof(m3u_description) );
					m3u_description[sizeof(m3u_description)-1] = 0;
					str = index(m3u_description, '\n');
					if( str != NULL)
					{
						*str = 0;
					}
				}
				str = index(m3u_url, '\n');
				if( str != NULL)
				{
					*str = 0;
				}
				state = 0;
		}
	}
	if( state > 0)
	{
		fclose(f);
	}
	return state;
}

FILE*  m3u_initFile(char *m3u_filename, const char* mode)
{
	FILE* f;
	f = fopen( m3u_filename, mode);
	if( f == NULL)
	{
		eprintf("M3U: Can't fopen '%s'\n", m3u_filename);
		return NULL;
	}
	
	if( fgets(m3u_url, MAX_URL, f) == NULL || strncmp( m3u_url, EXTM3U, 7 ) != 0 )
	{
		eprintf("M3U: '%s' is not a valid M3U file\n", m3u_filename);
		fclose(f);
		return NULL;
	}
	
	return f;
}

int m3u_createFile(char *m3u_filename)
{
	FILE *f = fopen( m3u_filename, "w");
	if( f == NULL)
	{
		eprintf("M3U: Can't create m3u file at '%s'\n", m3u_filename);
		return -1;
	}
	fputs(EXTM3U "\n", f);
	fclose(f);
	return 0;
}

int m3u_getEntry(char *m3u_filename, int selected)
{
	FILE* f;
	int i = 0;

	f = m3u_initFile(m3u_filename, "r");
	if( f == NULL )
	{
		return -1;
	}
	
	while ( m3u_readEntry(f) == 0 )
	{
		if (i == selected)
		{
			fclose(f);
			return 0;
		}
		i++;
	}
	
	return 1;
}

int m3u_findUrl(char *m3u_filename, char *url)
{
	FILE* f;

	f = m3u_initFile(m3u_filename, "r");
	if( f == NULL )
	{
		return -1;
	}
	
	while ( m3u_readEntry(f) == 0 )
	{
		if (strcasecmp(m3u_url, url) == 0)
		{
			fclose(f);
			return 0;
		}
	}
	
	return 1;
}

int m3u_findDescription(char *m3u_filename, char *description)
{
	FILE* f;

	f = m3u_initFile(m3u_filename, "r");
	if( f == NULL )
	{
		return -1;
	}
	
	while ( m3u_readEntry(f) == 0 )
	{
		if (strcasecmp(m3u_description, description) == 0)
		{
			fclose(f);
			return 0;
		}
	}
	
	return 1;
}

int m3u_addEntry(char *m3u_filename, char *url, char *description)
{
	FILE* f;

	if(url == NULL)
	{
		return -2;
	}

	f = fopen(m3u_filename, "a");
	if( f == NULL )
	{
		return -1;
	}
	if(description != NULL)
	{
		char *ptr;
		ptr = index( description, '\n' );
		if( ptr )
		{
			char tmp[MENU_ENTRY_INFO_LENGTH];
			size_t tmp_length;

			if( (tmp_length = ptr-description) >= MENU_ENTRY_INFO_LENGTH )
				tmp_length = MENU_ENTRY_INFO_LENGTH-1;
			strncpy(tmp, description, tmp_length);
			tmp[tmp_length] = 0;
			fprintf(f, EXTINF ":-1,%s\n", tmp);
		} else
			fprintf(f, EXTINF ":-1,%s\n", description);
	}
	fprintf(f, "%s\n", url);
	fclose(f);
	return 0;
}

int m3u_deleteEntryByIndex(char *m3u_filename, int index)
{
	FILE *old_file;
	int i, res;
	char cmd[PATH_MAX];
	if( index < 0)
	{
		return -2;
	}
	old_file = m3u_initFile(m3u_filename, "r");
	if(old_file == NULL)
	{
		return 1;
	}
	if( m3u_createFile( TMPFILE ) != 0)
	{
		fclose(old_file);
		return 1;
	}
	i = 0; res = 0;
	while ( m3u_readEntry(old_file) == 0 && res == 0 )
	{
		if( i != index)
		{
			res = m3u_addEntry(TMPFILE, m3u_url, m3u_description);
		}
		i++;
	}
	if( res == 0)
	{
		sprintf(cmd, "mv -f \"" TMPFILE "\" \"%s\"", m3u_filename );
		system(cmd);
	}
	return res;
}

int m3u_deleteEntryByUrl(char *m3u_filename, char *url)
{
	FILE *old_file;
	int i, res;
	char cmd[PATH_MAX];
	if( url == NULL)
	{
		return -2;
	}
	old_file = m3u_initFile(m3u_filename, "r");
	if(old_file == NULL)
	{
		return 1;
	}
	if( m3u_createFile( TMPFILE ) != 0)
	{
		fclose(old_file);
		return 1;
	}
	i = 0; res = 0;
	while ( m3u_readEntry(old_file) == 0 && res == 0 )
	{
		if( strcasecmp(m3u_url, url) != 0)
		{
			res = m3u_addEntry(TMPFILE, m3u_url, m3u_description);
		}
		i++;
	}
	if( res == 0)
	{
		sprintf(cmd, "mv -f \"" TMPFILE "\" \"%s\"", m3u_filename );
		system(cmd);
	}
	return res;
}

int m3u_replaceEntryByIndex(char *m3u_filename, int index, char *url, char *description)
{
	FILE *old_file;
	int i, res;
	char cmd[PATH_MAX];
	if( index < 0 || url == NULL)
	{
		return -2;
	}
	old_file = m3u_initFile(m3u_filename, "r");
	if(old_file == NULL)
	{
		return 1;
	}
	if( m3u_createFile( TMPFILE ) != 0)
	{
		fclose(old_file);
		return 1;
	}
	i = 0; res = 0;
	while ( m3u_readEntry(old_file) == 0 && res == 0 )
	{
		if( i != index)
		{
			res = m3u_addEntry(TMPFILE, m3u_url, m3u_description);
		} else {
			res = m3u_addEntry(TMPFILE, url, description[0] != 0 ? description : NULL );
		}
		i++;
	}
	if( res == 0)
	{
		sprintf(cmd, "mv -f \"" TMPFILE "\" \"%s\"", m3u_filename );
		system(cmd);
	}
	return res;
}

int m3u_readEntryFromBuffer(char **pData, int *length)
{
	char *ptr, *ptr2, *str;
	int state = 2, ptr_length;

	if( pData == NULL || length == NULL)
	{
		return -2;
	}
	ptr = *pData;
	while( *length > 0 && state > 0 )
	{
		if ( (ptr2 = strchr(ptr, '\r')) != NULL || (ptr2 = strchr(ptr, '\n')) != NULL )
		{
			/* Skip line ends and whitespaces */
			for(; *ptr2 && *ptr2 <= ' '; ptr2++ )
			{
				ptr2[0] = 0;
			}
			ptr_length = strlen(ptr);
		} else
		{
			ptr_length = strlen(ptr);
			ptr2 = ptr+ptr_length;
		}
		if ( ptr_length > 0 )
		{
			//dprintf("%s: Got string '%s' (%d)\n", __FUNCTION__, ptr, state);
			switch( *ptr )
			{
				case '#':
					if(strncmp(ptr, EXTINF, 7) == 0)
					{
						str = index(ptr, ',');
						if(str != NULL)
						{
							strncpy( m3u_description, &str[1], MENU_ENTRY_INFO_LENGTH );
							m3u_description[MENU_ENTRY_INFO_LENGTH-1] = 0;
							state = 1;
						} else
						{
							dprintf("%s: Invalid EXTINF format '%s'\n", __FUNCTION__, ptr);
							m3u_description[0] = 0;
						}
					} else
					{
						dprintf("%s: #EXTINF expected but '%s' found\n", __FUNCTION__, ptr);
					}
					break;
				default:
					if( ptr_length < MAX_URL )
					{
						strcpy( m3u_url, ptr );
					} else
					{
						strncpy( m3u_url, ptr, MAX_URL );
						m3u_url[MAX_URL-1] = 0;
					}
					if(state != 1)
					{
						//dprintf("%s: " EXTINF " section not found!\n", __FUNCTION__);
						strncpy( m3u_description, m3u_url, MENU_ENTRY_INFO_LENGTH );
						m3u_description[MENU_ENTRY_INFO_LENGTH-1] = 0;
					}
					state = 0;
			}
		}
		*length -= ptr2-ptr;
		ptr = ptr2;
	}// while ( state > 0 && ptr2 != NULL && *length-(ptr - *pData) > 0 );
	*pData = ptr;
	return state;
}
