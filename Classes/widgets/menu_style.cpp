/* $Id: menu_style.cpp 31858 2009-01-01 10:27:41Z mordante $ */
/*
   wesnoth menu styles Copyright (C) 2006 - 2009 by Patrick Parker <patrick_x99@hotmail.com>
   wesnoth menu Copyright (C) 2003-5 by David White <dave@whitevine.net>
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

#include "widgets/menu.hpp"

#include "font.hpp"
#include "video.hpp"


namespace gui {

	//static initializations
menu::imgsel_style menu::bluebg_style("misc/selection2", true,
										   0x000000, 0x000000, 0x001829,
									  0.35, 0.0, 1.0); //0.3);
menu::style menu::simple_style;
menu::imgsel_style menu::bigger_style("misc/selection2", true,
										  0x000000, 0x000000, 0x001829,
									  0.35, 0.0, 0.3, 25); //18);

#if defined(USE_TINY_GUI) && !defined(__IPHONEOS__)
menu::style &menu::default_style = menu::simple_style;
#else
menu::style &menu::default_style = menu::bluebg_style;
#endif
menu *empty_menu = NULL;

	//constructors
menu::style::style() : font_size_(font::SIZE_NORMAL),
		cell_padding_(font::SIZE_NORMAL * 3/5), thickness_(0),
		normal_rgb_(0x000000), selected_rgb_(0x000099), heading_rgb_(0x001829),
		normal_alpha_(0.2),  selected_alpha_(0.6), heading_alpha_(1.0),
		max_img_w_(-1), max_img_h_(-1)
{}

menu::style::style(int fontSize) : font_size_(fontSize),
	cell_padding_(fontSize * 3/5), thickness_(0),
	normal_rgb_(0x000000), selected_rgb_(0x000099), heading_rgb_(0x001829),
	normal_alpha_(0.2),  selected_alpha_(0.6), heading_alpha_(1.0),
	max_img_w_(-1), max_img_h_(-1)
{}
	
	
menu::style::~style()
{}
menu::imgsel_style::imgsel_style(const std::string &img_base, bool has_bg,
								 int normal_rgb, int selected_rgb, int heading_rgb,
								 double normal_alpha, double selected_alpha, double heading_alpha)
								 : img_base_(img_base), has_background_(has_bg),  initialized_(false), load_failed_(false),
								 normal_rgb2_(normal_rgb), selected_rgb2_(selected_rgb), heading_rgb2_(heading_rgb),
								 normal_alpha2_(normal_alpha), selected_alpha2_(selected_alpha), heading_alpha2_(heading_alpha)
{}
menu::imgsel_style::imgsel_style(const std::string &img_base, bool has_bg,
								 int normal_rgb, int selected_rgb, int heading_rgb,
								 double normal_alpha, double selected_alpha, double heading_alpha, int fontSize)
	: img_base_(img_base), has_background_(has_bg),  initialized_(false), load_failed_(false),
	normal_rgb2_(normal_rgb), selected_rgb2_(selected_rgb), heading_rgb2_(heading_rgb),
	normal_alpha2_(normal_alpha), selected_alpha2_(selected_alpha), heading_alpha2_(heading_alpha)
{
	font_size_ = fontSize;
	cell_padding_ = fontSize * 3/5;
}

menu::imgsel_style::~imgsel_style()
{}

size_t menu::style::get_font_size() const { return font_size_; }
size_t menu::style::get_cell_padding() const { return cell_padding_; }
size_t menu::style::get_thickness() const { return thickness_; }

void menu::style::scale_images(int max_width, int max_height)
{
	max_img_w_ = max_width;
	max_img_h_ = max_height;
}

surface menu::style::get_item_image(const image::locator& img_loc) const
{
	surface surf = image::get_image(img_loc);
	if(!surf.null())
	{
		int scale = 100;
		if(max_img_w_ > 0 && surf->w > max_img_w_) {
			scale = (max_img_w_ * 100) / surf->w;
		}
		if(max_img_h_ > 0 && surf->h > max_img_h_) {
			scale = std::min<int>(scale, ((max_img_h_ * 100) / surf->h));
		}
		if(scale != 100)
		{
			return scale_surface(surf, (scale * surf->w)/100, (scale * surf->h)/100);
		}
	}
	return surf;
}

bool menu::imgsel_style::load_image(const std::string &img_sub)
{
	std::string path = img_base_ + "-" + img_sub + ".png";
	const surface image = image::get_image(path);
	img_map_[img_sub] = image;
	return(!image.null());
}

bool menu::imgsel_style::load_images()
{
	if(!initialized_)
	{

		if(    load_image("border-botleft")
			&& load_image("border-botright")
			&& load_image("border-topleft")
			&& load_image("border-topright")
			&& load_image("border-left")
			&& load_image("border-right")
			&& load_image("border-top")
			&& load_image("border-bottom") )
		{
			thickness_ = std::min(
					img_map_["border-top"]->h,
					img_map_["border-left"]->w);

			if(has_background_ && !load_image("background"))
			{
				load_failed_ = true;
			}
			else
			{
				normal_rgb_ = normal_rgb2_;
				normal_alpha_ = normal_alpha2_;
				selected_rgb_ = selected_rgb2_;
				selected_alpha_ = selected_alpha2_;
				heading_rgb_ = heading_rgb2_;
				heading_alpha_ = heading_alpha2_;

				load_failed_ = false;
			}
			initialized_ = true;
		}
		else
		{
			thickness_ = 0;
			initialized_ = true;
			load_failed_ = true;
		}
	}
	return (!load_failed_);
}

void menu::imgsel_style::draw_row_bg(menu& menu_ref, const size_t row_index, const SDL_Rect& rect, ROW_TYPE type)
{
	if(type == SELECTED_ROW && has_background_ && !load_failed_) {
		if(bg_cache_.width != rect.w || bg_cache_.height != rect.h)
		{
			//draw scaled background image
			//scale image each time (to prevent loss of quality)
			bg_cache_.surf = scale_surface(img_map_["background"], rect.w, rect.h);
			bg_cache_.width = rect.w;
			bg_cache_.height = rect.h;
		}
		SDL_Rect clip = rect;
		blit_surface(rect.x,rect.y,bg_cache_.surf,NULL,&clip);
	}
	else {
		style::draw_row_bg(menu_ref, row_index, rect, type);
	}
}

void menu::imgsel_style::draw_row(menu& menu_ref, const size_t row_index, const SDL_Rect& rect, ROW_TYPE type)
{
	if(!load_failed_) {
		//draw item inside
		style::draw_row(menu_ref, row_index, rect, type);

		if(type == SELECTED_ROW) {
			// draw border
			surface image;
			SDL_Rect area;
			SDL_Rect clip = rect;
			area.x = rect.x;
			area.y = rect.y;

			image = img_map_["border-top"];
			area.x = rect.x;
			area.y = rect.y;
			do {
				blit_surface(area.x,area.y,image,NULL,&clip);
				area.x += image->w;
			} while( area.x < rect.x + rect.w );

			image = img_map_["border-left"];
			area.x = rect.x;
			area.y = rect.y;
			do {
				blit_surface(area.x,area.y,image,NULL,&clip);
				area.y += image->h;
			} while( area.y < rect.y + rect.h );

			image = img_map_["border-right"];
			area.x = rect.x + rect.w - thickness_;
			area.y = rect.y;
			do {
				blit_surface(area.x,area.y,image,NULL,&clip);
				area.y += image->h;
			} while( area.y < rect.y + rect.h );

			image = img_map_["border-bottom"];
			area.x = rect.x;
			area.y = rect.y + rect.h - thickness_;
			do {
				blit_surface(area.x,area.y,image,NULL,&clip);
				area.x += image->w;
			} while( area.x < rect.x + rect.w );

			image = img_map_["border-topleft"];
			area.x = rect.x;
			area.y = rect.y;
			blit_surface(area.x,area.y,image);

			image = img_map_["border-topright"];
			area.x = rect.x + rect.w - image->w;
			area.y = rect.y;
			blit_surface(area.x,area.y,image);

			image = img_map_["border-botleft"];
			area.x = rect.x;
			area.y = rect.y + rect.h - image->h;
			blit_surface(area.x,area.y,image);

			image = img_map_["border-botright"];
			area.x = rect.x + rect.w - image->w;
			area.y = rect.y + rect.h - image->h;
			blit_surface(area.x,area.y,image);
		}
	} else {
		//default drawing
		style::draw_row(menu_ref, row_index, rect, type);
	}
}

SDL_Rect menu::imgsel_style::item_size(const std::string& item) const
{
	SDL_Rect bounds = style::item_size(item);

	bounds.w += 2 * thickness_;
	bounds.h += 2 * thickness_;

	return bounds;
}


} //namesapce gui
