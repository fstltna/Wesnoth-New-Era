/* $Id: loadscreen.hpp 33126 2009-02-27 16:32:20Z soliton $ */
/*
   Copyright (C) 2005 - 2009 by Joeri Melis <joeri_melis@hotmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/** @file loadscreen.hpp */

#ifndef JM_LOADSCREEN_HPP
#define JM_LOADSCREEN_HPP

#include "SDL.h"

#include "global.hpp"

//#include "font.hpp"
#include "video.hpp"

#include <iostream>
#include <string>
#include <cassert>

#include "RenderQueue.h"

class loadscreen {
	public:
		// Preferred constructor
		explicit loadscreen(CVideo &screen, const int &percent = 0);
		// Keep default copy constructor
		// Keep default copy assignment
		// Destructor, dumps the counter values to stderr
		~loadscreen()
		{
			dump_counters();
			if (logo_surface_) {
				SDL_FreeSurface (logo_surface_);
			}
			if (logo_texture_)
			{
				//SDL_DestroyTexture(logo_texture_);
				//renderQueueDeleteTexture(logo_texture_);
				glDeleteTextures(1, &logo_texture_);
				logo_texture_ = 0;
				std::cerr << "Loadscreen: freed loadscreen texture...\n";
			}
		}

		/** Function to display a load progress bar. */
		void set_progress(const int percentage=0, const std::string &text="", const bool commit=true);
		/** Function to increment the progress bar. */
		void increment_progress(const int percentage=1, const std::string &text="", const bool commit=true);
		/** Function to draw a blank screen. */
		void clear_screen(const bool commit=true);

		// Counters
		int filesystem_counter;
		int setconfig_counter;
		int parser_counter;
	
		// KP: flag to prevent endless recursion
		bool disable_increments;

		/**
		 * A global loadscreen instance that can be used to avoid
		 * passing it on to functions that are many levels deep.
		 */
		static loadscreen *global_loadscreen;

		struct global_loadscreen_manager {
			explicit global_loadscreen_manager(CVideo& screen);
			~global_loadscreen_manager();
			static global_loadscreen_manager* get()
			{ return manager; }
			void reset();
private:
			static global_loadscreen_manager* manager;
			bool owns;
		};
	private:
		// Prohibit default constructor
		loadscreen();

		// Data members
		CVideo &screen_;
		SDL_Rect textarea_;
		SDL_Surface *logo_surface_;
		GLuint logo_texture_;
		bool logo_drawn_;
		int pby_offset_;
		int prcnt_;

		void dump_counters() const;
};

// Global accessible functions that centralize the loadscreen related work.
void increment_filesystem_progress();
void increment_set_config_progress();
void increment_parser_progress();

#endif
