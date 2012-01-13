
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

#include "smil.h"
#include <tinyxml.h>
#include "xmlconfig.h"

#include <string.h>
#include <stdio.h>

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

int smil_parseRTMPStreams(const char *data, char *rtmp_url, const size_t url_size)
{
	TiXmlDocument *smil = NULL;
	TiXmlNode *node = NULL;
	TiXmlNode *head = NULL;
	TiXmlNode *body = NULL;
	TiXmlNode *child = NULL;
	TiXmlElement *element = NULL;
	const char *base = NULL;
	const char *stream = NULL;
	char *str;
	size_t base_length = 0, stream_length = 0;

	if( data == NULL || rtmp_url == NULL || url_size < sizeof("rtmp://xx.xx")-1)
	{
		return -2;
	}

	smil = new TiXmlDocument();
	smil->Parse(data);
	if (smil->Error() != 0)
	{
		delete smil;
		return -2;
	}

	node = (TiXmlNode *)xmlConfigGetElement(smil, "smil", 0);
	if( node == NULL )
	{
		delete smil;
		return -1;
	}

	head = (TiXmlNode *)xmlConfigGetElement(node, "head", 0);
	if( head != NULL && head->Type() == TiXmlNode::TINYXML_ELEMENT || head->Type() == TiXmlNode::TINYXML_DOCUMENT )
	{
		for ( child = head->FirstChild(); child != 0; child = child->NextSibling() )
		{
			if (child->Type() == TiXmlNode::TINYXML_ELEMENT && strcmp(child->Value(), "meta") == 0)
			{
				element = (TiXmlElement *)child;
				if((base = element->Attribute("base")) != NULL )
					break;
			}
		}
	}
	body = (TiXmlNode *)xmlConfigGetElement(node, "body", 0);
	if( body != NULL && body->Type() == TiXmlNode::TINYXML_ELEMENT || body->Type() == TiXmlNode::TINYXML_DOCUMENT )
	{
		for ( child = body->FirstChild(); child != 0; child = child->NextSibling() )
		{
			if (child->Type() == TiXmlNode::TINYXML_ELEMENT && strcmp(child->Value(), "video") == 0)
			{
				element = (TiXmlElement *)child;
				if((stream = element->Attribute("src")) != NULL )
					break;
			}
		}
	}
	if( stream == NULL )
	{
		delete smil;
		return -1;
	}
	stream_length = strlen( stream ) + 1;
	if( strncmp(stream, "rtmp://", sizeof("rtmp://")-1 ) == 0 )
	{
		if( stream_length > url_size )
		{
			delete smil;
			return -1;//url_size - stream_length;
		}
		memcpy( rtmp_url, stream, stream_length );
	} else if( base != NULL && strncmp(base, "rtmp://", sizeof("rtmp://")-1 ) == 0 )
	{
		//str = strstr(base, "/_definst_");
		//if( str != NULL )
		//	*str = 0;
		base_length = strlen( base );
		if( stream_length+1+base_length > url_size )
		{
			delete smil;
			return -1;//url_size - (stream_length + base_length + 1);
		}
		sprintf( rtmp_url, "%s/%s", base, stream );
	} else
	{
		delete smil;
		return -1;
	}
	delete smil;
	return 0;
}
