/* $Id: mouse_handler_base.cpp 34574 2009-04-05 23:37:21Z alink $ */
/*
   Copyright (C) 2006 - 2009 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playturn Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "mouse_handler_base.hpp"

#include "cursor.hpp"
#include "log.hpp"
#include "preferences.hpp"

extern bool gMegamap;
extern bool gRedraw;
extern bool gIsDragging;


namespace events {

command_disabler::command_disabler()
{
	++commands_disabled;
}

command_disabler::~command_disabler()
{
	--commands_disabled;
}

int commands_disabled= 0;

static bool command_active()
{
#ifdef __APPLE__
	return (SDL_GetModState()&KMOD_META) != 0;
#else
	return false;
#endif
}

mouse_handler_base::mouse_handler_base() :
	simple_warp_(false),
	minimap_scrolling_(false),
	dragging_left_(false),
	dragging_started_(false),
	dragging_right_(false),
	drag_from_x_(0),
	drag_from_y_(0),
	drag_from_hex_(),
	last_hex_(),
	show_menu_(false),
	didDrag_(false)
{
}

bool mouse_handler_base::is_dragging() const
{
	return dragging_left_ || dragging_right_;
}

void mouse_handler_base::mouse_motion_event(const SDL_MouseMotionEvent& event, const bool browse)
{
	mouse_motion(event.x,event.y, browse);
}

void mouse_handler_base::mouse_update(const bool browse)
{
	int x, y;
	SDL_GetMouseState(0, &x,&y);
	mouse_motion(x, y, browse, true);
}

bool mouse_handler_base::mouse_motion_default(int x, int y, bool /*update*/)
{
	if(simple_warp_) {
		return true;
	}
/*
	if(minimap_scrolling_) {
		//if the game is run in a window, we could miss a LMB/MMB up event
		// if it occurs outside our window.
		// thus, we need to check if the LMB/MMB is still down
		minimap_scrolling_ = ((SDL_GetMouseState(0, NULL,NULL) & (SDL_BUTTON(1) | SDL_BUTTON(2))) != 0);
		if(minimap_scrolling_) {
			const map_location& loc = gui().minimap_location_on(x,y);
			if(loc.valid()) {
				if(loc != last_hex_) {
					last_hex_ = loc;
					gui().scroll_to_tile(loc,display::WARP,false);
				}
			} else {
				// clicking outside of the minimap will end minimap scrolling
				minimap_scrolling_ = false;
			}
		}
		if(minimap_scrolling_) return true;
	}
*/ 

	// Fire the drag & drop only after minimal drag distance
	// While we check the mouse buttons state, we also grab fresh position data.
	int mx = drag_from_x_; // some default value to prevent unlikely SDL bug
	int my = drag_from_y_;
	if (is_dragging() && !dragging_started_) {
		if ((dragging_left_ && (SDL_GetMouseState(0, &mx,&my) & SDL_BUTTON_LEFT) != 0)
		|| (dragging_right_ && (SDL_GetMouseState(0, &mx,&my) & SDL_BUTTON_RIGHT) != 0)) {
			const double drag_distance = std::pow((double) (drag_from_x_- mx), 2) + std::pow((double) (drag_from_y_- my), 2);
			if (drag_distance > drag_threshold()*drag_threshold()) {
				dragging_started_ = true;
#ifdef __IPHONEOS__
				dragged_x_ = 0;
				dragged_y_ = 0;
#endif				
				cursor::set_dragging(true);
			}
		}
	}
#ifdef __IPHONEOS__	
	if (is_dragging() && dragging_started_ && dragging_left_)
	{
		SDL_GetMouseState(0, &mx,&my);
		int dx = drag_from_x_ - mx;
		int dy = drag_from_y_ - my;
		int scrollChangeX = -dragged_x_ + dx;
		int scrollChangeY = -dragged_y_ + dy;
		gui().scroll(scrollChangeX, scrollChangeY);
		dragged_x_ = dx;
		dragged_y_ = dy;
		didDrag_ = true;
		gIsDragging = true;
		// KP: fixes #5
		unsigned long curTime = SDL_GetTicks();
		if ((curTime - drag_start_time_) > 100) // update velocity every 1/10 second
		{
			float seconds = (float)(curTime - drag_start_time_) / 1000;
			float xVelocity = (float)-dragged_x_ / seconds;
			float yVelocity = (float)-dragged_y_ / seconds;
			
			drag_last_xVelocity_ = xVelocity;
			drag_last_yVelocity_ = yVelocity;
			
			drag_start_time_ = curTime;
			drag_from_x_ = mx;
			drag_from_y_ = my;
			dragged_x_ = 0;
			dragged_y_ = 0;
		}
		//return true;
	}
#endif		
	return false;
}

void mouse_handler_base::mouse_press(const SDL_MouseButtonEvent& event, const bool browse)
{
	if(is_middle_click(event) && !preferences::middle_click_scrolls()) {
		simple_warp_ = true;
	}
	show_menu_ = false;
	//mouse_update(browse);
	int scrollx = 0;
	int scrolly = 0;

	if (is_left_click(event)) {
		if (event.state == SDL_PRESSED) {
			cancel_dragging();
			if (event.x < gui().map_area().w)
				init_dragging(dragging_left_);
			left_click(event.x, event.y, browse);
		} else if (event.state == SDL_RELEASED) {
			minimap_scrolling_ = false;
			clear_dragging(event, browse);
			left_mouse_up(event.x, event.y, browse);
		}
	} else if (is_right_click(event)) {
		if (event.state == SDL_PRESSED) {
			cancel_dragging();
			init_dragging(dragging_right_);
			right_click(event.x, event.y, browse);
		} else if (event.state == SDL_RELEASED) {
			minimap_scrolling_ = false;
			clear_dragging(event, browse);
			right_mouse_up(event.x, event.y, browse);
		}
	} else if (is_middle_click(event)) {
		if (event.state == SDL_PRESSED) {
			map_location loc = gui().minimap_location_on(event.x,event.y);
			minimap_scrolling_ = false;
			if(loc.valid()) {
				simple_warp_ = false;
				minimap_scrolling_ = true;
				last_hex_ = loc;
				gui().scroll_to_tile(loc,display::WARP,false);
			} else if(simple_warp_) {
				// middle click not on minimap, check gamemap instead
				loc = gui().hex_clicked_on(event.x,event.y);
				if(loc.valid()) {
					last_hex_ = loc;
					gui().scroll_to_tile(loc,display::WARP,false);
				}
			}
		} else if (event.state == SDL_RELEASED) {
			minimap_scrolling_ = false;
			simple_warp_ = false;
		}
	} else if (allow_mouse_wheel_scroll(event.x, event.y)) {
		if (event.button == SDL_BUTTON_WHEELUP) {
			scrolly = - preferences::scroll_speed();
		} else if (event.button == SDL_BUTTON_WHEELDOWN) {
			scrolly = preferences::scroll_speed();
		} else if (event.button == SDL_BUTTON_WHEELLEFT) {
			scrollx = - preferences::scroll_speed();
		} else if (event.button == SDL_BUTTON_WHEELRIGHT) {
			scrollx = preferences::scroll_speed();
		}
	}

	if (scrollx != 0 || scrolly != 0) {
		CKey pressed;
		// Alt + mousewheel do an 90° rotation on the scroll direction
		if (pressed[SDLK_LALT] || pressed[SDLK_RALT])
			gui().scroll(scrolly,scrollx);
		else
			gui().scroll(scrollx,scrolly);
	}
	if (!dragging_left_ && !dragging_right_ && dragging_started_) {
		dragging_started_ = false;
		cursor::set_dragging(false);
	}
//	mouse_update(browse);
}

bool mouse_handler_base::is_left_click(const SDL_MouseButtonEvent& event)
{
	return event.button == SDL_BUTTON_LEFT && !command_active();
}

bool mouse_handler_base::is_middle_click(const SDL_MouseButtonEvent& event)
{
	return event.button == SDL_BUTTON_MIDDLE;
}

bool mouse_handler_base::is_right_click(const SDL_MouseButtonEvent& event)
{
	return event.button == SDL_BUTTON_RIGHT || (event.button == SDL_BUTTON_LEFT && command_active());
}

bool mouse_handler_base::allow_mouse_wheel_scroll(int /*x*/, int /*y*/)
{
	return true;
}

bool mouse_handler_base::right_click_show_menu(int /*x*/, int /*y*/, const bool /*browse*/)
{
	return true;
}

bool mouse_handler_base::left_click(int x, int y, const bool /*browse*/)
{
	// clicked on a hex on the minimap? then initiate minimap scrolling
	if (gMegamap == true)
	{
		const map_location& loc = gui().megamap_location_on(x, y);
		if(loc.valid()) {
			minimap_scrolling_ = true;
			last_hex_ = loc;
			gui().scroll_to_tile(loc, display::WARP, false);
			gMegamap = false;
			gRedraw = true;
			cancel_dragging();
			dragged_x_ = 0;
			dragged_y_ = 0;
			didDrag_ = false;
			gIsDragging = false;
			drag_start_time_ = 0;
			scroll_velocity_x_ = 0;
			scroll_velocity_y_ = 0;
			drag_last_xVelocity_ = 0;
			drag_last_yVelocity_ = 0;
			minimap_scrolling_ = false;
			return true;
		}		
		return false;
	}
	else
	{
		const map_location& loc = gui().minimap_location_on(x, y);
		minimap_scrolling_ = false;
		if(loc.valid()) {
#ifdef __IPAD__			
			minimap_scrolling_ = true;
			last_hex_ = loc;
			gui().scroll_to_tile(loc,display::WARP, false);
#else
			// KP: now, a click on the minimap causes the megamap to popup
			gMegamap = true;
#endif
			return true;
		}
				
		return false;
	}
}

void mouse_handler_base::left_drag_end(int x, int y, const bool browse)
{
	unsigned long endTime = SDL_GetTicks();
	float seconds = (float)(endTime - drag_start_time_) / 1000;
	float xVelocity, yVelocity;
	if (seconds < 0.25)
	{
		xVelocity = drag_last_xVelocity_;
		yVelocity = drag_last_yVelocity_;
	}
	else
	{
		//xVelocity = (float)-dragged_x_ / seconds;
		//yVelocity = (float)-dragged_y_ / seconds;
		xVelocity = 0;
		yVelocity = 0;
	}
	gui().set_scroll_velocity(xVelocity, yVelocity);
	left_click(x, y, browse);
}

void mouse_handler_base::left_mouse_up(int /*x*/, int /*y*/, const bool /*browse*/)
{
}

bool mouse_handler_base::right_click(int x, int y, const bool browse)
{
	if (right_click_show_menu(x, y, browse)) {
		gui().draw(); // redraw highlight (and maybe some more)
		const theme::menu* const m = gui().get_theme().context_menu();
		if (m != NULL) {
			show_menu_ = true;
		} else {
			LOG_STREAM(warn, display) << "no context menu found...\n";
		}
		return true;
	}
	return false;
}

void mouse_handler_base::right_drag_end(int /*x*/, int /*y*/, const bool /*browse*/)
{
}

void mouse_handler_base::right_mouse_up(int /*x*/, int /*y*/, const bool /*browse*/)
{
}

void mouse_handler_base::init_dragging(bool& dragging_flag)
{
	dragging_flag = true;
	SDL_GetMouseState(0, &drag_from_x_, &drag_from_y_);
	drag_last_xVelocity_ = 0;
	drag_last_yVelocity_ = 0;
	drag_start_time_ = SDL_GetTicks();
	drag_from_hex_ = gui().hex_clicked_on(drag_from_x_, drag_from_y_);
}

void mouse_handler_base::cancel_dragging()
{
	dragging_started_ = false;
	dragging_left_ = false;
	dragging_right_ = false;
	gui().set_scroll_velocity(0, 0);
	cursor::set_dragging(false);
}

void mouse_handler_base::clear_dragging(const SDL_MouseButtonEvent& event, bool browse)
{
	// we reset dragging info before calling functions
	// because they may take time to return, and we
	// could have started other drag&drop before that
	cursor::set_dragging(false);
	if (dragging_started_) {
		dragging_started_ = false;
		if (dragging_left_) {
			dragging_left_ = false;
			left_drag_end(event.x, event.y, browse);
		}
		if (dragging_right_) {
			dragging_right_ = false;
			right_drag_end(event.x, event.y, browse);
		}
	} else {
		dragging_left_ = false;
		dragging_right_ = false;
	}
}


} //end namespace events
