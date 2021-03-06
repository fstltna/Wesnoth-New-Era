/* $Id: dialogs.cpp 34873 2009-04-13 15:17:37Z silene $ */
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
 * @file dialogs.cpp
 * Various dialogs: advance_unit, show_objectives, save+load game, network::connection.
 */

#include "global.hpp"

#include "dialogs.hpp"
#include "game_events.hpp"
#include "game_preferences.hpp"
#include "gettext.hpp"
#include "help.hpp"
#include "language.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "marked-up_text.hpp"
#include "menu_events.hpp"
#include "mouse_handler_base.hpp"
#include "minimap.hpp"
#include "replay.hpp"
#include "thread.hpp"
#include "wml_separators.hpp"
#include "widgets/progressbar.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"


//#ifdef _WIN32
//#include "locale.h"
//#endif

#include <clocale>

#define LOG_NG LOG_STREAM(info, engine)
#define LOG_DP LOG_STREAM(info, display)
#define ERR_G  LOG_STREAM(err, general)
#define ERR_CF LOG_STREAM(err, config)


#import "UnitDetails.h"

UnitDetails *unitDetailsController;
extern UIView *gLandscapeView;
//extern bool gPauseForOpenFeint;
extern bool gRedraw;

extern const unit_type* gUnitType;
const unit_type* gUnitType;

namespace dialogs
{

void advance_unit(const gamemap& map,
                  unit_map& units,
                  map_location loc,
                  game_display& gui,
		  bool random_choice,
		  const bool add_replay_event)
{
	unit_map::iterator u = units.find(loc);
	if(u == units.end() || u->second.advances() == false)
		return;

	LOG_DP << "advance_unit: " << u->second.type_id() << "\n";

	const std::vector<shared_string>& options = u->second.advances_to();

	std::vector<shared_string> lang_options;

	std::vector<unit> sample_units;
	for(std::vector<shared_string>::const_iterator op = options.begin(); op != options.end(); ++op) {
		sample_units.push_back(::get_advanced_unit(units,loc,*op));
		const unit& type = sample_units.back();

#ifdef LOW_MEM
		lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + COLUMN_SEPARATOR + type.type_name());
#else
		lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u->second.image_mods() + COLUMN_SEPARATOR + type.type_name());
#endif
		preferences::encountered_units().insert(*op);
	}

	const config::child_list& mod_options = u->second.get_modification_advances();
	bool always_display = false;
	for(config::child_list::const_iterator mod = mod_options.begin(); mod != mod_options.end(); ++mod) {
		if (utils::string_bool((**mod)["always_display"])) always_display = true;
		sample_units.push_back(::get_advanced_unit(units,loc,u->second.type_id()));
		sample_units.back().add_modification("advance",**mod);
		const unit& type = sample_units.back();
		if((**mod)["image"].size()){
		  lang_options.push_back(IMAGE_PREFIX + (**mod)["image"] + COLUMN_SEPARATOR + (**mod)["description"]);
		}else{
#ifdef LOW_MEM
		  lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + COLUMN_SEPARATOR + (**mod)["description"]);
#else
		  lang_options.push_back(IMAGE_PREFIX + type.absolute_image() + u->second.image_mods() + COLUMN_SEPARATOR + (**mod)["description"]);
#endif
		}
	}

	LOG_DP << "options: " << options.size() << "\n";

	int res = 0;

	if(lang_options.empty()) {
		return;
	} else if(random_choice) {
		res = rand()%lang_options.size();
	} else if(lang_options.size() > 1 || always_display) {

		units_list_preview_pane unit_preview(gui,&map,sample_units);
		std::vector<gui::preview_pane*> preview_panes;
		preview_panes.push_back(&unit_preview);

		gui::dialog advances = gui::dialog(gui,
				      _("Advance Unit"),
		                      _(""),
		                      gui::OK_ONLY);
		advances.set_menu(lang_options);
		advances.set_panes(preview_panes);
		res = advances.show();
	}

	if(add_replay_event) {
		recorder.add_advancement(loc);
	}

	recorder.choose_option(res);

	LOG_DP << "animating advancement...\n";
	animate_unit_advancement(units,loc,gui,size_t(res));

	// In some rare cases the unit can have enough XP to advance again,
	// so try to do that.
	// Make sure that we don't enter an infinite level loop.
	u = units.find(loc);
	if(u != units.end()) {
		// Level 10 unit gives 80 XP and the highest mainline is level 5
		if(u->second.experience() < 81) {
			// For all leveling up we have to add advancement to replay here because replay
			// doesn't handle multi advancemnet
			advance_unit(map, units, loc, gui, random_choice, true);
		} else {
			LOG_STREAM(err, config) << "Unit has an too high amount of " << u->second.experience()
				<< " XP left, cascade leveling disabled\n";
		}
	} else {
		LOG_STREAM(err, engine) << "Unit advanced no longer exists\n";
	}
}

bool animate_unit_advancement(unit_map& units, map_location loc, game_display& gui, size_t choice)
{
	const events::command_disabler cmd_disabler;

	unit_map::iterator u = units.find(loc);
	if(u == units.end() || u->second.advances() == false) {
		return false;
	}

	const std::vector<shared_string>& options = u->second.advances_to();
	const config::child_list& mod_options = u->second.get_modification_advances();

	if(choice >= options.size() + mod_options.size()) {
		return false;
	}

	// When the unit advances, it fades to white, and then switches
	// to the new unit, then fades back to the normal colour

	if(!gui.video().update_locked()) {
		unit_animator animator;
		animator.add_animation(&u->second,"levelout",u->first);
		animator.start_animations();
		animator.wait_for_end();
	}

	if(choice < options.size()) {
		const std::string& chosen_unit = options[choice];
		::advance_unit(units,loc,chosen_unit);
	} else {
		unit amla_unit(u->second);
		config mod_option(*mod_options[choice - options.size()]);

		LOG_NG << "firing advance event (AMLA)\n";
		game_events::fire("advance",loc);

		amla_unit.get_experience(-amla_unit.max_experience()); // subtract xp required
		amla_unit.add_modification("advance",mod_option);

		// KP: check of heroic AMLA unit
		if (amla_unit.side() == preferences::get_player_side())
		{
			if (amla_unit.level() == 4 || u->second.level() == 3)
				earn_achievement(ACHIEVEMENT_HEROIC_UNIT);
			else if (amla_unit.level() == 3)
				earn_achievement(ACHIEVEMENT_VETERAN_UNIT);
			if (amla_unit.can_recruit() && amla_unit.name() != "Galas")	// IftU part 2 advances you for free right at the start...
				earn_achievement(ACHIEVEMENT_FEARLESS_LEADER);
		}
		
		
		units.replace(loc, amla_unit);

		LOG_NG << "firing post_advance event (AMLA)\n";
		game_events::fire("post_advance",loc);
	}

	u = units.find(loc);
	gui.invalidate_unit();

	if(u != units.end() && !gui.video().update_locked()) {
		unit_animator animator;
		animator.add_animation(&u->second,"levelin",u->first);
		animator.start_animations();
		animator.wait_for_end();
		animator.set_all_standing();
		gui.invalidate(loc);
		gui.draw();
		events::pump();
	}

	gui.invalidate_all();
	gui.draw();

	return true;
}

void show_objectives(game_display& disp, const config& level, const std::string& objectives)
{
	static const std::string no_objectives(_("No objectives available"));
	const std::string& name = level["name"];
	shared_string campaign_name = level["campaign"];
	replace_underbar2space(campaign_name);

	gui::message_dialog(disp, "", "*~" 
						+ name +
			(campaign_name.empty() ? "\n" : " - " + campaign_name + "\n") +
	                (objectives.empty() ? no_objectives : objectives)
	                ).show();
}

bool is_illegal_file_char(char c)
{
	return c == '/' || c == '\\' || c == ':'
 	#ifdef _WIN32
	|| c == '?' || c == '|' || c == '<' || c == '>' || c == '*' || c == '"'
	#endif
	;
}

int get_save_name(display & disp,const std::string& message, const std::string& txt_label,
				  std::string* fname, gui::DIALOG_TYPE dialog_type, const std::string& title,
				  const bool has_exit_button, const bool ask_for_filename)
{
	static int quit_prompt = 0;
	std::string tmp_title = title;
	if (tmp_title.empty()) tmp_title = _("Save Game");
	bool ignore_opt = false;
	int overwrite=0;
	int res=0;
	bool ask = ask_for_filename;
	do {
		if (ask) {
			gui::dialog d(disp, tmp_title, message, dialog_type);
			d.set_textbox(txt_label, *fname);
			if(has_exit_button) {
				d.add_button(new gui::dialog_button(disp.video(), _("Quit Game"),
					gui::button::TYPE_PRESS, 2), gui::dialog::BUTTON_STANDARD);
				if(quit_prompt < 0) {
					res = 1;
				} else if(quit_prompt > 5) {
					d.add_button(new gui::dialog_button(disp.video(), _("Ignore All"),
						gui::button::TYPE_CHECK), gui::dialog::BUTTON_CHECKBOX);
					res = d.show();
					ignore_opt = d.option_checked();
				} else {
					res = d.show();
					if(res == 1) {
						++quit_prompt;
					} else {
						quit_prompt = 0;
					}
				}
			} else {
				res = d.show();
			}
			*fname = d.textbox_text();
		} else {
			ask = true;
		}

		if (std::count_if(fname->begin(),fname->end(),is_illegal_file_char)) {
			gui::message_dialog(disp, _("Error"),
				_("Save names may not contain colons, slashes, or backslashes. "
				"Please choose a different name.")).show();
			overwrite = 1;
			continue;
		}

		if (is_gzip_file(*fname)) {
			gui::message_dialog(disp, _("Error"),
				_("Save names should not end on '.gz'. "
				"Please choose a different name.")).show();
			overwrite = 1;
			continue;
		}

		if (res == 0 && save_game_exists(*fname)) {
			std::stringstream s;
			s << _("Save already exists. Do you want to overwrite it?")
			  << std::endl << _("Name: ") << *fname;
			overwrite = gui::dialog(disp,_("Overwrite?"),
			    s.str(), gui::YES_NO).show();
		} else {
			overwrite = 0;
		}
	} while ((res == 0) && (overwrite != 0));

	if(ignore_opt) {
		quit_prompt = -1;
	}
	return res;
}

namespace {

/** Class to handle deleting a saved game. */
class delete_save : public gui::dialog_button_action
{
public:
	delete_save(display& disp, gui::filter_textbox& filter, std::vector<save_info>& saves, std::vector<config*>& save_summaries) : disp_(disp), saves_(saves), summaries_(save_summaries), filter_(filter) {}
private:
	gui::dialog_button_action::RESULT button_pressed(int menu_selection);

	display& disp_;
	std::vector<save_info>& saves_;
	std::vector<config*>& summaries_;
	gui::filter_textbox& filter_;
};

gui::dialog_button_action::RESULT delete_save::button_pressed(int menu_selection)
{
	const size_t index = size_t(filter_.get_index(menu_selection));
	if(index < saves_.size()) {

		// See if we should ask the user for deletion confirmation
		if(preferences::ask_delete_saves()) {
			gui::dialog dmenu(disp_,"",
					       _("Do you really want to delete this game?"),
					       gui::YES_NO);
			dmenu.add_option(_("Don't ask me again!"), false);
			const int res = dmenu.show();
			// See if the user doesn't want to be asked this again
			if(dmenu.option_checked()) {
				preferences::set_ask_delete_saves(false);
			}

			if(res != 0) {
				return gui::CONTINUE_DIALOG;
			}
		}

		// Remove the item from filter_textbox memory
		filter_.delete_item(menu_selection);

		// Delete the file
		delete_game(saves_[index].name);

		// Remove it from the list of saves
		saves_.erase(saves_.begin() + index);

		if(index < summaries_.size()) {
			summaries_.erase(summaries_.begin() + index);
		}

		return gui::DELETE_ITEM;
	} else {
		return gui::CONTINUE_DIALOG;
	}
}

static const int save_preview_border = 10;

class save_preview_pane : public gui::preview_pane
{
public:
	save_preview_pane(CVideo &video, const config& game_config, gamemap* map,
			const std::vector<save_info>& info,
			const std::vector<config*>& summaries,
			const gui::filter_textbox& textbox) :
		gui::preview_pane(video),
		game_config_(&game_config),
		map_(map), info_(&info),
		summaries_(&summaries),
		index_(0),
		map_cache_(),
		skip_map_(false),
		textbox_(textbox)
	{
#ifdef __IPHONEOS__
		set_measurements(130, 220);
#else
		set_measurements(std::min<int>(200,video.getx()/4),
				 std::min<int>(400,video.gety() * 4/5));
#endif	
	}

	void draw_contents();
	void set_selection(int index) {
		index_ = textbox_.get_index(index);
		set_dirty();
	}

	bool left_side() const { return true; }

private:
	const config* game_config_;
	gamemap* map_;
	bool skip_map_;
	const std::vector<save_info>* info_;
	const std::vector<config*>* summaries_;
	int index_;
	std::map<std::string,surface> map_cache_;
	const gui::filter_textbox& textbox_;
};

void save_preview_pane::draw_contents()
{
	if (size_t(index_) >= summaries_->size() || info_->size() != summaries_->size()) {
		return;
	}

	std::string dummy;
	config& summary = *(*summaries_)[index_];
	if (summary["label"] == ""){
		try {
			load_game_summary((*info_)[index_].name, summary, &dummy);
			*(*summaries_)[index_] = summary;
		} catch(game::load_game_failed&) {
			summary["corrupt"] = "yes";
		}
	}

//	surface const screen = video().getSurface();

	SDL_Rect const &loc = location();
	const SDL_Rect area = { loc.x + save_preview_border, loc.y + save_preview_border,
	                        loc.w - save_preview_border * 2, loc.h - save_preview_border * 2 };
	SDL_Rect clip_area = area;
	const clip_rect_setter clipper(/*screen,*/clip_area);

	int ypos = area.y;

	const unit_type_data::unit_type_map::const_iterator leader = unit_type_data::types().find_unit_type(summary["leader"]);
	if(unit_type_data::types().unit_type_exists(summary["leader"])) {

#ifdef LOW_MEM
		const surface image(image::get_image(leader->second.image()));
#else
		const surface image(image::get_image(leader->second.image() + "~RC(" + leader->second.flag_rgb() + ">1)"));
#endif

		if(image != NULL) {
			SDL_Rect image_rect = {area.x,area.y,image->w,image->h};
			ypos += image_rect.h + save_preview_border;

			//SDL_BlitSurface(image,NULL,screen,&image_rect);
			blit_surface(image_rect.x, image_rect.y, image);
		}
	}

	shared_string map_data = summary["map_data"];
	if(map_data.empty() && !skip_map_) {
		const config* const scenario = game_config_->find_child(summary["campaign_type"],"id",summary["scenario"]);
		if(scenario != NULL && scenario->find_child("side","shroud","yes") == NULL) {
			map_data = (*scenario)["map_data"];
			if(map_data.empty() && (*scenario)["map"].empty() == false) {
				try {
					map_data = read_map((*scenario)["map"]);
				} catch(io_exception& e) {
					ERR_G << "could not read map '" << (*scenario)["map"] << "': " << e.what() << "\n";
				}
			}
		}
	}

	surface map_surf(NULL);

	if(map_data.empty() == false && !skip_map_) {
		const std::map<std::string,surface>::const_iterator itor = map_cache_.find(map_data);
		if(itor != map_cache_.end()) {
			map_surf = itor->second;
		} else if(map_ != NULL) {
			try {
#ifdef USE_TINY_GUI
				const int minimap_size = 60;
#else
				const int minimap_size = 100;
#endif
				map_->read(map_data);

				map_surf = image::getMinimap(minimap_size, minimap_size, *map_);
				if(map_surf != NULL) {
					map_cache_.insert(std::pair<std::string,surface>(map_data,surface(map_surf)));
				}
			} catch(incorrect_map_format_exception& e) {
				//ERR_CF << "map could not be loaded: " << e.msg_ << '\n';
				skip_map_ = true;
			} catch(twml_exception& e) {
				//ERR_CF << "map could not be loaded: " << e.dev_message << '\n';
				skip_map_ = true;
			}
		}
	}

	if(map_surf != NULL) {
		SDL_Rect map_rect = {area.x + area.w - map_surf->w,area.y,map_surf->w,map_surf->h};
		ypos = std::max<int>(ypos,map_rect.y + map_rect.h + save_preview_border);
		//SDL_BlitSurface(map_surf,NULL,screen,&map_rect);
		blit_surface(map_rect.x, map_rect.y, map_surf);
	}

	char* old_locale = std::setlocale(LC_TIME, get_locale().localename.c_str());
	char time_buf[256] = {0};
	const save_info& save = (*info_)[index_];
	tm* tm_l = localtime(&save.time_modified);
	if (tm_l) {
		const size_t res = strftime(time_buf,sizeof(time_buf),_("%a %b %d %H:%M %Y"),tm_l);
		if(res == 0) {
			time_buf[0] = 0;
		}
	} else {
		LOG_NG << "localtime() returned null for time " << save.time_modified << ", save " << save.name;
	}

	if(old_locale) {
		std::setlocale(LC_TIME, old_locale);
	}

	std::stringstream str;

	// Escape all special characters in filenames
	std::string name = (*info_)[index_].name;
	str << font::BOLD_TEXT << utils::escape(name) << "\n" << time_buf;

	const std::string& campaign_type = summary["campaign_type"];
	if(utils::string_bool(summary["corrupt"], false)) {
		str << "\n" << _("#(Invalid)");
	} else if (!campaign_type.empty()) {
		str << "\n";

		if(campaign_type == "scenario") {
			const std::string campaign_id = summary["campaign"];
			const config* campaign = campaign_id.empty() ? NULL : game_config_->find_child("campaign", "id", campaign_id);
			utils::string_map symbols;
			if (campaign != NULL) {
				symbols["campaign_name"] = (*campaign)["name"];
			} else {
				// Fallback to nontranslatable campaign id.
				symbols["campaign_name"] = "(" + campaign_id + ")";
			}
			str << vgettext("Campaign: $campaign_name", symbols);

			// Display internal id for debug purposes if we didn't above
			if (game_config::debug && (campaign != NULL)) {
				str << '\n' << "(" << campaign_id << ")";
			}
		} else if(campaign_type == "multiplayer") {
			str << _("Multiplayer");
		} else if(campaign_type == "tutorial") {
			str << _("Tutorial");
		} else {
			str << campaign_type;
		}

		str << "\n";

		if(utils::string_bool(summary["replay"], false) && !utils::string_bool(summary["snapshot"], true)) {
			str << _("replay");
		} else if (!summary["turn"].empty()) {
			str << _("Turn") << " " << summary["turn"];
		} else {
			str << _("Scenario Start");
		}

		str << "\n" << _("Difficulty: ") << string_table[summary["difficulty"]];
		if(!summary["version"].empty()) {
			str << "\n" << _("Version: ") << summary["version"];
		}
	}

	font::draw_text(&video(), area, font::SIZE_SMALL, font::NORMAL_COLOUR, str.str(), area.x, ypos, true);
}

std::string format_time_summary(time_t t)
{
	time_t curtime = time(NULL);
	const struct tm* timeptr = localtime(&curtime);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm current_time = *timeptr;

	timeptr = localtime(&t);
	if(timeptr == NULL) {
		return "";
	}

	const struct tm save_time = *timeptr;

	const char* format_string = _("%b %d %y");

	if(current_time.tm_year == save_time.tm_year) {
		const int days_apart = current_time.tm_yday - save_time.tm_yday;
		if(days_apart == 0) {
			// save is from today
			format_string = _("%H:%M");
		} else if(days_apart > 0 && days_apart <= current_time.tm_wday) {
			// save is from this week
#ifdef __IPHONEOS__
			format_string = _("%a %H:%M");
#else			
			format_string = _("%A, %H:%M");
#endif
		} else {
			// save is from current year
			format_string = _("%b %d");
		}
	} else {
		// save is from a different year
		format_string = _("%b %d %y");
	}

	char buf[40];
	const size_t res = strftime(buf,sizeof(buf),format_string,&save_time);
	if(res == 0) {
		buf[0] = 0;
	}

	return buf;
}

} // end anon namespace

std::string load_game_dialog(display& disp, const config& game_config, bool* show_replay, bool* cancel_orders)
{
	// KP: free all cache memory, to make room for parsing the save summaries
	free_all_caches();
	
	std::vector<save_info> games;
	{
		cursor::setter cur(cursor::WAIT);
		games = get_saves_list();
	}

	if(games.empty()) {
		gui::message_dialog(disp,
		                 _("No Saved Games"),
				 _("There are no saved games to load.\n\n(Games are saved automatically when you complete a scenario)")).show();
		return "";
	}

	std::vector<config*> summaries;
	std::vector<save_info>::const_iterator i;
	for(i = games.begin(); i != games.end(); ++i) {
		config& cfg = save_summary(i->name);
		summaries.push_back(&cfg);
	}

	const events::event_context context;

	std::vector<shared_string> items;
	std::ostringstream heading;
	heading << HEADING_PREFIX << _("Name") << COLUMN_SEPARATOR << _("Date");
	items.push_back(heading.str());

	int si = 0;
	for(i = games.begin(); i != games.end(); ++i, si++) {
		std::string name = i->name;
		utils::truncate_as_wstring(name, std::min<size_t>(name.size(), 40));	
		
		std::ostringstream str;
		//str << name << COLUMN_SEPARATOR << format_time_summary(i->time_modified);
		str <<  font::EXTRA_LARGE_TEXT << name << "\n";
//			<< "Campaign: " << (*summaries[si])["campaign"]  << ", " << (*summaries[si])["turn"]  << "\n    "
		

		config &summary = *summaries[si];
		
		// code from save_preview_pane
		const std::string& campaign_type = summary["campaign_type"];
		if(utils::string_bool(summary["corrupt"], false)) 
		{
			str << "\n" << _("#(Invalid)");
		} 
		else if (!campaign_type.empty()) 
		{
			str << "\n";
			
			if(campaign_type == "scenario") {
				utils::string_map symbols;
				std::string temp = summary["campaign"];
				std::replace(temp.begin(),temp.end(),'_',' ');
				symbols["campaign_name"] = temp;
				str << vgettext("Campaign: $campaign_name", symbols);
				
			} else if(campaign_type == "multiplayer") {
				str << _("Multiplayer");
			} else if(campaign_type == "tutorial") {
				str << _("Tutorial");
			} else {
				str << campaign_type;
			}
			
			str << ", ";
			
			if(utils::string_bool(summary["replay"], false) && !utils::string_bool(summary["snapshot"], true)) {
				str << _("replay");
			} else if (!summary["turn"].empty()) {
				str << _("Turn") << " " << summary["turn"];
			} else {
				str << _("Scenario Start");
			}
		}		
		
		
		
		
			str << COLUMN_SEPARATOR << font::EXTRA_LARGE_TEXT << format_time_summary(i->time_modified);
		

		items.push_back(str.str());
	}

	gamemap map_obj(game_config, "");


	gui::dialog lmenu(disp,
			  _("Load Game"),
			  _(""), gui::NULL_DIALOG, gui::dialog::default_style);
	lmenu.set_basic_behavior(gui::OK_CANCEL);

	gui::menu::basic_sorter sorter;
	sorter.set_alpha_sort(0).set_id_sort(1);
	lmenu.set_menu(items, &sorter);

	gui::filter_textbox* filter = new gui::filter_textbox(disp.video(), _("Filter: "), items, items, 1, lmenu);
#ifndef __IPHONEOS__
	lmenu.set_textbox(filter);
#endif
//	save_preview_pane save_preview(disp.video(),game_config,&map_obj,games,summaries,*filter);
//	lmenu.add_pane(&save_preview);
	// create an option for whether the replay should be shown or not
	if(show_replay != NULL) {
//		lmenu.add_option(_("Show replay"), false,
//			game_config::small_gui ? gui::dialog::BUTTON_CHECKBOX : gui::dialog::BUTTON_STANDARD);
	}
#ifndef __IPHONEOS__
	if(cancel_orders != NULL) {
		lmenu.add_option(_("Cancel orders"), false,
			game_config::small_gui ? gui::dialog::BUTTON_STANDARD : gui::dialog::BUTTON_EXTRA);
	}
#endif	
	lmenu.add_button(new gui::standard_dialog_button(disp.video(),_("OK"),0,false), gui::dialog::BUTTON_STANDARD);
	lmenu.add_button(new gui::standard_dialog_button(disp.video(),_("Cancel"),1,true), gui::dialog::BUTTON_STANDARD);

	delete_save save_deleter(disp,*filter,games,summaries);
	gui::dialog_button_info delete_button(&save_deleter,_("Delete Save"));

	lmenu.add_button(delete_button,
#ifdef __IPHONEOS__
		 gui::dialog::BUTTON_HELP);
#else
		game_config::small_gui ? gui::dialog::BUTTON_HELP : gui::dialog::BUTTON_EXTRA);
#endif
	int res = lmenu.show();

	write_save_index();

	if(res == -1)
		return "";

	res = filter->get_index(res);
	int option_index = 0;
	if(show_replay != NULL) {
	  *show_replay = lmenu.option_checked(option_index++);

		const config& summary = *summaries[res];
		if(utils::string_bool(summary["replay"], false) && !utils::string_bool(summary["snapshot"], true)) {
			*show_replay = true;
		}
	}
	if (cancel_orders != NULL) {
		*cancel_orders = lmenu.option_checked(option_index++);
	}

	return games[res].name;
}

namespace {
	static const int unit_preview_border = 10;
}

unit_preview_pane::details::details() :
	image(),
	name(),
	type_name(),
	race(),
	level(0),
	alignment(),
	traits(),
	abilities(),
	hitpoints(0),
	max_hitpoints(0),
	experience(0),
	max_experience(0),
	hp_color(),
	xp_color(),
	movement_left(0),
	total_movement(0),
	attacks()
{
}

unit_preview_pane::unit_preview_pane(game_display& disp, const gamemap* map,
		const gui::filter_textbox* filter, TYPE type, bool on_left_side)
				    : gui::preview_pane(disp.video()), disp_(disp), map_(map), index_(0),
				      details_button_(disp.video(), _("Profile"),
				      gui::button::TYPE_PRESS,"lite_small", gui::button::MINIMUM_SPACE),
				      filter_(filter), weapons_(type == SHOW_ALL), left_(on_left_side)
{
#ifdef __IPHONEOS__
	#ifdef __IPAD__
		unsigned w = font::relative_size(weapons_ ? 200 : 190);
		unsigned h = font::relative_size(weapons_ ? 370 : 110);
	#else
		unsigned w = (weapons_ ? 100 : 155);	// weapons_ == true for the recruit/recall screen, otherwise it is the attack choice screen
		unsigned h = (weapons_ ? 280 : 90);		// the preview panes dictate how big the dialog box should be...
	#endif
#else	
	unsigned w = font::relative_size(weapons_ ? 200 : 190);
	unsigned h = font::relative_size(weapons_ ? 370 : 140);
#endif	
	set_measurements(w, h);
}


handler_vector unit_preview_pane::handler_members()
{
	handler_vector h;
	h.push_back(&details_button_);
	return h;
}

bool unit_preview_pane::show_above() const
{
	return !weapons_;
}

bool unit_preview_pane::left_side() const
{
	return left_;
}

void unit_preview_pane::set_selection(int index)
{
	index = std::min<int>(static_cast<int>(size())-1,index);
	if (filter_) {
		index = filter_->get_index(index);
	}
	if(index != index_) {
		index_ = index;
		set_dirty();
		if(map_ != NULL && index >= 0) {
			details_button_.set_dirty();
		}
	}
}

void unit_preview_pane::draw_contents()
{
	if(index_ < 0 || index_ >= int(size())) {
		return;
	}

	const details det = get_details();

	const bool right_align = left_side();

//	surface const screen = video().getSurface();

	SDL_Rect const &loc = location();
	
	/*const*/ SDL_Rect area = { loc.x + unit_preview_border, loc.y + unit_preview_border,
	                        loc.w - unit_preview_border * 2, loc.h - unit_preview_border * 2 };
	if (weapons_)
	{
		area.y -= 20;
		area.h += 20;
	}
	
	SDL_Rect clip_area = area;
	const clip_rect_setter clipper(/*screen,*/clip_area);

	surface unit_image = det.image;
//	if (!left_)
//		unit_image = image::reverse_image(unit_image);

	SDL_Rect image_rect = {area.x,area.y,0,0};

	if(unit_image != NULL) {
		SDL_Rect rect = {right_align ? area.x : area.x + area.w - unit_image->w,area.y,unit_image->w,unit_image->h};
		//SDL_BlitSurface(unit_image,NULL,screen,&rect);
		if (!left_)
			blit_surface(rect.x, rect.y, unit_image, NULL, NULL, FLOP);
		else
			blit_surface(rect.x, rect.y, unit_image);
		image_rect = rect;
	}

	// Place the 'unit profile' button
	if(map_ != NULL) {
		SDL_Rect button_loc = {right_align ? area.x : area.x + area.w - details_button_.location().w,
		                             image_rect.y + image_rect.h,
		                             details_button_.location().w,details_button_.location().h};
#ifdef __IPHONEOS__
		if (right_align)
			button_loc.x += 19;
		else
			button_loc.x -= 19;
		button_loc.y -= 5;
		
		// KP: no in-game access to help
		button_loc.h = 0;
		details_button_.hide();
#endif
		details_button_.set_location(button_loc);
	}

	SDL_Rect description_rect = {image_rect.x,image_rect.y+image_rect.h+details_button_.location().h,0,0};

	if(det.name.empty() == false) {
		std::stringstream desc;
		desc << font::NORMAL_TEXT << det.name;
		const std::string description = desc.str();
		description_rect = font::text_area(description, font::SIZE_NORMAL);
		description_rect = font::draw_text(&video(), area,
							font::SIZE_NORMAL, font::NORMAL_COLOUR,
							desc.str(), right_align ?  image_rect.x :
							image_rect.x + image_rect.w - description_rect.w,
							image_rect.y + image_rect.h + details_button_.location().h);
	}

	std::stringstream text;
	text << det.type_name << "\n"
#ifndef __IPHONEOS__	
		<< det.race << "\n"
#endif	
		<< font::BOLD_TEXT << _("level") << " " << det.level << "\n"
#ifndef __IPHONEOS__	
		<< det.alignment << "\n"
#endif	
		<< det.traits << "\n";

	for(std::vector<shared_string>::const_iterator a = det.abilities.begin(); a != det.abilities.end(); a++) {
		if(a != det.abilities.begin()) {
			text << ", ";
		}
		text << (*a);
	}
	text << "\n";

	// Use same coloring as in generate_report.cpp:
	text << det.hp_color << _("HP: ")
		<< det.hitpoints << "/" << det.max_hitpoints << "\n";

	text << det.xp_color << _("XP: ")
		<< det.experience << "/" << det.max_experience << "\n";

	if(weapons_) {
		text << _("Moves: ")
			<< det.movement_left << "/" << det.total_movement << "\n";

		for(std::vector<attack_type>::const_iterator at_it = det.attacks.begin();
		    at_it != det.attacks.end(); ++at_it) {
			// specials_context seems not needed here
			//at_it->set_specials_context(map_location(),u);

			// see generate_report() in generate_report.cpp
			text << "<245,230,193>" << at_it->name()
				<< " (" << gettext(at_it->type().c_str()) << ")\n";

			std::string special = at_it->weapon_specials(true);
			if (!special.empty()) {
				text << "<166,146,117>  " << special << "\n";
			}
			std::string accuracy = at_it->accuracy_parry_description();
			if(accuracy.empty() == false) {
				accuracy += " ";
			}

			text << "<166,146,117>  " << at_it->damage() << "-" << at_it->num_attacks()
				<< " " << accuracy << "-- " << _(at_it->range().c_str()) << "\n";
		}
	}

	// we don't remove empty lines, so all fields stay at the same place
	const std::vector<std::string> lines = utils::split(text.str(), '\n',
		utils::STRIP_SPACES & !utils::REMOVE_EMPTY);

	SDL_Rect cur_area = area;

	if(weapons_) {
		cur_area.y += image_rect.h + description_rect.h + details_button_.location().h;
	}

	for(std::vector<std::string>::const_iterator line = lines.begin(); line != lines.end(); ++line) {
		int xpos = cur_area.x;
		if(right_align && !weapons_) {
			const SDL_Rect& line_area = font::text_area(*line,font::SIZE_SMALL);
			xpos = cur_area.x + cur_area.w - line_area.w;
		}

		cur_area = font::draw_text(&video(),location(),font::SIZE_SMALL,font::NORMAL_COLOUR,*line,xpos,cur_area.y);
		cur_area.y += cur_area.h;
	}
}

units_list_preview_pane::units_list_preview_pane(game_display& disp,
		const gamemap* map, const unit& u, TYPE type, bool on_left_side) :
	unit_preview_pane(disp, map, NULL, type, on_left_side),
	units_(&unit_store_),
	unit_store_(1, u)
{
}

units_list_preview_pane::units_list_preview_pane(game_display& disp,
		const gamemap* map, std::vector<unit>& units,
		const gui::filter_textbox* filter, TYPE type, bool on_left_side) :
	unit_preview_pane(disp, map, filter, type, on_left_side),
	units_(&units),
	unit_store_()
{
}

size_t units_list_preview_pane::size() const
{
	return (units_!=NULL) ? units_->size() : 0;
}

const unit_preview_pane::details units_list_preview_pane::get_details() const
{
	unit& u = (*units_)[index_];
	details det;

	det.image = u.still_image();

	det.name = u.name();
	det.type_name = u.type_name();
	if(u.race() != NULL) {
		det.race = u.race()->name(u.gender());
	}
	det.level = u.level();
	det.alignment = unit_type::alignment_description(u.alignment(), u.gender());
	det.traits = u.traits_description();

	//we filter to remove the tooltips (increment by 2)
	const std::vector<std::string>& abilities = u.unit_ability_tooltips();
	for(std::vector<std::string>::const_iterator a = abilities.begin();
		 a != abilities.end(); a+=2) {
		det.abilities.push_back(*a);
	}

	det.hitpoints = u.hitpoints();
	det.max_hitpoints = u.max_hitpoints();
	det.hp_color = font::color2markup(u.hp_color());

	det.experience = u.experience();
	det.max_experience = u.max_experience();
	det.xp_color = font::color2markup(u.xp_color());

	det.movement_left = u.movement_left();
	det.total_movement= u.total_movement();

	det.attacks = u.attacks();
	return det;
}

void units_list_preview_pane::process_event()
{
	if(map_ != NULL && details_button_.pressed() && index_ >= 0 && index_ < int(size())) {
		show_unit_description(disp_, (*units_)[index_]);
	}
}

unit_types_preview_pane::unit_types_preview_pane(game_display& disp, const gamemap* map,
					std::vector<const unit_type*>& unit_types, const gui::filter_textbox* filter,
					int side, TYPE type, bool on_left_side)
					: unit_preview_pane(disp, map, filter, type, on_left_side),
					  unit_types_(&unit_types), side_(side)
{}

size_t unit_types_preview_pane::size() const
{
	return (unit_types_!=NULL) ? unit_types_->size() : 0;
}

const unit_types_preview_pane::details unit_types_preview_pane::get_details() const
{
	const unit_type* t = (*unit_types_)[index_];
	details det;

	if (t==NULL)
		return det;

    //FIXME: There should be a better way to deal with this
    unit_type_data::types().find_unit_type(t->id(), unit_type::WITHOUT_ANIMATIONS);

	std::string mod = "~RC(" + t->flag_rgb() + ">" + team::get_side_colour_index(side_) + ")";
	det.image = image::get_image(t->image()+mod);

	det.name = "";
	det.type_name = t->type_name();
	det.level = t->level();
	det.alignment = unit_type::alignment_description(t->alignment(), t->genders().front());

	race_map const& rcm = unit_type_data::types().races();
	race_map::const_iterator ri = rcm.find(t->race());
	if(ri != rcm.end()) {
		assert(t->genders().empty() != true);
		det.race = (*ri).second.name(t->genders().front());
	}

	//FIXME: This probably must be move into a unit_type function
	const std::vector<config*> traits = t->possible_traits();
	for(std::vector<config*>::const_iterator i = traits.begin(); i != traits.end(); i++) {
		if((**(i))["availability"] == "musthave") {
#ifdef __IPHONEOS__
			std::string gender_string = "male_name";
#else
			std::string gender_string = (!t->genders().empty() && t->genders().front()== unit_race::FEMALE) ? "female_name" : "male_name";
#endif
			t_string name = (**i)[gender_string];
			if (name.empty()) {
				name = (**i)["name"];
			}
			if (!name.empty()) {
				if (i != traits.begin()) {
					det.traits += ", ";
				}
				det.traits += name;
			}
		}
	}

	det.abilities = t->abilities();

	det.hitpoints = t->hitpoints();
	det.max_hitpoints = t->hitpoints();
	det.hp_color = "<33,225,0>"; // from unit::hp_color()

	det.experience = 0;
	det.max_experience = t->experience_needed();
	det.xp_color = "<0,160,225>"; // from unit::xp_color()

	// Check if AMLA color is needed
	// FIXME: not sure if it's fully accurate (but not very important for unit_type)
	// xp_color also need a simpler function for doing this
	const config::child_list& advances = t->modification_advancements();
	for(config::child_list::const_iterator j = advances.begin(); j != advances.end(); ++j) {
		if (!utils::string_bool((**j)["strict_amla"]) || !t->can_advance()) {
			det.xp_color = "<100,0,150>"; // from unit::xp_color()
			break;
		}
	}

	det.movement_left = 0;
	det.total_movement= t->movement();

	det.attacks = t->attacks();
	return det;
}

void unit_types_preview_pane::process_event()
{
	if(map_ != NULL && details_button_.pressed() && index_ >= 0 && index_ < int(size())) {
		const unit_type* type = (*unit_types_)[index_];
		if (type != NULL)
			show_unit_description(disp_, *type);
	}
}

	
void show_unit_description(game_display &disp, const unit& u)
{
	const unit_type* t = u.type();
/*
	if (t != NULL)
		show_unit_description(disp, *t);
	else
		// can't find type, try open the id page to have feedback and unit error page
	  help::show_unit_help(disp, u.type_id());
*/	
	
	if (t != NULL)
	{
		gUnitType = t;
		unitDetailsController = [[UnitDetails alloc] initWithNibName:@"UnitDetails" bundle:nil];
		
		CGRect frameRect = unitDetailsController.view.frame;
#ifdef __IPAD__
		frameRect.origin.x = (1024-480)/2;
		frameRect.origin.y = (768-320)/2;
#else
		frameRect.origin.x = 0;
		frameRect.origin.y = 0;
#endif
		unitDetailsController.view.frame = frameRect;
		[gLandscapeView addSubview:unitDetailsController.view];
		gLandscapeView.hidden = NO;
//		gPauseForOpenFeint = true;
		gRedraw = true;
	}
	
}

void show_unit_description(game_display &disp, const unit_type& t)
{
	help::show_unit_help(disp, t.id(), t.hide_help());
}


namespace {
	static const int campaign_preview_border = font::relative_size(10);
}

campaign_preview_pane::campaign_preview_pane(CVideo &video,std::vector<std::pair<shared_string,shared_string> >* desc) : gui::preview_pane(video),descriptions_(desc),index_(0)
{
// size of the campaign info window with the campaign description and image in pixel
#if defined(USE_TINY_GUI)
  #ifdef FREE_VERSION	
	set_measurements(230, 157);
  #else
	set_measurements(180, 234);
  #endif
#else
	set_measurements(430, 440);
#endif
	index_ = -1;
	set_selection(0);	
}

bool campaign_preview_pane::show_above() const { return false; }
bool campaign_preview_pane::left_side() const { return false; }

void campaign_preview_pane::set_selection(int index)
{
	index = std::min<int>(descriptions_->size()-1,index);
	if(index != index_ && index >= 0) {
		index_ = index;
		const SDL_Rect area = {
			location().x+campaign_preview_border,
			location().y,
			location().w-campaign_preview_border*2,
			location().h };
		//desc_text = font::word_wrap_text((*descriptions_)[index_].first, font::SIZE_SMALL, area.w - 2 * campaign_preview_border);
		desc_text = font::word_wrap_text((*descriptions_)[index_].first, font::SIZE_PLUS, area.w - 2 * campaign_preview_border);
		set_dirty();
	}
}

void campaign_preview_pane::draw_contents()
{
	if (size_t(index_) >= descriptions_->size()) {
		return;
	}

	const SDL_Rect area = {
		location().x+campaign_preview_border,
		location().y,
		location().w-campaign_preview_border*2,
		location().h };

	/* background frame */
	gui::dialog_frame f(video(), "", gui::dialog_frame::preview_style, false);
	f.layout(area);
	f.draw_background();
	f.draw_border();

	/* description text */
/*	
	std::string desc_text;
	try {
		desc_text = font::word_wrap_text((*descriptions_)[index_].first,
			font::SIZE_SMALL, area.w - 2 * campaign_preview_border);
	} catch (utils::invalid_utf8_exception&) {
		LOG_STREAM(err, engine) << "Invalid utf-8 found, campaign description is ignored.\n";
	}
*/ 
//	const std::vector<std::string> lines = utils::split(desc_text, '\n',utils::STRIP_SPACES);
	SDL_Rect txt_area = { area.x+campaign_preview_border,area.y+campaign_preview_border,0,0 };

//	for(std::vector<std::string>::const_iterator line = lines.begin(); line != lines.end(); ++line) {
//	  txt_area = font::draw_text(&video(),location(),font::SIZE_SMALL,font::NORMAL_COLOUR,*line,txt_area.x,txt_area.y);
//		txt_area.y += txt_area.h;
//	}
	font::draw_text(&video(),location(),font::SIZE_PLUS,font::NORMAL_COLOUR,desc_text,txt_area.x,txt_area.y);

	/* description image */
/*	
	surface img(NULL);
	const std::string desc_img_name = (*descriptions_)[index_].second;
	if(!desc_img_name.empty()) {
		img.assign(image::get_image(desc_img_name));
	}
	if (!img.null()) {
		SDL_Rect src_rect,dst_rect;

		int max_height = area.h-(txt_area.h+txt_area.y-area.y);

		// scale the image to fit the area, and preserve aspect ratio
		SDL_Rect scaledSize;
		scaledSize.w = img->w;
		scaledSize.h = img->h;
		if (scaledSize.w > area.w)
		{
			float ratio;
			ratio = (float)area.w / (float)scaledSize.w;
			
			scaledSize.w = ((float)scaledSize.w * ratio);
			scaledSize.h = ((float)scaledSize.h * ratio);
		}
		if (scaledSize.h > max_height)
		{
			float ratio;
			ratio = (float)max_height / (float)scaledSize.h;
			
			scaledSize.w = ((float)scaledSize.w * ratio);
			scaledSize.h = ((float)scaledSize.h * ratio);			
		}
		if (scaledSize.w != img->w || scaledSize.h != img->h)
		{
			img.assign(scale_surface(img,scaledSize.w,scaledSize.h));
		}
		
		
		
		src_rect.x = src_rect.y = 0;
		src_rect.w = std::min<int>(area.w,img->w);
		src_rect.h = std::min<int>(max_height,img->h);
		
		
		dst_rect.x = area.x+(area.w-src_rect.w)/2;
		dst_rect.y = txt_area.y+((max_height-src_rect.h)*8)/13;
		if(dst_rect.y - txt_area.h - txt_area.y >= 120) {
			//for really tall dialogs, just put it under the text
			dst_rect.y = txt_area.y + font::get_max_height(font::SIZE_SMALL)*5;
		}
		


		SDL_BlitSurface(img,&src_rect,video().getSurface(),&dst_rect);

	}
*/ 
}

static network::connection network_data_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num, network::statistics (*get_stats)(network::connection handle))
{
#ifdef USE_TINY_GUI
	const size_t width = 200;
	const size_t height = 40;
	const size_t border = 10;
#else
	const size_t width = 300;
	const size_t height = 80;
	const size_t border = 20;
#endif
	const int left = disp.w()/2 - width/2;
	const int top  = disp.h()/2 - height/2;

	const events::event_context dialog_events_context;

	gui::button cancel_button(disp.video(),_("Cancel"));
	std::vector<gui::button*> buttons_ptr(1,&cancel_button);

	gui::dialog_frame frame(disp.video(), msg, gui::dialog_frame::default_style, true, &buttons_ptr);
	SDL_Rect centered_layout = frame.layout(left,top,width,height).interior;
	centered_layout.x = disp.w() / 2 - centered_layout.w / 2;
	centered_layout.y = disp.h() / 2 - centered_layout.h / 2;
	// HACK: otherwise we get an empty useless space in the dialog below the progressbar
	centered_layout.h = height;
	frame.layout(centered_layout);
	frame.draw();

	const SDL_Rect progress_rect = {centered_layout.x+border,centered_layout.y+border,centered_layout.w-border*2,centered_layout.h-border*2};
	gui::progress_bar progress(disp.video());
	progress.set_location(progress_rect);

	events::raise_draw_event();
	disp.flip();

	network::statistics old_stats = get_stats(connection_num);

	cfg.clear();
	for(;;) {
		const network::connection res = network::receive_data(cfg,connection_num,100);
		const network::statistics stats = get_stats(connection_num);
		if(stats.current_max != 0 && stats != old_stats) {
			old_stats = stats;
			progress.set_progress_percent((stats.current*100)/stats.current_max);
			std::ostringstream stream;
			stream << stats.current/1024 << "/" << stats.current_max/1024 << _("KB");
			progress.set_text(stream.str());
		}

		events::raise_draw_event();
		disp.flip();
		events::pump();

		if(res != 0) {
			return res;
		}


		if(cancel_button.pressed()) {
			return res;
		}
	}
}

network::connection network_send_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num)
{
	return network_data_dialog(disp, msg, cfg, connection_num,
							   network::get_send_stats);
}

network::connection network_receive_dialog(display& disp, const std::string& msg, config& cfg, network::connection connection_num)
{
	return network_data_dialog(disp, msg, cfg, connection_num,
							   network::get_receive_stats);
}

} // end namespace dialogs

namespace {

class connect_waiter : public threading::waiter
{
public:
	connect_waiter(display& disp, gui::button& button) : disp_(disp), button_(button)
	{}
	ACTION process();

private:
	display& disp_;
	gui::button& button_;
};

connect_waiter::ACTION connect_waiter::process()
{
	events::raise_draw_event();
	disp_.flip();
	events::pump();
	if(button_.pressed()) {
		return ABORT;
	} else {
		return WAIT;
	}
}

}

namespace dialogs
{

network::connection network_connect_dialog(display& disp, const std::string& msg, const std::string& hostname, int port)
{
#ifdef USE_TINY_GUI
	const size_t width = 200;
	const size_t height = 20;
#else
	const size_t width = 250;
	const size_t height = 20;
#endif
	const int left = disp.w()/2 - width/2;
	const int top  = disp.h()/2 - height/2;

	const events::event_context dialog_events_context;

	gui::button cancel_button(disp.video(),_("Cancel"));
	std::vector<gui::button*> buttons_ptr(1,&cancel_button);

	gui::dialog_frame frame(disp.video(), msg, gui::dialog_frame::default_style, true, &buttons_ptr);
	frame.layout(left,top,width,height);
	frame.draw();

	events::raise_draw_event();
	disp.flip();

	connect_waiter waiter(disp,cancel_button);
	return network::connect(hostname,port,waiter);
}

} // end namespace dialogs
