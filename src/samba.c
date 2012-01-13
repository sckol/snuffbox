
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

#include "samba.h"
#include "debug.h"
#include "l10n.h"
#include "StbMainApp.h"
#include "media.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dlfcn.h>
#include <libsmbclient.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static int samba_init();
static int samba_fillBrowseMenu(interfaceMenu_t *pMenu, void *pArg);
static int samba_browseNetwork(interfaceMenu_t *pMenu, void *pArg);
static int samba_browseWorkgroup(interfaceMenu_t *pMenu, void *pArg);
static int samba_selectShare(interfaceMenu_t *pMenu, void *pArg);
static int samba_mountShare(const char *machine, const char *share, const char *mountPoint);
static int samba_setShareName(interfaceMenu_t *pMenu, char *value, void* pArg);
static int samba_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg);

static void samba_auth_data_fn(const char *srv,
	const char *shr,
	char *wg, int wglen,
	char *un, int unlen,
	char *pw, int pwlen);

static int samba_enterLogin(interfaceMenu_t *pMenu, void *pArg);
static char *samba_getUsername(int index, void* pArg);
static char *samba_getPasswd(int index, void* pArg);

static int samba_manualBrowse(interfaceMenu_t *pMenu, char *value, void* pArg);
static int samba_setUsername(interfaceMenu_t *pMenu, char *value, void* pArg);
static int samba_setPasswd(interfaceMenu_t *pMenu, char *value, void* pArg);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static void *samba_handle = NULL;
static int (*smbc_opendir_func)(const char *) = NULL;
static int (*smbc_closedir_func)(int) = NULL;
static int (*smbc_init_func)(smbc_get_auth_data_fn, int) = NULL;
static struct smbc_dirent* (*smbc_readdir_func)(int) = NULL;
/*
BOOL resolve_name(const char *name, struct in_addr *return_ip, int name_type)
 */
static BOOL (*resolve_name_func)(const char *, struct in_addr *, int);

/* 0 - NETWORK
 * 1 - SMBC_WORKGROUP
 * 2 - SMBC_SERVER
 */
static int  samba_browseType;
static char samba_url[MAX_URL];
static char samba_username[MENU_ENTRY_INFO_LENGTH];
static char samba_passwd[MENU_ENTRY_INFO_LENGTH];
static char samba_workgroup[MENU_ENTRY_INFO_LENGTH];
static char samba_machine[MENU_ENTRY_INFO_LENGTH];
static char samba_share[MENU_ENTRY_INFO_LENGTH];

static volatile int waiting_password = 0;

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

interfaceListMenu_t SambaMenu;
const  char    sambaRoot[] = "/samba/";

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

static int samba_init()
{
	int res = 0;
	int i;
	char config[BUFFER_SIZE];
	char *str;
	char temp[64];
	char buf[255];

	strcpy(config, "[global]\n\tinterfaces =");
	str = &config[strlen(config)];
	for (i=0;;i++)
	{
		int exists;

		sprintf(temp, "/sys/class/net/%s", helperEthDevice(i));
		exists = helperCheckDirectoryExsists(temp);

		if (exists)
		{
			sprintf(temp, "ifconfig %s | grep \"inet addr\"", helperEthDevice(i));
			if (helperParseLine(INFO_TEMP_FILE, temp, "inet addr:", buf, ' '))
			{
				*str = ' ';
				str++;
				strcpy(str, buf);
				str = &str[strlen(str)];
				strcpy(str, "/24");
				str+=3;
				res++;
				/*sprintf(temp, "ifconfig %s | grep \"Mask:\"", helperEthDevice(i));
				if (helperParseLine(INFO_TEMP_FILE, temp, "Mask:", buf, ' '))
				{
					*str = '/';
					str++;
					strcpy(str, buf);
					str = &str[strlen(str)];
				}*/
			}
		}
		if (!exists)
		{
			break;
		}
	}
#ifdef ENABLE_WIFI
	{
		int exists;
		sprintf(temp, "/sys/class/net/%s", helperEthDevice(ifaceWireless));
		exists = helperCheckDirectoryExsists(temp);

		if (exists)
		{
			sprintf(temp, "ifconfig %s | grep \"inet addr\"", helperEthDevice(ifaceWireless));
			if (helperParseLine(INFO_TEMP_FILE, temp, "inet addr:", buf, ' '))
			{
				*str = ' ';
				str++;
				strcpy(str, buf);
				str = &str[strlen(str)];
				strcpy(str, "/24");
				str+=3;
				res++;
			}
		}
	}
#endif
	if(res == 0)
	{
		eprintf("Samba: Network not found!\n");
		return -2;
	}
	//strcpy(str, "\n\tbind interfaces only = yes\n\tdos charset = cp866\n\tunix charset = UTF8\n\tdisplay charset = UTF8\n");
	strcpy(str, "\n\tbind interfaces only = yes\n\tunix charset = UTF8\n\tdisplay charset = UTF8\n");

	//strcpy(config, "[global]\n\tinterfaces = 192.168.111.1/24\n\tbind interfaces only = yes\n");

	i = open("/tmp/smb.conf", O_CREAT | O_TRUNC | O_WRONLY );
	if( i > 0 )
	{
		write( i, config, strlen(config) );
		close(i);
	} else
	{
		perror("Samba: Failed to open smb.conf for writing");
	}

	if ( samba_handle == NULL )
	{
		samba_handle = dlopen("libsmbclient.so", RTLD_LAZY);
		if (!samba_handle)
		{
			eprintf ("Samba: Can't init libsmbclient: %s\n", dlerror());
			return -1;
		}
		smbc_opendir_func =  dlsym( samba_handle, "smbc_opendir");
		//dprintf("%s: smbc_opendir:  (%p) %s\n", __FUNCTION__,smbc_opendir_func,dlerror());
		smbc_closedir_func = dlsym( samba_handle, "smbc_closedir");
		//dprintf("smbc_closedir: (%p) %s\n", __FUNCTION__,smbc_closedir_func,dlerror());
		smbc_readdir_func =  dlsym( samba_handle, "smbc_readdir");
		//dprintf("smbc_readdir:  (%p) %s\n", __FUNCTION__,smbc_readdir_func,dlerror());
		smbc_init_func =     dlsym( samba_handle, "smbc_init");
		//dprintf("smbc_init:     (%p) %s\n", __FUNCTION__,smbc_init_func,dlerror());
		resolve_name_func =  dlsym( samba_handle, "resolve_name");
		//dprintf("resolve_name:  (%p) %s\n", __FUNCTION__,resolve_name_func,dlerror());
		if( smbc_opendir_func == NULL || smbc_closedir_func == NULL || smbc_init_func == NULL || smbc_readdir_func == NULL)
		{
			eprintf ("Samba: Can't import all functions from libsmbclient: %s\n", dlerror());
			return -1;
		}
	} else
	{
		return 0;
	}

	res = (*smbc_init_func)(samba_auth_data_fn, 0);
	if( res < 0 )
	{
		eprintf("Samba: libsmbclient failed to initialize: %s\n", dlerror());
		samba_cleanup();
	}
	samba_browseType = 0;
	strcpy(samba_url, "smb://");
	samba_workgroup[0] = 0;

	return res;
}

void samba_cleanup()
{
	if(samba_handle)
	{
		dlclose(samba_handle);
		samba_handle = NULL;
		smbc_opendir_func = NULL;
		smbc_closedir_func = NULL;
		smbc_init_func = NULL;
		smbc_readdir_func = NULL;
	}
}

void samba_buildMenu(interfaceMenu_t *pParent)
{
	int samba_icons[4] = { 0, 0, statusbar_f3_edit, statusbar_f4_enterurl };

	createListMenu(&SambaMenu, _T("NETWORK_BROWSING"), thumbnail_multicast, samba_icons, pParent,
	interfaceListMenuIconThumbnail, samba_fillBrowseMenu, NULL, NULL);
	interface_setCustomKeysCallback((interfaceMenu_t*)&SambaMenu, samba_keyCallback);
}

static int samba_browseNetwork(interfaceMenu_t *pMenu, void *pArg)
{
	samba_url[6] = 0;
	samba_workgroup[0] = 0;
	samba_browseType = 0;
	samba_fillBrowseMenu(pMenu,pArg);
	interface_displayMenu(1);
	return 0;
}

static int samba_browseWorkgroup(interfaceMenu_t *pMenu, void *pArg)
{
	samba_browseType = SMBC_WORKGROUP;
	strcpy(&samba_url[6], samba_workgroup );
	samba_fillBrowseMenu(pMenu,pArg);
	interface_displayMenu(1);
	return 0;
}

static int samba_selectWorkgroup(interfaceMenu_t *pMenu, void *pArg)
{
	interface_getMenuEntryInfo((interfaceMenu_t*)&SambaMenu, (int)pArg, samba_workgroup, MENU_ENTRY_INFO_LENGTH);
	samba_browseWorkgroup(pMenu, pArg);
	return 0;
}

static int samba_selectMachine(interfaceMenu_t *pMenu, void *pArg)
{
	samba_browseType = SMBC_SERVER;
	interface_getMenuEntryInfo((interfaceMenu_t*)&SambaMenu, (int)pArg, samba_machine, MENU_ENTRY_INFO_LENGTH);
	strcpy(&samba_url[6], samba_machine);
	samba_fillBrowseMenu(pMenu,pArg);
	interface_displayMenu(1);
	return 0;
}

/** Ask user for Samba login and password
 * @param[in] pArg If not 0, refresh list afterwards
 */
static int samba_enterLogin(interfaceMenu_t *pMenu, void *pArg)
{
	return interface_getText(pMenu, _T("LOGIN"), "\\w+", samba_setUsername, samba_getUsername, inputModeABC, pArg);
}

static int samba_fillBrowseMenu(interfaceMenu_t *pMenu, void *pArg)
{
	int dh;
	int res;
	int icon;
	menuActionFunction pAction;
	char buf[MENU_ENTRY_INFO_LENGTH];
	char *str;
	int found;

	if( samba_init() != 0 )
	{
		interface_showMessageBox(_T("FAIL"), thumbnail_error, 5000);
		return 1;
	}

	interface_clearMenuEntries((interfaceMenu_t*)&SambaMenu);

	switch( samba_browseType )
	{
		case SMBC_WORKGROUP:
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("SAMBA_WORKGROUP"), samba_workgroup);
			smartLineTrim(buf, MENU_ENTRY_INFO_LENGTH );
			interface_setMenuName((interfaceMenu_t*)&SambaMenu, buf, MENU_ENTRY_INFO_LENGTH);
			str = _T("SAMBA_WORKGROUP_LIST");
			interface_addMenuEntry((interfaceMenu_t*)&SambaMenu, str, samba_browseNetwork, pArg, thumbnail_multicast);
			break;
		case SMBC_SERVER:
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("SAMBA_WORKSTATION"), samba_machine);
			smartLineTrim(buf, MENU_ENTRY_INFO_LENGTH );
			interface_setMenuName((interfaceMenu_t*)&SambaMenu, buf, MENU_ENTRY_INFO_LENGTH);
			snprintf(buf, MENU_ENTRY_INFO_LENGTH, "%s: %s", _T("SAMBA_WORKGROUP"), samba_workgroup);
			smartLineTrim(buf, MENU_ENTRY_INFO_LENGTH );
			str = buf;
			interface_addMenuEntry((interfaceMenu_t*)&SambaMenu, str, samba_browseWorkgroup, pArg, thumbnail_workstation);
			break;
		default:
			interface_setMenuName((interfaceMenu_t*)&SambaMenu, _T("SAMBA_WORKGROUP_LIST"), MENU_ENTRY_INFO_LENGTH);
			interface_addMenuEntry((interfaceMenu_t*)&SambaMenu, _T("NETWORK_PLACES"), media_initSambaBrowserMenu, pArg, thumbnail_network);
	}
	interface_setSelectedItem((interfaceMenu_t *)&SambaMenu, 0 );

	interface_showLoading();
	interface_displayMenu(1);
	dh = (*smbc_opendir_func)(samba_url);
	interface_hideLoading();
	found = 0;
	if(dh < 0)
	{
		eprintf("Samba: smbc_opendir failed. errno = %d (%s)\n", errno, strerror(errno));
		res = 0; // dh;
		switch( errno )
		{
			case EACCES: // permission denied
				if( interfaceInfo.messageBox.type != interfaceMessageBoxNone )
					res = 1; // if called from text input function, leave message box opened
				interface_hideMessageBox();
				interface_addMenuEntry((interfaceMenu_t *)&SambaMenu, _T("ERR_NOT_LOGGED_IN"), samba_enterLogin, (void*)1, thumbnail_info);
				samba_enterLogin((interfaceMenu_t *)&SambaMenu, (void*)1);
				return res;
			default:
				interface_showMessageBox(_T("ERR_CONNECT"), thumbnail_error, 5000);
		}
	} else
	{
		struct smbc_dirent *pdirent;

		while((pdirent = (*smbc_readdir_func)(dh)) != NULL)
		{
			dprintf("%s: Found entry type <%d>, named <%s>\n", __FUNCTION__, pdirent->smbc_type, pdirent->name);
			switch(pdirent->smbc_type)
			{
				case SMBC_WORKGROUP:  icon = thumbnail_multicast; pAction = samba_selectWorkgroup; break;
				case SMBC_SERVER:     icon = thumbnail_workstation; pAction = samba_selectMachine; break;
				case SMBC_FILE_SHARE: icon = thumbnail_folder; pAction = samba_selectShare; break;
				default:
					icon = -1;
					pAction = NULL;
			}
			if( icon > 0 && pdirent->name[strlen(pdirent->name)-1] != '$' /* hide system shares */ )
			{
				found++;
				interface_addMenuEntry((interfaceMenu_t*)&SambaMenu, pdirent->name, pAction, (void*)interface_getMenuEntryCount((interfaceMenu_t*)&SambaMenu), icon);
			}
		}
		res = (*smbc_closedir_func)(dh);
	}
	if(found == 0)
		interface_addMenuEntryDisabled((interfaceMenu_t *)&SambaMenu, _T("SAMBA_NO_SHARES"), thumbnail_info);
	return res;
}

static char *samba_getUsername(int index, void* pArg)
{
	return index == 0 ? samba_username : NULL;
}

static char *samba_getPasswd(int index, void* pArg)
{
	return index == 0 ? samba_passwd : NULL;
}

static char *samba_getMachine(int index, void* pArg)
{
	return index == 0 ? samba_machine : NULL;
}

static int samba_setUsername(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if( value == NULL )
	{
		return 1;
	}
	strcpy(samba_username, value);
	interface_getText(pMenu, _T("PASSWORD"), "\\w+", samba_setPasswd, samba_getPasswd, inputModeABC, pArg);
	return 1;
}

static int samba_setPasswd(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	if( value == NULL )
	{
		return 1;
	}
	strcpy(samba_passwd, value);
	dprintf("%s: New login/password: <%s><%s>\n", __FUNCTION__, samba_username, samba_passwd);
	if( pArg != 0 )
	{
		samba_fillBrowseMenu( pMenu, NULL );
	}
	return 0;
}

static int samba_manualBrowse(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	struct in_addr ip;
	int res;

	if( value == NULL )
	{
		return 1;
	}
	res = (*resolve_name_func)(value, &ip, 0x20);
	if( res == TRUE )
	{
		samba_workgroup[0] = 0;
		strcpy(samba_machine, value);
		samba_browseType = SMBC_SERVER;
		strcpy(&samba_url[6], samba_machine);
		dprintf("%s: manual browse %s (%s)\n", __FUNCTION__, inet_ntoa(ip), samba_url);
		res = samba_fillBrowseMenu(pMenu, pArg);
		interface_displayMenu(1);
	} else
	{
		eprintf("Samba: Can't resolve '%s', res = %d\n", value, res);
		interface_showMessageBox( _T("CONNECTION_FAILED"), thumbnail_error, 5000 );
		return 1;
	}
	return res;
}

static void samba_auth_data_fn(const char *srv,
	const char *shr,
	char *wg, int wglen,
	char *un, int unlen,
	char *pw, int pwlen)
{
	dprintf("%s: Authentification required for SERVER <%s> SHARE <%s> in WORKGROUP <%s>.\n", __FUNCTION__, srv, shr, wg);
	//strncpy(wg, samba_workgroup, wglen);
	strncpy(un, samba_username[0] != 0 ? samba_username : "guest", unlen);
	strncpy(pw, samba_passwd, pwlen);
	dprintf("%s: Returning <%s><%s>\n", __FUNCTION__, un, pw);
}

static char *samba_getShareName(int index, void* pArg)
{
	return index == 0 ? samba_share : NULL;
}

static int samba_selectShare(interfaceMenu_t *pMenu, void *pArg)
{
	int ret;
	char mount_path[strlen(sambaRoot) + MENU_ENTRY_INFO_LENGTH];
	char *str;
	struct in_addr ip;
	char share_addr[ 18 + MENU_ENTRY_INFO_LENGTH];
	size_t share_len, buf_len;
	char buf[2*BUFFER_SIZE];

	strcpy( mount_path, sambaRoot );
	str = &mount_path[strlen(sambaRoot)];
	interface_getMenuEntryInfo((interfaceMenu_t *)&SambaMenu, (int)pArg, samba_share, MENU_ENTRY_INFO_LENGTH);
	strcpy( str, samba_share );

	if( (ret = helperCheckDirectoryExsists(mount_path)) != 0 )
	{
		ret = (*resolve_name_func)(samba_machine, &ip, 0x20);
		if( ret == TRUE )
		{
			FILE *f;
			f = fopen(SAMBA_CONFIG, "r");
			if( f == NULL )
			{
				interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
				return 1;
			}
			share_len = snprintf(share_addr, sizeof(share_addr), "//%s/%s", inet_ntoa(ip), samba_share);
			while( fgets( buf, sizeof(buf), f ) != NULL )
			{
				buf_len = strlen(buf);
				if( buf_len > 0 && buf[buf_len-1] == '\n' )
				{
					buf[buf_len-1] = 0;
					buf_len--;
				}
				if( buf_len > share_len && strcmp( &buf[buf_len-share_len], share_addr ) == 0 && (str = index(buf, ';')) != NULL)
				{
					dprintf("%s: '%s' already exists in samba.auto file\n", __FUNCTION__,share_addr);
					fclose(f);
					*str = 0;
					sprintf( currentPath, "%s%s/", sambaRoot, buf );
					media_initSambaBrowserMenu( interfaceInfo.currentMenu, (void*)MENU_ITEM_BACK );
					return 0;
				}
			}
			fclose(f); // not found
			interface_getText(interfaceInfo.currentMenu, _T("ENTER_SHARE_NAME"), "\\w+", samba_setShareName, samba_getShareName, inputModeABC, pArg);
			return 1;
		} else
		{
			eprintf("Samba: Can't resolve '%s', res = %d\n", samba_machine, ret);
			interface_showMessageBox( _T("CONNECTION_FAILED"), thumbnail_error, 5000 );
			return 1;
		}
	}

	return samba_mountShare(samba_machine, samba_share, samba_share);
}

static int samba_setShareName(interfaceMenu_t *pMenu, char *value, void* pArg)
{
	char *str, *ptr;
	char mount_path[strlen(sambaRoot) + MENU_ENTRY_INFO_LENGTH];
	int ret;

	if( value == NULL )
	{
		return 1;
	}

	strcpy( mount_path, sambaRoot );
	str = &mount_path[strlen(sambaRoot)];
	strcpy( str, value );
	for( ptr = str; *str; str++, ptr++ )
	{
		while( *str == '/' )
			str++;
		if( str != ptr )
			*ptr = *str;
	}
	if( (ret = helperCheckDirectoryExsists(mount_path)) != 0 )
	{
		interface_getText(pMenu, _T("ENTER_SHARE_NAME"), "\\w+", samba_setShareName, samba_getShareName, inputModeABC, pArg);
		return 1;
	}
	str = rindex(mount_path, '/');
	if( str != NULL )
	{
		samba_mountShare(samba_machine, samba_share, str+1);
	}
	return 1;
}

static void samba_escapeCharacters(char *src, char *dst)
{
	if( src == NULL || dst == NULL )
		return;
	while( *src != 0 )
	{
		switch( *src )
		{
			case '$':
				*dst++ = '\\';
			default:
				*dst++ = *src++;
		}
	}
	*dst = 0;
}

static int samba_mountShare(const char *machine, const char *share, const char *mountPoint)
{
	char buf[2*BUFFER_SIZE];
	char share_addr[ 18 + MENU_ENTRY_INFO_LENGTH];
	size_t share_len, buf_len;
	char username[2*MENU_ENTRY_INFO_LENGTH], passwd[2*MENU_ENTRY_INFO_LENGTH];
	struct in_addr ip;
	int res;

	dprintf("%s: Adding new samba share: '%s' on '%s'\n", __FUNCTION__,share, mountPoint);

	res = (*resolve_name_func)(machine, &ip, 0x20);
	if( res == TRUE )
	{
		FILE *f;
		f = fopen(SAMBA_CONFIG, "a+");
		if( f == NULL )
		{
			interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
			return 1;
		}
		share_len = sprintf(share_addr, "//%s/%s", inet_ntoa(ip), share);
		while( fgets( buf, sizeof(buf), f ) != NULL )
		{
			buf_len = strlen(buf);
			if( buf_len > 0 && buf[buf_len-1] == '\n' )
			{
				buf[buf_len-1] = 0;
				buf_len--;
			}
			if( buf_len > share_len && strcmp( &buf[buf_len-share_len], share_addr ) == 0 )
			{
				dprintf("Samba: '%s' already exists in samba.auto file\n",share_addr);
				fclose(f);
				sprintf( currentPath, "%s%s/", sambaRoot, mountPoint );
				media_initSambaBrowserMenu( interfaceInfo.currentMenu, (void*)MENU_ITEM_BACK );
				return 0;
			}
		}
		mkdir("/tmp/mount_test", 0555);
		sprintf(buf, "username=%s,password=%s", samba_username, samba_passwd);
		res = mount(share_addr, "/tmp/mount_test", "cifs", MS_RDONLY, (const void*)buf);
		dprintf("%s: Test mount <%s><%s> res=%d\n", __FUNCTION__, samba_username, samba_passwd, res);
		if( res != 0 )
		{
			//interface_showMessageBox( _T("SAMBA_MOUNT_ERROR"), thumbnail_error, 5000 );
			sprintf(buf, "%s\n\n%s:", _T("SAMBA_MOUNT_ERROR"), _T("LOGIN"));
			interface_getText(interfaceInfo.currentMenu, buf, "\\w+", samba_setUsername, samba_getUsername, inputModeABC, NULL);
			return 1;
		}
		umount("/tmp/mount_test");
		samba_escapeCharacters(samba_username, username);
		samba_escapeCharacters(samba_passwd, passwd);

		//sprintf( cmd, "mount -t cifs -o ro,username=%s,passwd=%s //%s/%s %s", samba_username[0] ? samba_username : "", samba_username[0] && samba_passwd[0] ? samba_passwd : "", inet_ntoa(ip), share, mountPoint );
		fprintf( f, "%s;-fstype=cifs,ro,username=%s,password=%s :%s\n", mountPoint, username, passwd, share_addr );
		fclose(f);
		dprintf("%s: Added new record to samba.auto:\n%s;-fstype=cifs,ro,username='%s',password='%s' :%s\n", __FUNCTION__, mountPoint, username, passwd, share_addr );
		sprintf( currentPath, "%s%s/", sambaRoot, mountPoint );
		interface_showMessageBox( _T("ADDED_TO_NETWORK_PLACES"), thumbnail_info, 5000 );
		media_initSambaBrowserMenu( interfaceInfo.currentMenu, (void*)MENU_ITEM_BACK );
	} else
	{
		eprintf("Samba: Can't resolve '%s', res = %d\n", machine, res);
		interface_showMessageBox( _T("CONNECTION_FAILED"), thumbnail_error, 5000 );
		return 1;
	}
	return 0;
}

static int samba_keyCallback(interfaceMenu_t *pMenu, pinterfaceCommandEvent_t cmd, void* pArg)
{
	switch(cmd->command )
	{
		case interfaceCommandYellow:
			samba_enterLogin(pMenu, NULL);
			return 0;
		case interfaceCommandBlue:
			interface_getText(pMenu, _T("ENTER_WORKSTATION_ADDR"), "\\w+", samba_manualBrowse, samba_getMachine, inputModeABC, NULL);
			return 0;
		case interfaceCommandBack:
			switch( samba_browseType )
			{
				case SMBC_WORKGROUP:
					samba_browseNetwork(pMenu, pArg);
					return 0;
				case SMBC_SERVER:
					samba_browseWorkgroup(pMenu, pArg);
					return 0;
				default: ;
			}
		default:
			return 1;
	}
}

int samba_unmountShare(const char *mountPoint)
{
	FILE *src, *dst;
	char buf[2*BUFFER_SIZE];
	int found = 0;
	int mountPointNameLength = strlen(mountPoint);

	dprintf("%s: Unmounting %s\n, __FUNCTION__", mountPoint);

	if( mountPointNameLength == 0 )
		return 1;

	if(rename(SAMBA_CONFIG, SAMBA_CONFIG ".old") != 0 )
	{
		interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
		return 1;
	}

	dst = fopen(SAMBA_CONFIG, "w");
	if( dst == NULL )
	{
		rename(SAMBA_CONFIG ".old", SAMBA_CONFIG);
		unlink(SAMBA_CONFIG ".old");
		interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
		return 1;
	}
	src = fopen(SAMBA_CONFIG ".old", "r");
	if( src == NULL )
	{
		fclose(dst);
		rename(SAMBA_CONFIG ".old", SAMBA_CONFIG);
		unlink(SAMBA_CONFIG ".old");
		interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
		return 1;
	}

	while( fgets( buf, sizeof(buf), src ) != NULL )
	{
		if( strncmp( buf, mountPoint, mountPointNameLength ) != 0 || buf[mountPointNameLength] != ';' )
			fputs(buf, dst);
		else
			found = 1;
	}
	fclose(src);
	fclose(dst);
	unlink(SAMBA_CONFIG ".old");
	system("killall -SIGUSR1 automount");
	if(!found)
	{
		interface_showMessageBox( _T("SETTINGS_SAVE_ERROR"), thumbnail_error, 5000 );
	}
	return 0;
}
