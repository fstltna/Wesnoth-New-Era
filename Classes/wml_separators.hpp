/* $Id: wml_separators.hpp 31858 2009-01-01 10:27:41Z mordante $ */
/*
   Copyright (C) 2003 - 2009 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef WIDGET_DEFINES_HPP_INCLUDED
#define WIDGET_DEFINES_HPP_INCLUDED

char const HELP_STRING_SEPARATOR = '|', 
			//DEFAULT_ITEM = '*', 
			DEFAULT_ITEM = '+',
			COLUMN_SEPARATOR = '=',
           IMAGE_PREFIX = '&', IMG_TEXT_SEPARATOR = 1, HEADING_PREFIX = 2;

inline bool is_wml_separator(char c)
{
	switch(c)
	{
	case HELP_STRING_SEPARATOR:
	case DEFAULT_ITEM:
	case COLUMN_SEPARATOR:
	case IMAGE_PREFIX:
	case IMG_TEXT_SEPARATOR:
	case HEADING_PREFIX:
		return true;
	default:
		return false;
	}
}

#endif
