
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

#include "xspf.h"
#include "common.h"
#include <tinyxml.h>

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

int xspf_parseBuffer(const char *data, xspfEntryHandler pCallback, void *pArg)
{
	TiXmlDocument *xspf;
	TiXmlNode *node;
	TiXmlNode *child;
	TiXmlNode *text;
	TiXmlElement *element;
	const char *url, *name, *rel, *descr, *thumb, *idstr;
	char *endstr;

	if( data == NULL || pCallback == NULL )
	{
		eprintf("%s: invalid arguments (data %p, callback %p)\n", __FUNCTION__, data, pCallback);
		return -2;
	}
	xspf = new TiXmlDocument();
	xspf->Parse(data);
	if (xspf->Error() != 0)
	{
		eprintf("%s: XML parse failed: %s\n", __FUNCTION__, xspf->ErrorDesc());
		delete xspf;
		return -2;
	}

	node = (TiXmlNode *)xmlConfigGetElement(xspf, "playlist", 0);
	if( node == NULL )
	{
		eprintf("%s: Failed to find 'playlist' element\n", __FUNCTION__);
		delete xspf;
		return -1;
	}
	node = (TiXmlNode *)xmlConfigGetElement(node, "trackList", 0);
	if( node == NULL )
	{
		eprintf("%s: Failed to find 'trackList' element\n", __FUNCTION__);
		delete xspf;
		return -1;
	}
	if (node->Type() == TiXmlNode::TINYXML_ELEMENT || node->Type() == TiXmlNode::TINYXML_DOCUMENT)
	{
		for ( child = node->FirstChild(); child != 0; child = child->NextSibling() )
		{
			if (child->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(child->Value(), "track") == 0)
			{
				url = xmlConfigGetText(child, "location");
				name = xmlConfigGetText(child, "title");
				element = (TiXmlElement*)child;
				if( url != NULL )
				{
					pCallback(pArg, url, name, (const xmlConfigHandle_t)child);
				}
			}
		}
	}
	delete xspf;
	return 0;
}
