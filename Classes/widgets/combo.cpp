/* $Id: combo.cpp 31858 2009-01-01 10:27:41Z mordante $ */
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

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "widgets/combo.hpp"
#include "display.hpp"
#include "show_dialog.hpp"


namespace gui {

#ifdef USE_TINY_GUI
	const shared_string combo::empty_combo_label = "           ";
#else
	const shared_string combo::empty_combo_label = "";
#endif


const int combo::font_size = font::SIZE_SMALL;
const int combo::horizontal_padding = 10;
const int combo::vertical_padding = 10;

combo::combo(display& disp, const std::vector<shared_string>& items)
//	: button(disp.video(), items.empty() ? empty_combo_label : items[0]),
	: button(disp.video(), items.empty() ? empty_combo_label : items[0], TYPE_PRESS, "button"),
	  items_(items), selected_(0), oldSelected_(0), disp_(&disp)
{
}

int combo::selected() const
{
	return selected_;
}

bool combo::changed()
{
	if (oldSelected_ != selected_) {
		oldSelected_ = selected_;
		return true;
	} else
		return false;
}

void combo::set_items(const std::vector<shared_string>& items)
{
	items_ = items;
	selected_ = -1;
}

size_t combo::items_size() const
{
	return items_.size();
}

void combo::set_selected_internal(int val)
{
	const size_t index = size_t(val);
	if (val == selected_ || index >= items_.size())
		return;
	set_label(items_[index]);
	oldSelected_ = selected_;
	selected_ = val;
}

void combo::set_selected(int val)
{
	set_selected_internal(val);
	oldSelected_ = selected_;
}

void combo::make_drop_down_menu()
{
	SDL_Rect const &loc = location();
	set_selected_internal(gui::show_dialog(*disp_, NULL, "", "", gui::MESSAGE, &items_,
	                                       NULL, "", NULL, -1, NULL, loc.x, loc.y + loc.h));
}

void combo::process_event()
{
	if (!pressed())
		return;
	make_drop_down_menu();
}

}
