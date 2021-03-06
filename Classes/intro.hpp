/* $Id: intro.hpp 31858 2009-01-01 10:27:41Z mordante $ */
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

/** @file intro.hpp */

#ifndef INTRO_HPP_INCLUDED
#define INTRO_HPP_INCLUDED

class config;
class vconfig;
class display;
#include "SDL.h"

#include <string>

/**
 * Function to show an introduction sequence using story WML.
 * The WML config data has a format similar to:
 * @code
 * [part]
 *     id='id'
 *     story='story'
 *     image='img'
 * [/part]
 * @endcode
 * Where 'id' is a unique identifier, 'story' is text describing the
 * storyline,and 'img' is a background image. Each part of the sequence will
 * be displayed in turn, with the user able to go to the next part, or skip
 * it entirely.
 */
void show_intro(display &disp, const vconfig& data, const config& level);

/**
 * Displays a simple fading screen with any user-provided text.
 * Used after the end of single-player campaigns.
 *
 * @param text     Text to display, centered on the screen.
 *
 * @param duration In milliseconds, for how much time the text will
 *                 be displayed on screen.
 */
void the_end(display &disp, std::string text, unsigned int duration);

#endif
