/* $Id: editor_resize_map.cpp 31858 2009-01-01 10:27:41Z mordante $ */
/*
   copyright (C) 2008 - 2009 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/
#define GETTEXT_DOMAIN "wesnoth-editor"

#include "gui/dialogs/editor_resize_map.hpp"

#include "gui/dialogs/field.hpp"
#include "gui/dialogs/helper.hpp"
#include "gui/widgets/label.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/slider.hpp"

namespace gui2 {

teditor_resize_map::teditor_resize_map() :
	map_width_(register_integer("width", false)),
	map_height_(register_integer("height", false)),
	height_(NULL),
	width_(NULL),
	copy_edge_terrain_(register_bool("copy_edge_terrain", false)),
	old_width_(),
	old_height_(),
	expand_direction_(EXPAND_BOTTOM_RIGHT)
{
}

void teditor_resize_map::set_map_width(int value)
{
	map_width_->set_cache_value(value);
}

int teditor_resize_map::map_width() const
{
	return map_width_->get_cache_value();
}

void teditor_resize_map::set_map_height(int value)
{
	map_height_->set_cache_value(value);
}

int teditor_resize_map::map_height() const
{
	return map_height_->get_cache_value();
}

void teditor_resize_map::set_old_map_width(int value)
{
	old_width_ = value;
}

void teditor_resize_map::set_old_map_height(int value)
{
	old_height_ = value;
}

bool teditor_resize_map::copy_edge_terrain() const
{
	return copy_edge_terrain_->get_cache_value();
}

twindow* teditor_resize_map::build_window(CVideo& video)
{
	return build(video, get_id(EDITOR_RESIZE_MAP));
}

void teditor_resize_map::pre_show(CVideo& /*video*/, twindow& window)
{
	tlabel& old_width = window.get_widget<tlabel>("old_width", false);
	tlabel& old_height = window.get_widget<tlabel>("old_height", false);
	height_ = &window.get_widget<tslider>("height", false);
	width_ = &window.get_widget<tslider>("width", false);

	height_->set_callback_positioner_move(dialog_callback<teditor_resize_map, &teditor_resize_map::update_expand_direction>);
	width_->set_callback_positioner_move(dialog_callback<teditor_resize_map, &teditor_resize_map::update_expand_direction>);
	old_width.set_label(lexical_cast<std::string>(old_width_));
	old_height.set_label(lexical_cast<std::string>(old_height_));

	std::string name_prefix = "expand";
	for (int i = 0; i < 9; ++i) {
		std::string name = name_prefix + lexical_cast<std::string>(i);
		direction_buttons_[i] = dynamic_cast<ttoggle_button*>(window.find_widget(name, false));
		VALIDATE(direction_buttons_[i], missing_widget(name));
		direction_buttons_[i]->set_callback_state_change(
			dialog_callback<teditor_resize_map, &teditor_resize_map::update_expand_direction>);
	}
	direction_buttons_[0]->set_value(true);
	update_expand_direction(window);
}

/** Convert a coordinate on a 3  by 3 grid to an index, return 9 for out of bounds */
static int resize_grid_xy_to_idx(int x, int y)
{
	if (x < 0 || x > 2 || y < 0 || y > 2) {
		return 9;
	} else {
		return y * 3 + x;
	}
}

void teditor_resize_map::set_direction_icon(int index, std::string icon)
{
	if (index < 9) {
		direction_buttons_[index]->set_icon_name("buttons/resize-direction-" + icon + ".png");
	}
}

void teditor_resize_map::update_expand_direction(twindow& window)
{
	std::string name_prefix = "expand";
	for (int i = 0; i < 9; ++i) {
		if (direction_buttons_[i]->get_value() && static_cast<int>(expand_direction_) != i) {
			expand_direction_ = static_cast<EXPAND_DIRECTION>(i);
			break;
		}
	}
	for (int i = 0; i < static_cast<int>(expand_direction_); ++i) {
		direction_buttons_[i]->set_value(false);
		set_direction_icon(i, "none");
	}
	direction_buttons_[expand_direction_]->set_value(true);
	for (int i = expand_direction_ + 1; i < 9; ++i) {
		direction_buttons_[i]->set_value(false);
		set_direction_icon(i, "none");
	}

	int xdiff = map_width_->get_widget_value(window) - old_width_ ;
	int ydiff = map_height_->get_widget_value(window) - old_height_ ;
	int x = static_cast<int>(expand_direction_) % 3;
	int y = static_cast<int>(expand_direction_) / 3;
	set_direction_icon(expand_direction_, "center");
	if (xdiff != 0) {
		int left = resize_grid_xy_to_idx(x - 1, y);
		int right = resize_grid_xy_to_idx(x + 1, y);
		if (xdiff < 0) std::swap(left, right);
		set_direction_icon(left, "left");
		set_direction_icon(right, "right");
	}
	if (ydiff != 0) {
		int top = resize_grid_xy_to_idx(x, y - 1);
		int bottom = resize_grid_xy_to_idx(x, y + 1);
		if (ydiff < 0) std::swap(top, bottom);
		set_direction_icon(top, "top");
		set_direction_icon(bottom, "bottom");
	}
	if (xdiff < 0 || ydiff < 0 || (xdiff > 0 && ydiff > 0)) {
		int nw = resize_grid_xy_to_idx(x - 1, y - 1);
		int ne = resize_grid_xy_to_idx(x + 1, y - 1);
		int sw = resize_grid_xy_to_idx(x - 1, y + 1);
		int se = resize_grid_xy_to_idx(x + 1, y + 1);
		if (xdiff < 0 || ydiff < 0) {
			std::swap(nw, se);
			std::swap(ne, sw);
		}
		set_direction_icon(nw, "top-left");
		set_direction_icon(ne, "top-right");
		set_direction_icon(sw, "bottom-left");
		set_direction_icon(se, "bottom-right");
	}
}

} // namespace gui2
