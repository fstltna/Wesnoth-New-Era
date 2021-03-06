/* $Id: titlescreen.cpp 33383 2009-03-07 14:53:43Z ilor $ */
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

/**
 *  @file titlescreen.cpp
 *  Shows the titlescreen, with main-menu and tip-of-the-day.
 *
 *  The menu consists of buttons, such als Start-Tutorial, Start-Campaign,
 *  Load-Game, etc.  As decoration, the wesnoth-logo and a landmap in the
 *  background are shown.
 */


#include "global.hpp"

#include "config.hpp"
#include "construct_dialog.hpp"
#include "cursor.hpp"
#include "game_display.hpp"
#include "game_preferences.hpp"
#include "events.hpp"
#include "filesystem.hpp"
#include "game_config.hpp"
#include "hotkeys.hpp"
#include "key.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "preferences_display.hpp"
#include "sdl_utils.hpp"
#include "show_dialog.hpp"
#include "titlescreen.hpp"
#include "video.hpp"
#include "serialization/parser.hpp"
#include "serialization/preprocessor.hpp"
#include <algorithm>
#include <vector>

#include <SDL_image.h>

#import "CoreFoundation/CoreFoundation.h"

//#include "SDL_ttf.h"
#include "font.hpp"

#include "memory_wrapper.h"

#include "achievements.h"

#include <OpenGLES/ES1/gl.h>
extern GLuint gScrollTex;
#include "RenderQueue.h"

/** Log info-messages to stdout during the game, mainly for debugging */
#define LOG_DP LOG_STREAM(info, display)
/** Log error-messages to stdout during the game, mainly for debugging */
#define ERR_DP LOG_STREAM(err, display)
#define LOG_CONFIG LOG_STREAM(info, config)
#define ERR_CONFIG LOG_STREAM(err, config)

/**
 *  Fade-in the wesnoth-logo.
 *
 *  Animation-effect: scroll-in from right. \n
 *  Used only once, after the game is started.
 *
 *  @param	screen	surface to operate on
 *  @param	xpos	x-position of logo
 *  @param	ypos	y-position of logo
 *
 *  @return		Result of running the routine
 *  @retval true	operation finished (successful or not)
 *  @retval false	operation failed (because modeChanged), need to retry
 */
/*
static bool fade_logo(game_display& screen, int xpos, int ypos)
{
	const surface logo(image::get_image(game_config::game_logo));
	if(logo == NULL) {
		ERR_DP << "Could not find game logo\n";
		return true;
	}

	surface const fb = screen.video().getSurface();

	if(fb == NULL || xpos < 0 || ypos < 0 || xpos + logo->w > fb->w || ypos + logo->h > fb->h) {
		return true;
	}

	// Only once, when the game is first started, the logo fades in unless
	// it was disabled in adv. preferences
	static bool faded_in = !preferences::startup_effect();
//	static bool faded_in = true;	// for faster startup: mark logo as 'has already faded in'

	CKey key;
	bool last_button = key[SDLK_ESCAPE] || key[SDLK_SPACE];

	LOG_DP << "fading logo in....\n";
	LOG_DP << "logo size: " << logo->w << "," << logo->h << "\n";

	for(int x = 0; x != logo->w; ++x) {
		SDL_Rect srcrect = {x,0,1,logo->h};
		SDL_Rect dstrect = {xpos+x,ypos,1,logo->h};
		SDL_BlitSurface(logo,&srcrect,fb,&dstrect);

		update_rect(dstrect);

		if(!faded_in && (x%5) == 0) {

			const bool new_button = key[SDLK_ESCAPE] || key[SDLK_SPACE] ||
									key[SDLK_RETURN] || key[SDLK_KP_ENTER] ;
			if(new_button && !last_button) {
				faded_in = true;
			}

			last_button = new_button;

			screen.update_display();
			screen.delay(10);

			events::pump();
			if(screen.video().modeChanged()) {
				faded_in = true;
				return false;
			}
		}

	}

	LOG_DP << "logo faded in\n";

	faded_in = true;
	return true;
}
*/

/** Read the file with the tips-of-the-day. */
/*
static void read_tips_of_day(config& tips_of_day)
{
	tips_of_day.clear();
	
	
#ifdef BUILD_CACHE	
	LOG_CONFIG << "Loading tips of day\n";
	try {
		scoped_istream stream = preprocess_file(get_wml_location("hardwired/tips.cfg"));
		read(tips_of_day, *stream);
	} catch(config::error&) {
		ERR_CONFIG << "Could not read data/hardwired/tips.cfg\n";
	}
	
	std::string filename = get_cache_dir() + "/tips";
	tips_of_day.saveCache(filename);
#else	
	std::string filename = get_cache_dir() + "/tips";
	tips_of_day.loadCache(filename);
#endif	

	//we shuffle the tips after each initial loading. We only shuffle if
	//the upload_log preference has been set. If it hasn't been set, it's the
	//user's first time playing since this feature has been added, so we'll
	//leave the tips in their default order, which will always contain a tip
	//regarding the upload log first, so the user sees it.
	config::child_itors tips = tips_of_day.child_range("tip");
	if (tips.first != tips.second && preferences::has_upload_log()) {
		std::random_shuffle(tips.first, tips.second);
	}

	//Make sure that the upload log preference is set, if it's not already, so
	//that we know next time we've already seen the message about uploads.
	if(!preferences::has_upload_log()) {
		preferences::set_upload_log(preferences::upload_log());
	}
}
*/
/** Go to the next tips-of-the-day */
/*
static void next_tip_of_day(config& tips_of_day, bool reverse = false)
{
	// we just rotate the tip list, to avoid the need to keep track
	// of the current one, and keep it valid, cycle it, etc...
	config::child_itors tips = tips_of_day.child_range("tip");
	if (tips.first != tips.second) {
		config::child_iterator direction = reverse ? tips.first+1 : tips.second-1;
		std::rotate(tips.first, direction, tips.second);
	}
}
*/
/** Return the text for one of the tips-of-the-day. */
/*
static const config* get_tip_of_day(config& tips_of_day)
{
	if (tips_of_day.empty()) {
		read_tips_of_day(tips_of_day);
	}

	const config::child_list& tips = tips_of_day.get_children("tip");

	// next_tip_of_day rotate tips, so better stay iterator-safe
	for (size_t t=0; t < tips.size(); t++, next_tip_of_day(tips_of_day)) {
		const config* tip = tips.front();
		if (tip == NULL) continue;

		const std::vector<std::string> needed_units = utils::split((*tip)["encountered_units"],',');
		if (needed_units.empty()) {
			return tip;
		}
		const std::set<std::string>& seen_units = preferences::encountered_units();

		// test if one of the listed unit types is already encountered
		// if if's a number, test if we have encountered more than this
		for (std::vector<std::string>::const_iterator i = needed_units.begin();
				i != needed_units.end(); i++) {
			int needed_units_nb = lexical_cast_default<int>(*i,-1);
			if (needed_units_nb !=-1) {
				if (needed_units_nb <= static_cast<int>(seen_units.size())) {
					return tip;
				}
			} else if (seen_units.find(*i) != seen_units.end()) {
				return tip;
			}
		}
	}
	// not tip match, someone forget to put an always-match one
	return NULL;
}
*/
/**
 *  Show one tip-of-the-day in a frame on the titlescreen.
 *  This frame has 2 buttons: Next-Tip, and Show-Help.
 */
/*
static void draw_tip_of_day(game_display& screen,
							config& tips_of_day,
							const gui::dialog_frame::style& style,
							gui::button* const previous_tip_button,
							gui::button* const next_tip_button,
							gui::button* const help_tip_button,
							const SDL_Rect* const main_dialog_area,
							surface_restorer& tip_of_day_restorer)
{
	if(preferences::show_tip_of_day() == false) {
		return;
	}

    // Restore the previous tip of day area to its old state (section of the title image).
    tip_of_day_restorer.restore();

    // Draw tip of the day
    const config* tip = get_tip_of_day(tips_of_day);
    if(tip != NULL) {
    	int tip_width = game_config::title_tip_width * screen.w() / 1024;

		try {
	        const std::string& text =
				font::word_wrap_text((*tip)["text"], font::SIZE_NORMAL, tip_width);
			const std::string& source =
				font::word_wrap_text((*tip)["source"], font::SIZE_NORMAL, tip_width);

			const int pad = game_config::title_tip_padding;

			SDL_Rect area = font::text_area(text,font::SIZE_NORMAL);
			area.w = tip_width;
			SDL_Rect source_area = font::text_area(source, font::SIZE_NORMAL, TTF_STYLE_ITALIC);
			area.w = std::max<size_t>(area.w, source_area.w) + 2*pad;
			area.h += source_area.h + next_tip_button->location().h + 3*pad;

			area.x = main_dialog_area->x - (game_config::title_tip_x * screen.w() / 1024) - area.w;
			area.y = main_dialog_area->y + main_dialog_area->h - area.h;

			// Note: The buttons' locations need to be set before the dialog frame is drawn.
			// Otherwise, when the buttons restore their area, they
			// draw parts of the old dialog frame at their old locations.
			// This way, the buttons draw a part of the title image,
			// because the call to restore above restored the area
			// of the old tip of the day to its initial state (the title image).
			int button_x = area.x + area.w - next_tip_button->location().w - pad;
			int button_y = area.y + area.h - pad - next_tip_button->location().h;
			next_tip_button->set_location(button_x, button_y);
			next_tip_button->set_dirty(); //force redraw even if location did not change.

			button_x -= previous_tip_button->location().w + pad;
			previous_tip_button->set_location(button_x, button_y);
			previous_tip_button->set_dirty();

			button_x = area.x + pad;
			if (help_tip_button != NULL)
			{
				help_tip_button->set_location(button_x, button_y);
				help_tip_button->set_dirty();
			}

			gui::dialog_frame f(screen.video(), "", style, false);
			tip_of_day_restorer = surface_restorer(&screen.video(), f.layout(area).exterior);
			f.draw_background();
			f.draw_border();

			font::draw_text(&screen.video(), area, font::SIZE_NORMAL, font::NORMAL_COLOUR,
							 text, area.x + pad, area.y + pad);
			// todo
			font::draw_text(&screen.video(), area, font::SIZE_NORMAL, font::NORMAL_COLOUR,
							 source, area.x + area.w - source_area.w - pad,
							 next_tip_button->location().y - source_area.h - pad,
							 false, TTF_STYLE_ITALIC);
		} catch (utils::invalid_utf8_exception&) {
			LOG_STREAM(err, engine) << "Invalid utf-8 found, tips of day aren't drawn.\n";
			return;
		}

	    LOG_DP << "drew tip of day\n";
	}
}
*/

SDL_TextureID title_tex = 0;

/**
 *  Draw the map image background, revision number
 *  and fade the log the first time
 */
static void draw_background(game_display& screen)
{
	static int bgNum = -1;		// KP: the background will only change once per game load
	
	//bool fade_failed = false;
	
	// +d+
//	surface const target(screen.video().getSurface());
	
//	do 
	if (!title_tex)
	{
//		int logo_x = game_config::title_logo_x * screen.w() / 1024,
//			logo_y = game_config::title_logo_y * screen.h() / 768;

		/*Select a random game_title*/
		std::string titles = "data/campaigns/Heir_To_The_Throne/images/story/httt_story2.jpg, data/campaigns/Heir_To_The_Throne/images/story/httt_story4.jpg, data/campaigns/Heir_To_The_Throne/images/story/httt_story5.jpg, data/campaigns/Heir_To_The_Throne/images/story/httt_story6.jpg";
		std::vector<shared_string> game_title_list =
			utils::split_shared(titles /*game_config::game_title*/, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);

		if(game_title_list.empty()) {
			ERR_CONFIG << "No title image defined\n";
		} else {
			if (bgNum == -1)
				bgNum = rand()%game_title_list.size();
				// KP: do not cache this
				//surface const title_surface(scale_opaque_surface(image::get_image(game_title_list[bgNum], image::UNSCALED, false),screen.w(), screen.h()));
				surface title_surface(image::get_image(game_title_list[bgNum], image::UNSCALED, false));

			if (title_surface.null()) {
				ERR_DP << "Could not find title image\n";
			} else {
				//screen.video().blit_surface(0, 0, title_surface);
				title_tex = SDL_CreateTextureFromSurface(SDL_PIXELFORMAT_ABGR8888, title_surface);
				// (surface is freed when it goes out of scope...)
				title_surface.assign(NULL);
				//update_rect(screen_area());
				LOG_DP << "displayed title image\n";
			}
		}

//		fade_failed = !fade_logo(screen, logo_x, logo_y);
	} 
//	while (fade_failed);
	
	SDL_RenderCopy(title_tex, NULL, NULL, DRAW);
	LOG_DP << "faded logo\n";

	// Display Wesnoth version and (if possible) revision
/*	const std::string& version_str = _("Version") +
		std::string(" ") + game_config::revision;

	const SDL_Rect version_area = font::draw_text(NULL, screen_area(),
								  font::SIZE_TINY, font::NORMAL_COLOUR,
								  version_str,0,0);
	const size_t versiony = screen.h() - version_area.h;

	if(versiony < size_t(screen.h())) {
		draw_solid_tinted_rectangle(0, versiony - 2, version_area.w + 3, version_area.h + 2,0,0,0,0.75,screen.video().getSurface());
		font::draw_text(&screen.video(),screen.screen_area(),
				font::SIZE_TINY, font::NORMAL_COLOUR,
				version_str,0,versiony);
	}

	LOG_DP << "drew version number\n";
*/ 
}

void free_title(void)
{
	SDL_DestroyTexture(title_tex);
	title_tex = 0;
}


namespace {

/**
 *  Handler for forcing a discrete ESC keypress to quit the game (bug #12747)
 *  This hack is here because the GUI code used here doesn't handle this yet.
 *  Once it does, revert this part of r31758.
 */
class titlescreen_handler : public events::handler
{
public:
	titlescreen_handler(bool ignore_esc = false)
		: handler(), ignore_esc_(ignore_esc)
	{
		if(ignore_esc) {
			LOG_DP << "ignoring held ESCAPE key\n";
		}
	}

	bool get_esc_ignore() { return ignore_esc_; }
	void set_esc_ignore(bool ignore) { ignore_esc_ = ignore; }

	virtual void handle_event(const SDL_Event& event)
	{
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE) {
			ignore_esc_ = false;
			LOG_DP << "ESCAPE key no longer ignored\n";
		}
	}

private:
	bool ignore_esc_;
};

}	// end anon namespace

namespace gui {


static bool background_is_dirty_ = true;
void set_background_dirty() {
	background_is_dirty_ = true;
}


TITLE_RESULT show_title(game_display& screen, config& tips_of_day)
{
	disableTerrainCache();
	
	free_all_caches();
	
	cursor::set(cursor::NORMAL);

	const preferences::display_manager disp_manager(&screen);
	const hotkey::basic_handler key_handler(&screen);

	const font::floating_label_context label_manager;

	screen.video().modeChanged(); // resets modeChanged value

//	if (background_is_dirty_) 
	{
		draw_background(screen);
	}

	//- Texts for the menu-buttons.
	//- Members of this array must correspond to the enumeration TITLE_RESULT
	static const char* button_labels[] = {
					       //N_("TitleScreen button^Tutorial"),
					       N_("TitleScreen button^Campaign"),
#ifndef FREE_VERSION		
							N_("TitleScreen button^Skirmish"),
					       N_("TitleScreen button^Multiplayer"),
#endif
					       N_("TitleScreen button^Load"),
//					       N_("TitleScreen button^Add-ons"),
//#ifndef DISABLE_EDITOR2
					       //N_("TitleScreen button^Map Editor"),
//#endif
					       //N_("TitleScreen button^Language"),
					       N_("TitleScreen button^Preferences"),
							N_("Sync Saves"),
#ifndef DISABLE_OPENFEINT
							N_("TitleScreen button^OpenFeint"),
#endif
					       N_("TitleScreen button^Help"),
					       //N_("TitleScreen button^Quit"),
								// Only the above buttons go into the menu-frame
								// Next 2 buttons go into frame for the tip-of-the-day:
					       N_("TitleScreen button^Previous"),
					       N_("TitleScreen button^Next"),
					       //N_("TitleScreen button^Help"),
								// Next entry is no button, but shown as a mail-icon instead:
					       //N_("TitleScreen button^Help Wesnoth") 
							};
	//- Texts for the tooltips of the menu-buttons
	static const char* help_button_labels[] = { 
							//N_("Start a tutorial to familiarize yourself with the game"),
						    N_("Start a new single player campaign"),
#ifndef FREE_VERSION
							N_("Play a single scenario against the AI"),
						    N_("Play multiplayer (hotseat or Internet)"),
#endif
						    N_("Load a saved game"),
//						    N_("Download usermade campaigns, eras, or map packs"),
//#ifndef DISABLE_EDITOR2
						    //N_("Start the map editor"),
//#endif
						    //N_("Change the language"),
						    N_("Configure the game's settings"),
							N_("Sync saved games"),
#ifndef DISABLE_OPENFEINT
							N_("Launch the OpenFeint dashboard"),
#endif

							N_("Show Battle for Wesnoth help"),
							//N_("Quit the game"),
						    N_("Show next tip of the day"),
						    //N_("Upload statistics") 
							};

	//static const size_t nbuttons = sizeof(button_labels)/sizeof(*button_labels);
#ifdef FREE_VERSION	
	int nbuttons = 10;
#else
	int nbuttons = 8;
#endif
	
#ifdef DISABLE_OPENFEINT
	nbuttons--;
#endif
	
#ifndef __IPAD__
	nbuttons--;	// because help is off to the left now
#endif
	
	int menu_xbase = CVideo::getx()-108-10; //380; //(game_config::title_buttons_x*screen.w())/1024;
	const int menu_xincr = 0;

#ifdef USE_TINY_GUI
	//const int menu_ybase = 15; //(330*screen.h())/768 - 50; //15;
	const int menu_yincr = 36+5; //(35*3)/4;//15;
	const int menu_ybase = (screen.h() - (36+5)*nbuttons - 5) / 2 + 4;
#else
	const int menu_ybase = (screen.h() - (36+5)*nbuttons - 5) / 2 + 4;
	const int menu_yincr = 36+5;
#endif
	
#ifdef __IPAD__
	menu_xbase -= 10;
#endif

	const int padding = 4; //game_config::title_buttons_padding;

	std::vector<button> buttons;
	size_t b, max_width = 0;
	size_t n_menubuttons = 0;
	for(b = 0; b != nbuttons; ++b) 
	{
//#ifdef __IPHONEOS__		
//		if (b + TUTORIAL == TUTORIAL || b + TUTORIAL == START_MAP_EDITOR || b+TUTORIAL == CHANGE_LANGUAGE)
//			continue;
//#endif		
		buttons.push_back(button(screen.video(),sgettext(button_labels[b])));
		buttons.back().set_help_string(sgettext(help_button_labels[b]));
		max_width = std::max<size_t>(max_width,buttons.back().width());

		n_menubuttons = b;
#ifdef __IPAD__
		if(b + NEW_CAMPAIGN == SHOW_HELP) break;
#else
		if(b + NEW_CAMPAIGN == SHOW_OPENFEINT) break;		
#endif
	}

	SDL_Rect main_dialog_area = {menu_xbase-padding, menu_ybase-padding, max_width+padding*2,
								 menu_yincr*(n_menubuttons)+buttons.back().height()+padding*2};

	gui::dialog_frame main_frame(screen.video(), "", gui::dialog_frame::titlescreen_style, false);
	main_frame.layout(main_dialog_area);

	// we only redraw transparent parts when asked,
	// to prevent alpha growing
	if (background_is_dirty_) {
		main_frame.draw_background();
		main_frame.draw_border();
	}

	int i=0;
	for(b = 0; b != nbuttons; ++b) 
	{
//#ifdef __IPHONEOS__		
//		if (b + TUTORIAL == TUTORIAL || b + TUTORIAL == START_MAP_EDITOR || b+TUTORIAL == CHANGE_LANGUAGE)
//			continue;
//#endif		
		buttons[i].set_width(max_width);
		buttons[i].set_location(menu_xbase + i*menu_xincr, menu_ybase + i*menu_yincr);
#ifdef __IPAD__
		if(b + NEW_CAMPAIGN == SHOW_HELP) break;
#else
		if(b + NEW_CAMPAIGN == SHOW_OPENFEINT) break;		
#endif
		i++;
	}
	
#ifndef __IPAD__
	buttons.push_back(button(screen.video(),sgettext(button_labels[i+1])));
	buttons.back().set_help_string(sgettext(help_button_labels[i+1]));
	buttons[i+1].set_width(max_width);
	buttons[i+1].set_location(5, 320-menu_yincr);	
#endif

//	b = TIP_PREVIOUS - NEW_CAMPAIGN;
//	gui::button previous_tip_button(screen.video(),sgettext(button_labels[b]),button::TYPE_PRESS,"lite_small");
//	previous_tip_button.set_help_string( sgettext(button_labels[b] ));

//	b = TIP_NEXT - NEW_CAMPAIGN;
//	gui::button next_tip_button(screen.video(),sgettext(button_labels[b]),button::TYPE_PRESS,"lite_small");
//	next_tip_button.set_help_string( sgettext(button_labels[b] ));

//	b = SHOW_HELP - NEW_CAMPAIGN;
//	gui::button help_tip_button(screen.video(),sgettext(button_labels[b]),button::TYPE_PRESS,"lite_small");
//	help_tip_button.set_help_string( sgettext(button_labels[b] ));

//	gui::button beg_button(screen.video(),_("Help Wesnoth"),button::TYPE_IMAGE,"menu-button",button::MINIMUM_SPACE);
//	beg_button.set_help_string(_("Help Wesnoth by sending us information"));

//	next_tip_of_day(tips_of_day);

//	surface_restorer tip_of_day_restorer;

//	draw_tip_of_day(screen, tips_of_day, gui::dialog_frame::titlescreen_style,
//					&previous_tip_button, &next_tip_button, NULL/*&help_tip_button*/, &main_dialog_area, tip_of_day_restorer);

//	const int pad = game_config::title_tip_padding;
//	beg_button.set_location(screen.w() - pad - beg_button.location().w,
//		screen.h() - pad - beg_button.location().h);
	events::raise_draw_event();

	LOG_DP << "drew buttons dialog\n";
	
	
	// draw logo over everything
#ifdef __IPAD__	
	std::string path = game_config::path + "/data/core/images/misc/logo.png";
#else
	std::string path = game_config::path + "/data/core/images/misc/logo_small.png";
#endif	
	surface logo_surface = IMG_Load(path.c_str());
	//blit_surface(480-logo_surface.get()->w, 0, logo_surface);
	
	

	CKey key;

	size_t keyboard_button = nbuttons;
	bool key_processed = false;

//	update_whole_screen();
	background_is_dirty_ = false;

	titlescreen_handler ts_handler(key[SDLK_ESCAPE] != 0); //!= 0 to avoid a MSVC warning C4800

	LOG_DP << "entering interactive loop...\n";
	
	
	memory_stats("At titlescreen");
	
	// Initialize OpenFeint now
	of_init();

	
	for(;;) {
		events::pump();
		for(size_t b = 0; b != buttons.size(); ++b) {
			if(buttons[b].pressed()) {
				free_title();
				return static_cast<TITLE_RESULT>(b + NEW_CAMPAIGN);
			}
		}

/*		if(previous_tip_button.pressed()) {
			next_tip_of_day(tips_of_day, true);
			draw_tip_of_day(screen, tips_of_day, gui::dialog_frame::titlescreen_style,
						&previous_tip_button, &next_tip_button, &help_tip_button, &main_dialog_area, tip_of_day_restorer);
		}
		if(next_tip_button.pressed()) {
			next_tip_of_day(tips_of_day, false);
			draw_tip_of_day(screen, tips_of_day, gui::dialog_frame::titlescreen_style,
						&previous_tip_button, &next_tip_button, &help_tip_button, &main_dialog_area, tip_of_day_restorer);
		}
*/
//		if(help_tip_button.pressed()) {
//			return SHOW_HELP;
//		}
//		if(beg_button.pressed()) {
//			return BEG_FOR_UPLOAD;
//		}
		if (key[SDLK_UP]) {
			if (!key_processed) {
				buttons[keyboard_button].set_active(false);
				if (keyboard_button == 0) {
					keyboard_button = nbuttons - 1;
				} else {
					keyboard_button--;
				}
				key_processed = true;
				buttons[keyboard_button].set_active(true);
			}
		} else if (key[SDLK_DOWN]) {
			if (!key_processed) {
				buttons[keyboard_button].set_active(false);
				if (keyboard_button > nbuttons - 1) {
					keyboard_button = 0;
				} else {
					keyboard_button++;
				}
				key_processed = true;
				buttons[keyboard_button].set_active(true);
			}
		} else {
			key_processed = false;
		}

		events::raise_process_event();
		
		
		// KP: redraw
		draw_background(screen);
		main_frame.draw_background();
		main_frame.draw_border();
		events::raise_draw_event();
		//blit_surface(480-logo_surface.get()->w, 0, logo_surface);
#ifdef __IPAD__
		blit_surface(-10, 0, logo_surface);
#else
		blit_surface(-15, -10, logo_surface);
#endif
		
		screen.flip();

//		if (key[SDLK_ESCAPE] && !ts_handler.get_esc_ignore())
//			return QUIT_GAME;
		if (key[SDLK_F5])
			return RELOAD_GAME_DATA;

		if (key[SDLK_RETURN] && keyboard_button < nbuttons) {
			return static_cast<TITLE_RESULT>(keyboard_button + NEW_CAMPAIGN);
		}



		// If the resolution has changed due to the user resizing the screen,
		// or from changing between windowed and fullscreen:
		if(screen.video().modeChanged()) {
			return REDRAW_BACKGROUND;
		}
		
		screen.delay(10);
	}

	free_title();
	return REDRAW_BACKGROUND;
}

} // namespace gui

//.
