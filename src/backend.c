
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

#ifdef STB82

/***********************************************
* INCLUDE FILES                                *
************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>

#include <fcntl.h>

#include <phStbGpio.h>

#include "app_info.h"
#include "common.h"

/***********************************************
* LOCAL MACROS                                 *
************************************************/

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

/*******************************************************************************
* FUNCTION IMPLEMENTATION  <Module>[_<Word>+] for static functions             *
*                          tm[<layer>]<Module>[_<Word>+] for exported functions*
********************************************************************************/

void backend_setup(void)
{
    int gpio;
    char gpioValue;

    gpioValue = '1';
    /* Modify the RGB termination by using the logic attached to GPIO4 */
    gpio = open("/dev/gpio4", O_RDWR);
    if (gpio >= 0)
    {
        int ioctlErr;
        /* Set up GPIO4 in gpio mode */
        if ((ioctlErr = ioctl(gpio, PHSTBGPIO_IOCTL_SET_PIN_MODE, phStbGpio_PinModeGpio)) < 0)
        {
            eprintf("backend: Unable to set up gpio4 in gpio mode\n");
        }
        /* Set up the RGB termination appropriately */
        if (write(gpio, &gpioValue, 1) != 1)
        {
            eprintf("backend: Unable to set gpio4 value\n" );
        }
        close(gpio);
    }
    else
    {
        eprintf("backend: Unable to access /dev/gpio4\n" );
    }

    if ((appControlInfo.outputInfo.encConfig[0].out_signals == DSOS_CVBS) ||
        (appControlInfo.outputInfo.encConfig[0].out_signals == DSOS_YCBCR))
    {
        gpioValue = '0';
    }
    else
    {
        gpioValue = '1';
    }
    /* Modify the RGB termination by using the logic attached to GPIO14 */
    gpio = open("/dev/gpio14", O_RDWR);
    if (gpio >= 0)
    {
        int ioctlErr;
        /* Set up GPIO14 in gpio mode */
        if ((ioctlErr = ioctl(gpio, PHSTBGPIO_IOCTL_SET_PIN_MODE, phStbGpio_PinModeGpio)) < 0)
        {
            eprintf("backend: Unable to set up gpio14 in gpio mode\n");
        }
        /* Set up the RGB termination appropriately */
        if (write(gpio, &gpioValue, 1) != 1)
        {
            eprintf("backend: Unable to set gpio14 value\n" );
        }
        close(gpio);
    }
    else
    {
        eprintf("backend: Unable to access /dev/gpio14\n" );
    }
}

#endif // #ifdef STB82
