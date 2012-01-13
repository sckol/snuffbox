#ifndef __STB_RESOURCE_H
#define __STB_RESOURCE_H

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

#include "defines.h"

/*******************
* EXPORTED MACROS  *
********************/

/******************************************************************
* EXPORTED TYPEDEFS                                               *
*******************************************************************/

typedef enum __imageIndex_t
{
	arrow = 1,
	arrows,
	button2,
	button4,
	card_orps,
	card_sb,
	clinic_box,
	clinic_drugs,
	clinic_tools,
	cycle,
	icon_down,
	icon_incoming_call,
	icon_up,
	icons_state,
	logo_elc,
	logo,
	logo_rd,
	playcontrol_mode,
	radiobtn_empty,
	radiobtn_filled,
	settings_datetime,
	settings_dvb,
	settings_interface,
	settings_language,
	settings_network,
	settings_renew,
	settings_size,
	settings_video,
	slider_active,
	slider_cursor,
	slider_inactive,
	slideshow_buttons,
	sound,
	splash,
	statusbar_f1_cancel,
	statusbar_f1_delete,
	statusbar_f1_hang,
	statusbar_f1_record,
	statusbar_f1_sorting,
	statusbar_f2_add_record,
	statusbar_f2_dial,
	statusbar_f2_info,
	statusbar_f2_ok,
	statusbar_f2_playmode,
	statusbar_f2_settings,
	statusbar_f2_timeline,
	statusbar_f3_abook,
	statusbar_f3_add,
	statusbar_f3_edit,
	statusbar_f3_edit_record,
	statusbar_f3_keyboard,
	statusbar_f3_sshow_mode,
	statusbar_f4_enterurl,
	statusbar_f4_favorites,
	statusbar_f4_filetype,
	statusbar_f4_number,
	statusbar_f4_rename,
	statusbar_f4_schedule,
	thumbnail_account,
	thumbnail_account_active,
	thumbnail_account_buddy,
	thumbnail_account_inactive,
	thumbnail_add_url,
	thumbnail_address_book,
	thumbnail_answered_calls,
	thumbnail_balance,
	thumbnail_billed,
	thumbnail_channels,
	thumbnail_configure,
	thumbnail_dial,
	thumbnail_dialed_numbers,
	thumbnail_dvb,
	thumbnail_elecardtv,
	thumbnail_enterurl,
	thumbnail_epg,
	thumbnail_error_old,
	thumbnail_error,
	thumbnail_favorites,
	thumbnail_file,
	thumbnail_folder,
	thumbnail_gov,
	thumbnail_gov_business,
	thumbnail_gov_customs,
	thumbnail_gov_education,
	thumbnail_gov_environment,
	thumbnail_gov_gibdd,
	thumbnail_gov_healthcare,
	thumbnail_gov_insurance,
	thumbnail_gov_militia,
	thumbnail_gov_passport,
	thumbnail_gov_recruiter,
	thumbnail_gov_ssecurity,
	thumbnail_gov_statistics,
	thumbnail_gov_taxes,
	thumbnail_info,
	thumbnail_internet,
	thumbnail_loading,
	thumbnail_log,
	thumbnail_logo,
	thumbnail_message_new,
	thumbnail_message_open,
	thumbnail_messages,
	thumbnail_missed_calls,
	thumbnail_movies,
	thumbnail_multicast,
	thumbnail_music,
	thumbnail_network,
	thumbnail_no,
	thumbnail_not_selected,
	thumbnail_paused,
	thumbnail_power,
	thumbnail_question,
	thumbnail_radio,
	thumbnail_rd,
	thumbnail_recorded,
	thumbnail_recorded_epg,
	thumbnail_recorded_play,
	thumbnail_recording,
	thumbnail_redial,
	thumbnail_rutube,
	thumbnail_scan,
	thumbnail_schedule_record,
	thumbnail_search_folder,
	thumbnail_search,
	thumbnail_selected,
	thumbnail_select,
	thumbnail_sound_disabled,
	thumbnail_sound,
	thumbnail_turnaround,
	thumbnail_tvinfo,
	thumbnail_tvstandard,
	thumbnail_usb,
	thumbnail_usb_audio,
	thumbnail_usb_image,
	thumbnail_usb_video,
	thumbnail_video,
	thumbnail_vidimax,
	thumbnail_vod,
	thumbnail_voip,
	thumbnail_warning,
	thumbnail_workstation,
	thumbnail_workstation_audio,
	thumbnail_workstation_image,
	thumbnail_workstation_video,
	thumbnail_yes,
	thumbnail_youtube,	
	imageCount
} imageIndex_t;

/*******************************************************************
* EXPORTED DATA                                                    *
********************************************************************/

extern char *resource_thumbnails[];
extern char *resource_thumbnailsBig[];

/********************************
* EXPORTED FUNCTIONS PROTOTYPES *
*********************************/

#endif //__STB_RESOURCE_H
