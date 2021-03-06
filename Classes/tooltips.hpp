/* $Id: tooltips.hpp 31858 2009-01-01 10:27:41Z mordante $ */
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

#ifndef TOOLTIPS_HPP_INCLUDED
#define TOOLTIPS_HPP_INCLUDED

#include "SDL.h"

#include <string>

class CVideo;

namespace tooltips {

struct manager
{
	manager(CVideo& video);
	~manager();
};

void clear_tooltips();
void clear_tooltips(const SDL_Rect& rect);
void add_tooltip(const SDL_Rect& rect, const std::string& message);
void process(int mousex, int mousey);

}

#endif
