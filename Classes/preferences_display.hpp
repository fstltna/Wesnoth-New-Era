/* $Id: preferences_display.hpp 33157 2009-02-28 16:12:49Z ivanovic $ */
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

/** @file preferences_display.hpp */

#ifndef PREFERENCES_DISPLAY_HPP_INCLUDED
#define PREFERENCES_DISPLAY_HPP_INCLUDED

#include "game_preferences.hpp"

namespace preferences {

	struct display_manager
	{
		display_manager(display* disp);
		~display_manager();
	};

	void set_fullscreen(bool ison);
	void set_scroll_to_action(bool ison);
	void set_resolution(const std::pair<int,int>& res);
	void set_turbo(bool ison);
	void set_grid(bool ison);
	void set_turbo_speed(double speed);
	void set_colour_cursors(bool value);

	// Control unit idle animations
	void set_idle_anim(bool ison);
	void set_idle_anim_rate(int rate);

	std::string show_wesnothd_server_search(display&);
	void show_preferences_dialog(display& disp, const config& game_cfg);
	bool show_video_mode_dialog(display& disp);
	bool show_theme_dialog(display& disp);

	// If prefs is non-null, save the hotkeys in that config
	// instead of the default.
	void show_hotkeys_dialog (display & disp, config *prefs=NULL);
} // end namespace preferences

#endif
