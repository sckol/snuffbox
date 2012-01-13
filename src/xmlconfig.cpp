
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


#include <tinyxml.h>
#include "xmlconfig.h"

xmlConfigHandle_t xmlConfigOpen(char *path)
{
	TiXmlDocument *config = new TiXmlDocument(path);
	if (config->LoadFile(TIXML_DEFAULT_ENCODING) != true)
	{
		delete config;
		return NULL;
	}

	return config;
}

xmlConfigHandle_t xmlConfigParse(char *data)
{
	TiXmlDocument *config = new TiXmlDocument();
	config->Parse(data);

	if (config->Error() != 0)
	{
		delete config;
		return NULL;
	}

	return config;
}

xmlConfigHandle_t xmlConfigGetElement(xmlConfigHandle_t parent, char *name, int index)
{
	TiXmlNode *node = (TiXmlNode*)parent;
	TiXmlNode *child;
	int count = 0;

	if (node->Type() == TiXmlNode::TINYXML_ELEMENT || node->Type() == TiXmlNode::TINYXML_DOCUMENT)
	{
		for ( child = node->FirstChild(); child != 0; child = child->NextSibling() )
		{
			if (child->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(child->Value(), name) == 0)
			{
				if (count == index)
				{
					return child;
				}
				count++;
			}
		}
	}

	return NULL;
}

xmlConfigHandle_t xmlConfigGetNextElement(xmlConfigHandle_t element, char *name)
{
	TiXmlNode *node = (TiXmlNode*)element;
	TiXmlNode *child;
	int count = 0;

	if (node->Type() == TiXmlNode::TINYXML_ELEMENT || node->Type() == TiXmlNode::TINYXML_DOCUMENT)
	{
		for ( node = node->NextSibling(); node != 0; node = node->NextSibling() )
		{
			if (node->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(node->Value(), name) == 0)
			{
				return node;
			}
		}
	}

	return NULL;
}

const char *xmlConfigGetText(xmlConfigHandle_t parent, char *name)
{
	TiXmlNode *node = (TiXmlNode*)parent;
	TiXmlNode *child, *text;

	if (node->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		/* Find element with specified name */
		for ( child = node->FirstChild(); child != 0; child = child->NextSibling() )
		{
			if (child->Type() == TiXmlNode::TINYXML_ELEMENT && strcasecmp(child->Value(), name) == 0)
			{
				/* Find text node inside found element */
				for ( text = child->FirstChild(); text != 0; text = text->NextSibling() )
				{
					if (text->Type() == TiXmlNode::TINYXML_TEXT)
					{
						return text->ToText()->Value();
					}
				}
				/* In case there's no text - return empty string */
				return "";
			}
		}
	}

	return NULL;
}

const char *xmlConfigGetElementText(xmlConfigHandle_t element)
{
	TiXmlNode *node = (TiXmlNode*)element;
	TiXmlNode *text;

	if (node->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		/* Find text node inside found element */
		for ( text = node->FirstChild(); text != 0; text = text->NextSibling() )
		{
			if (text->Type() == TiXmlNode::TINYXML_TEXT)
			{
				return text->ToText()->Value();
			}
		}
		/* In case there's no text - return empty string */
		return "";
	}

	return NULL;
}

const char *xmlConfigGetAttribute(xmlConfigHandle_t parent, char *name)
{
	TiXmlElement *node = (TiXmlElement*)parent;
	TiXmlAttribute* attrib;

	if (node->Type() == TiXmlNode::TINYXML_ELEMENT)
	{
		/* Find element with specified name */
		for ( attrib = node->FirstAttribute(); attrib != 0; attrib = attrib->Next() )
		{
			if (strcasecmp(attrib->Name(), name) == 0)
			{
				return attrib->Value();
			}
		}
	}

	return NULL;
}

void xmlConfigClose(xmlConfigHandle_t config)
{
	if (config != NULL)
	{
		delete (TiXmlDocument *)config;
	}
}
