
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

#include <fcntl.h>

#include <directfb.h>
#include "app_info.h"
#include "debug.h"
#include "sem.h"
#include "gfx.h"
#include "interface.h"
#include "StbMainApp.h"

/*
#include "gfx.h"
#include "interface.h"
#include "menu_app.h"
#include "StbMainApp.h"
*/
#include "sound.h"

#include <alsa/asoundlib.h>

/***********************************************
* LOCAL MACROS                                 *
************************************************/

#ifndef STBxx
	#define SOUND_VOLUME_STEPS	(100)
#else
	#define SOUND_VOLUME_STEPS	(25)
#endif

/******************************************************************
* LOCAL TYPEDEFS                                                  *
*******************************************************************/

/******************************************************************
* STATIC FUNCTION PROTOTYPES                  <Module>_<Word>+    *
*******************************************************************/

static int sound_callback(interfaceSoundControlAction_t action, void *pArg);

static long sound_getVolume(void);

/******************************************************************
* STATIC DATA                  g[k|p|kp|pk|kpk]<Module>_<Word>+   *
*******************************************************************/

static snd_mixer_elem_t *mainVolume;
static snd_mixer_t *handle;

static long min;
static long max;

/*********************************************************(((((((**********
* EXPORTED DATA      g[k|p|kp|pk|kpk]ph[<lnx|tm|NONE>]StbTemplate_<Word>+ *
***************************************************************************/

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

/* -------------------------- SOUND SETTING --------------------------- */

int sound_init(void)
{
#ifdef STBPNX
	int err;
	snd_mixer_selem_id_t *sid;
#ifdef STB225
	static snd_mixer_elem_t *elem;
#endif
	char card[64] = "default";

	/* Perform the necessary pre-amble to start up ALSA Mixer */
	snd_mixer_selem_id_alloca(&sid);
	if ( (err = snd_mixer_open(&handle, 0)) < 0 )
	{
		eprintf("Sound: Mixer %s open error: %s\n", card, snd_strerror(err));
		return err;
	}
	if ( (err = snd_mixer_attach(handle, card)) < 0 )
	{
		eprintf("Sound: Mixer attach %s error: %s\n", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	if ( (err = snd_mixer_selem_register(handle, NULL, NULL)) < 0 )
	{
		eprintf("Sound: Mixer register error: %s\n", snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}
	err = snd_mixer_load(handle);
	if ( err < 0 )
	{
		eprintf("Sound: Mixer %s load error: %s\n", card, snd_strerror(err));
		snd_mixer_close(handle);
		return err;
	}

#ifdef STB6x8x
	/* Get the first mixer element - this will be the AK4702 which controls the volume! */
	mainVolume = snd_mixer_first_elem(handle);
	eprintf("Sound: First channel is %s (Rev. %d)\n", snd_mixer_selem_get_name(mainVolume), appControlInfo.soundInfo.hardwareRevision);
	if (appControlInfo.soundInfo.hardwareRevision < 3)
	{
		if ( appControlInfo.soundInfo.rcaOutput==1 )
		{
			/* Get the next mixer element if the SAA8510 (Anabel) is going to be used */
			mainVolume = snd_mixer_elem_next(mainVolume);
			eprintf("Sound: Second channel is %s\n", snd_mixer_selem_get_name(mainVolume));
		}
	} else
	{
		/* Get the first channel of SAA8510 (Anabel) */
		mainVolume = snd_mixer_elem_next(mainVolume);
		eprintf("Sound: Second channel is %s\n", snd_mixer_selem_get_name(mainVolume));
		if ( appControlInfo.soundInfo.rcaOutput==0 )
		{
			/* Get the next channel of SAA8510 (Anabel) */
			mainVolume = snd_mixer_elem_next(mainVolume);
			eprintf("Sound: Third channel is %s\n", snd_mixer_selem_get_name(mainVolume));
		}
	}
	snd_mixer_selem_get_id(mainVolume, sid);
#endif
#ifdef STB225
    /* find the Scart volume control */ 
    snd_mixer_selem_id_set_name(sid, "SCART");
    snd_mixer_selem_id_set_index(sid, 0);
    mainVolume = snd_mixer_find_selem( handle, sid );
    if(mainVolume == NULL)
    {
        eprintf("Sound: SCART Volume not found - using Decoder1 volume instead\n");
        snd_mixer_selem_id_set_name(sid, "Decoder");
        snd_mixer_selem_id_set_index(sid, 0);
        mainVolume = snd_mixer_find_selem( handle, sid );
        if(mainVolume == NULL)
        {
            eprintf("Sound: No Decoder1 volume control either!\n");
        }
    }
    // Set the default audio mixer controls
    // decoders are set to 100% volume and centre balance
    snd_mixer_selem_id_set_name(sid, "Decoder"); // decoder volume
    snd_mixer_selem_id_set_index(sid, 0);
    elem = snd_mixer_find_selem( handle, sid );
    if(elem == NULL)
    {
        eprintf("Sound: Decoder 0 Volume not found\n");
    }
    else
    {
        (void)snd_mixer_selem_set_playback_volume_all(elem, 100 );
    }
    snd_mixer_selem_id_set_index(sid, 1);
    elem = snd_mixer_find_selem( handle, sid );
    if(elem == NULL)
    {
        eprintf("Sound: Decoder 1 Volume not found\n");
    }
    else
    {
        (void)snd_mixer_selem_set_playback_volume_all(elem, 100 );
    }
    snd_mixer_selem_id_set_name(sid, "Decoder Balance"); // decoder balance
    snd_mixer_selem_id_set_index(sid, 0);
    elem = snd_mixer_find_selem( handle, sid );
    if(elem == NULL)
    {
        eprintf("Sound: Decoder 0 Balance not found\n");
    }
    else
    {
        (void)snd_mixer_selem_set_playback_volume_all(elem, 0 );
    }
    snd_mixer_selem_id_set_index(sid, 1);
    elem = snd_mixer_find_selem( handle, sid );
    if(elem == NULL)
    {
        eprintf("Sound: Decoder 1 Balance not found\n");
    }
    else
    {
        (void)snd_mixer_selem_set_playback_volume_all(elem, 0 );
    }
#endif

	sound_setVolume(appControlInfo.soundInfo.volumeLevel);

	snd_mixer_selem_get_playback_volume_range(mainVolume, &min, &max);

	interface_soundControlSetup(sound_callback, NULL, min, max, sound_getVolume());
#endif
	return 0;
}

static long sound_getVolume(void)
{
	return appControlInfo.soundInfo.volumeLevel;
}

void sound_setVolume(long value)
{
#ifdef STBPNX
	int outputValue = 0;

	if ( value >= 0 )
	{
		if ( appControlInfo.soundInfo.muted && value )
		{
			/* Turn off muting */
			appControlInfo.soundInfo.muted = 0;
		}

		if ( !appControlInfo.soundInfo.muted )
		{
			/* Save the new volume level */
			outputValue = appControlInfo.soundInfo.volumeLevel = (int)value;
		}
	}

	dprintf("%s: Volume is %d (%d)\n", __FUNCTION__, outputValue, appControlInfo.soundInfo.muted);

	snd_mixer_selem_set_playback_volume_all(mainVolume, appControlInfo.playbackInfo.audioStatus == audioMute ? 0 : outputValue);
#endif
}


static int sound_callback(interfaceSoundControlAction_t action, void *pArg)
{
#ifdef STBPNX
	long v;

	//dprintf("%s: in\n", __FUNCTION__);

	if ( action == interfaceSoundControlActionVolumeUp )
	{
		v = sound_getVolume() + (max-min)/SOUND_VOLUME_STEPS;
		sound_setVolume(v > max ? max : v);

		//dprintf("%s: interfaceSoundControlActionVolumeUp %d (min %d, max %d)\n", __FUNCTION__, sound_getVolume(), min, max);

		interface_soundControlSetValue(sound_getVolume());
		interface_soundControlSetMute(appControlInfo.soundInfo.muted);
	} else if ( action == interfaceSoundControlActionVolumeDown )
	{
		v = sound_getVolume() - (max-min)/SOUND_VOLUME_STEPS;
		sound_setVolume(v < min ? min : v);

		//dprintf("%s: interfaceSoundControlActionVolumeDown\n", __FUNCTION__);

		interface_soundControlSetValue(sound_getVolume());
		interface_soundControlSetMute(appControlInfo.soundInfo.muted);
	} else if ( action == interfaceSoundControlActionVolumeMute )
	{
		if ( appControlInfo.soundInfo.muted )
		{
			appControlInfo.soundInfo.muted = 0;
			sound_setVolume(appControlInfo.soundInfo.volumeLevel);
		} else
		{
			appControlInfo.soundInfo.muted = 1;
			sound_setVolume(0);
		}

		//dprintf("%s: interfaceSoundControlActionVolumeMute\n", __FUNCTION__);

		interface_soundControlSetMute(appControlInfo.soundInfo.muted);
	} else if ( action == interfaceSoundControlActionVolumeHide )
	{
		saveAppSettings();
		return 0;
	}

	interface_soundControlRefresh(1);
#endif
	return 0;
}

int sound_restart(void)
{
#ifdef STBPNX
	dprintf("%s: Restarting sound system.\n", __FUNCTION__);

	snd_mixer_close(handle);
#endif
	return sound_init();
}
