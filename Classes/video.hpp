/* $Id: video.hpp 33157 2009-02-28 16:12:49Z ivanovic $ */
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
#ifndef VIDEO_HPP_INCLUDED
#define VIDEO_HPP_INCLUDED

#include "events.hpp"
#include "SDL.h"
#include "sdl_utils.hpp"

#include <boost/utility.hpp>

//possible flags when setting video modes
#define FULL_SCREEN SDL_FULLSCREEN
#define VIDEO_MEMORY SDL_HWSURFACE
#define SYSTEM_MEMORY SDL_SWSURFACE

//surface display_format_alpha(surface surf);
//surface get_video_surface();
SDL_Rect screen_area();

bool non_interactive();

//which areas of the screen will be updated when the buffer is flipped?
//void update_rect(size_t x, size_t y, size_t w, size_t h);
//void update_rect(const SDL_Rect& rect);
//void update_whole_screen();


// KP: helper function to free all cache memory
extern void free_all_caches(void);


class CVideo : private boost::noncopyable {
     public:
		 enum FAKE_TYPES {
			 NO_FAKE,
			 FAKE,
			 FAKE_TEST
		 };
	CVideo(FAKE_TYPES type = NO_FAKE);
	CVideo(int x, int y, int bits_per_pixel, int flags);
	~CVideo();


	int modePossible( int x, int y, int bits_per_pixel, int flags );
	int setMode( int x, int y, int bits_per_pixel, int flags );

	//did the mode change, since the last call to the modeChanged() method?
	bool modeChanged();

	//functions to get the dimensions of the current video-mode
	static int getx();
	static int gety();
	int getBitsPerPixel();
	int getBytesPerPixel();
	int getRedMask();
	int getGreenMask();
	int getBlueMask();

	//functions to access the screen
	void lock();
	void unlock();
	int mustLock();

	//blits a surface with black as alpha
	//static void blit_surface(int x, int y, surface surf, SDL_Rect* srcrect=NULL, SDL_Rect* clip_rect=NULL);
	//static void blit_surface_scaled(int x, int y, int w, int h, surface surf); // -> externed instead
	//void flip();

	//surface getSurface();

	bool isFullScreen() const;

	struct error {};

	struct quit {};

	//functions to allow changing video modes when 16BPP is emulated
	void setBpp( int bpp );
	int getBpp();

	//void make_fake();
	//void make_test_fake();
	bool faked() const { return false; /*fake_screen_*/ }

	//functions to set and clear 'help strings'. A 'help string' is like a tooltip, but it appears
	//at the bottom of the screen, so as to not be intrusive. Setting a help string sets what
	//is currently displayed there.
	int set_help_string(const std::string& str);
	void clear_help_string(int handle);
	void clear_all_help_strings();

	//function to stop the screen being redrawn. Anything that happens while
	//the update is locked will be hidden from the user's view.
	//note that this function is re-entrant, meaning that if lock_updates(true)
	//is called twice, lock_updates(false) must be called twice to unlock
	//updates.
	void lock_updates(bool value);
	bool update_locked() const;

private:

	void initSDL();

	bool mode_changed_;

	int bpp_;	// Store real bits per pixel

	//if there is no display at all, but we 'fake' it for clients
	bool fake_screen_;

	//variables for help strings
	int help_string_;

	int updatesLocked_;
};

//an object which will lock the display for the duration of its lifetime.
struct update_locker
{
	update_locker(CVideo& v, bool lock=true) : video(v), unlock(lock) {
		if(lock) {
			video.lock_updates(true);
		}
	}

	~update_locker() {
		unlock_update();
	}

	void unlock_update() {
		if(unlock) {
			video.lock_updates(false);
			unlock = false;
		}
	}

private:
	CVideo& video;
	bool unlock;
};

class resize_monitor : public events::pump_monitor {
	void process(events::pump_info &info);
};

//an object which prevents resizing of the screen occuring during
//its lifetime.
struct resize_lock {
	resize_lock();
	~resize_lock();
};

extern bool gIsTerrainCacheEnabled;
extern void enableTerrainCache(void);
extern void disableTerrainCache(void);
extern void cacheTerrain(surface surf);
extern void blit_surface(int x, int y, surface surf, SDL_Rect* srcrect=NULL, SDL_Rect* clip_rect=NULL, textureRenderFlags flags=DRAW);
extern void blit_texture(int x, int y, SDL_TextureID tex, SDL_Rect* srcrect=NULL, SDL_Rect* clip_rect=NULL, textureRenderFlags flags=DRAW);
extern void blit_surface_scaled(int x, int y, int w, int h, surface surf, textureRenderFlags flags=DRAW);
extern void blit_texture_scaled(int x, int y, int w, int h, SDL_TextureID tex, textureRenderFlags flags=DRAW);
extern void draw_line(Uint32 color, int x1, int y1, int x2, int y2);
extern void fill_rect(Uint32 color, SDL_Rect *rect);
extern void convertSurfaceToFill(int x, int y, surface surf);
extern void draw_wait_cursor(void);
extern int nextPowerOf2(int in);
#endif
