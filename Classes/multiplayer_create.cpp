/* $Id: multiplayer_create.cpp 33835 2009-03-18 16:02:24Z shadowmaster $ */
/*
   Copyright (C) 2007 - 2009
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file multiplayer_create.cpp
 * Create a multiplayer-game: select map, players, options etc.
 */

#include "global.hpp"

#include "gettext.hpp"
#include "construct_dialog.hpp"
#include "settings.hpp"
#include "map.hpp"
#include "map_exception.hpp"
#include "map_create.hpp"
#include "gui/dialogs/message.hpp"
#include "minimap.hpp"
#include "multiplayer_create.hpp"
#include "filesystem.hpp"
#include "log.hpp"
#include "wml_exception.hpp"
#include "wml_separators.hpp"
#include "formula_string_utils.hpp"


#define ERR_CF LOG_STREAM(err, config)

namespace {
const SDL_Rect null_rect = {0, 0, 0, 0};
}

namespace mp {

create::create(game_display& disp, const config &cfg, chat& c, config& gamelist) :
	ui(disp, _("Create Game"), cfg, c, gamelist),

	tooltip_manager_(disp.video()),
	map_selection_(-1),
	mp_countdown_init_time_(270),
	mp_countdown_reservoir_time_(330),
	user_maps_(),
	map_options_(),
	map_index_(),

	maps_menu_(disp.video(), std::vector<shared_string>()),
	turns_slider_(disp.video()),
	turns_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	countdown_game_(disp.video(), _("Time limit"), gui::button::TYPE_CHECK),
	countdown_init_time_slider_(disp.video()),
	countdown_init_time_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	countdown_reservoir_time_slider_(disp.video()),
	countdown_reservoir_time_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	countdown_turn_bonus_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	countdown_turn_bonus_slider_(disp.video()),
	countdown_action_bonus_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	countdown_action_bonus_slider_(disp.video()),
	village_gold_slider_(disp.video()),
	village_gold_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	xp_modifier_slider_(disp.video()),
	xp_modifier_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	name_entry_label_(disp.video(), _("Name of game:"), font::SIZE_PLUS, font::LOBBY_COLOUR),
	num_players_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	map_size_label_(disp.video(), "", font::SIZE_SMALL, font::LOBBY_COLOUR),
	era_label_(disp.video(), _("Era:"), font::SIZE_SMALL, font::LOBBY_COLOUR),
	map_label_(disp.video(), _("Map to play:"), font::SIZE_SMALL, font::LOBBY_COLOUR),
	use_map_settings_(disp.video(), _("Use map settings"), gui::button::TYPE_CHECK),
	random_start_time_(disp.video(), _("Random start time"), gui::button::TYPE_CHECK),
	fog_game_(disp.video(), _("Fog Of War"), gui::button::TYPE_CHECK),
	shroud_game_(disp.video(), _("Shroud"), gui::button::TYPE_CHECK),
	observers_game_(disp.video(), _("Observers"), gui::button::TYPE_CHECK),
	cancel_game_(disp.video(), _("Cancel"), gui::button::TYPE_PRESS, "button"),
	launch_game_(disp.video(), _("OK"), gui::button::TYPE_PRESS, "button"),
	regenerate_map_(disp.video(), _("Regenerate")),
	generator_settings_(disp.video(), _("Settings...")),
	password_button_(disp.video(), _("Set Password...")),
	era_combo_(disp, std::vector<shared_string>()),
	vision_combo_(disp, std::vector<shared_string>()),
	name_entry_(disp.video(), 32),
//	minimap_restorer_(NULL),
	minimap_rect_(null_rect),
	generator_(NULL),
	parameters_()
{
	// Build the list of scenarios to play

	// Add the 'load game' option
	std::string markup_txt = "`~";
	std::string help_sep = " ";
	help_sep[0] = HELP_STRING_SEPARATOR;
	std::string menu_help_str = help_sep + _("Load Game");
	map_options_.push_back(markup_txt + _("Load Game...") + menu_help_str);

	// User maps
	get_files_in_dir(get_user_data_dir() + "/editor/maps",&user_maps_,NULL,FILE_NAME_ONLY);

	size_t i = 0;
	for(i = 0; i < user_maps_.size(); i++)
	{
		menu_help_str = help_sep + user_maps_[i];
		map_options_.push_back(user_maps_[i] + menu_help_str);
	}

	// Standard maps
	const config::child_list& levels = cfg.get_children("multiplayer");
	i = 0;
	for(config::child_list::const_iterator j = levels.begin(); j != levels.end(); ++j, ++i)
	{
		if ( ((*j)->values.find("allow_new_game") == (*j)->values.end())
			|| utils::string_bool((**j)["allow_new_game"],true))
		{
			menu_help_str = help_sep + ((**j)["name"]);
			map_options_.push_back(((**j)["name"]) + menu_help_str);
			map_index_.push_back(i);
		}
	}

	// Create the scenarios menu
	maps_menu_.set_items(map_options_);
	if (size_t(preferences::map()) < map_options_.size())
		maps_menu_.move_selection(preferences::map());
	maps_menu_.set_numeric_keypress_selection(false);

	turns_slider_.set_min(settings::turns_min);
	turns_slider_.set_max(settings::turns_max);
	turns_slider_.set_increment(settings::turns_step);
	turns_slider_.set_value(preferences::turns());
	turns_slider_.set_help_string(_("The maximum number of turns the game can last"));

	countdown_game_.set_check(preferences::countdown());
	countdown_game_.set_help_string(_("Enables user time limit"));

	countdown_init_time_slider_.set_min(30);
	countdown_init_time_slider_.set_max(1500);
	countdown_init_time_slider_.set_increment(30);
	countdown_init_time_slider_.set_value(preferences::countdown_init_time());
	countdown_init_time_slider_.set_help_string(_("Longest time allowed for first turn (seconds)"));

	countdown_reservoir_time_slider_.set_min(30);
	countdown_reservoir_time_slider_.set_max(1500);
	countdown_reservoir_time_slider_.set_increment(30);
	countdown_reservoir_time_slider_.set_value(preferences::countdown_reservoir_time());
	countdown_reservoir_time_slider_.set_help_string(_("Longest time possible for any turn (seconds)"));

	countdown_turn_bonus_slider_.set_min(10);
	countdown_turn_bonus_slider_.set_max(300);
	countdown_turn_bonus_slider_.set_increment(5);
	countdown_turn_bonus_slider_.set_value(preferences::countdown_turn_bonus());
	countdown_turn_bonus_slider_.set_help_string(_("Time for general tasks each turn (seconds)"));

	countdown_action_bonus_slider_.set_min(0);
	countdown_action_bonus_slider_.set_max(30);
	countdown_action_bonus_slider_.set_increment(1);
	countdown_action_bonus_slider_.set_value(preferences::countdown_action_bonus());
	countdown_action_bonus_slider_.set_help_string(_("Time for each attack, recruit, and capture"));

	village_gold_slider_.set_min(1);
	village_gold_slider_.set_max(5);
	village_gold_slider_.set_value(preferences::village_gold());
	village_gold_slider_.set_help_string(_("The amount of income each village yields per turn"));

	xp_modifier_slider_.set_min(30);
	xp_modifier_slider_.set_max(200);
	xp_modifier_slider_.set_value(preferences::xp_modifier());
	xp_modifier_slider_.set_increment(10);
	xp_modifier_slider_.set_help_string(_("The amount of experience a unit needs to advance"));

	use_map_settings_.set_check(preferences::use_map_settings());
	use_map_settings_.set_help_string(_("Use scenario specific settings"));

	random_start_time_.set_check(preferences::random_start_time());
	random_start_time_.set_help_string(_("Randomize time of day in begin"));

	fog_game_.set_check(preferences::fog());
	fog_game_.set_help_string(_("Enemy units cannot be seen unless they are in range of your units"));

	shroud_game_.set_check(preferences::shroud());
	shroud_game_.set_help_string(_("The map is unknown until your units explore it"));

	observers_game_.set_check(preferences::allow_observers());
	observers_game_.set_help_string(_("Allow users who are not playing to watch the game"));

	// The possible vision settings
	std::vector<shared_string> vision_types;
	vision_types.push_back(_("Share View"));
	vision_types.push_back(_("Share Maps"));
	vision_types.push_back(_("Share None"));
	vision_combo_.set_items(vision_types);
	vision_combo_.set_selected(0);

	// The possible eras to play

	const config::child_list& era_list = cfg.get_children("era");
	std::vector<shared_string> eras;
	for(config::child_list::const_iterator er = era_list.begin(); er != era_list.end(); ++er) {
		eras.push_back((**er)["name"]);
	}
	if(eras.empty()) {
		gui::message_dialog(disp, "", _("No eras found.")).show();
		throw config::error(_("No eras found"));
	}
	era_combo_.set_items(eras);

	if (size_t(preferences::era()) < eras.size()) {
		era_combo_.set_selected(preferences::era());
	} else {
		era_combo_.set_selected(0);
	}

	era_combo_.hide();

	utils::string_map i18n_symbols;
	i18n_symbols["login"] = preferences::login();
	name_entry_.set_text(vgettext("$login's game", i18n_symbols));

	gamelist_updated();
}

create::~create()
{
	// Only save the settings if the dialog was 'accepted'
	if(get_result() != CREATE) {
		return;
	}

	// Retrieve values
	get_parameters();

	// Save values for next game
	preferences::set_allow_observers(parameters_.allow_observers);
	preferences::set_use_map_settings(parameters_.use_map_settings);
	preferences::set_countdown(parameters_.mp_countdown);
	preferences::set_countdown_init_time(parameters_.mp_countdown_init_time);
	preferences::set_countdown_turn_bonus(parameters_.mp_countdown_turn_bonus);
	preferences::set_countdown_reservoir_time(parameters_.mp_countdown_reservoir_time);
	preferences::set_countdown_action_bonus(parameters_.mp_countdown_action_bonus);
	preferences::set_era(era_combo_.selected()); /** @todo FIXME: may be broken if new eras are added. */
	preferences::set_map(map_selection_);

	// When using map settings, the following variables are determined by the map,
	// so don't store them as the new preferences.
	if(!parameters_.use_map_settings) {
		preferences::set_fog(parameters_.fog_game);
		preferences::set_shroud(parameters_.shroud_game);
		preferences::set_turns(parameters_.num_turns);
		preferences::set_random_start_time(parameters_.random_start_time);
		preferences::set_village_gold(parameters_.village_gold);
		preferences::set_xp_modifier(parameters_.xp_modifier);
	}
}

create::parameters& create::get_parameters()
{
	const config::child_list& era_list = game_config().get_children("era");
	const int turns = turns_slider_.value() < turns_slider_.max_value() ?
		turns_slider_.value() : -1;

	const int mp_countdown_turn_bonus_val = countdown_turn_bonus_slider_.value() <= countdown_turn_bonus_slider_.max_value() ?
		countdown_turn_bonus_slider_.value() : -1;
	const int mp_countdown_action_bonus_val = countdown_action_bonus_slider_.value() <= countdown_action_bonus_slider_.max_value() ?
		countdown_action_bonus_slider_.value() : -1;
	const int mp_countdown_reservoir_time_val = countdown_reservoir_time_slider_.value() <= countdown_reservoir_time_slider_.max_value() ?
		countdown_reservoir_time_slider_.value() : -1;
	int mp_countdown_init_time_val = countdown_init_time_slider_.value() <= countdown_init_time_slider_.max_value() ?
		countdown_init_time_slider_.value() : -1;
	if(mp_countdown_reservoir_time_val > 0 && mp_countdown_init_time_val > mp_countdown_reservoir_time_val)
		mp_countdown_init_time_val = mp_countdown_reservoir_time_val;

	// Updates the values in the "parameters_" member to match
	// the values selected by the user with the widgets:
	parameters_.name = name_entry_.text();
	if (size_t(era_combo_.selected()) >= era_list.size()) {
		throw config::error(_("Invalid era selected"));
	}
	parameters_.era = (*era_list[era_combo_.selected()])["id"];
	parameters_.num_turns = turns;
	// CHECK
	parameters_.mp_countdown_init_time = mp_countdown_init_time_val;
	parameters_.mp_countdown_turn_bonus = mp_countdown_turn_bonus_val;
	parameters_.mp_countdown_reservoir_time = mp_countdown_reservoir_time_val;
	parameters_.mp_countdown_action_bonus = mp_countdown_action_bonus_val;
	parameters_.mp_countdown = countdown_game_.checked();
	parameters_.village_gold = village_gold_slider_.value();
	parameters_.xp_modifier = xp_modifier_slider_.value();
	parameters_.use_map_settings = use_map_settings_.checked();
	parameters_.random_start_time = random_start_time_.checked();
	parameters_.fog_game = fog_game_.checked();
	parameters_.shroud_game = shroud_game_.checked();
	parameters_.allow_observers = observers_game_.checked();
	parameters_.share_view = vision_combo_.selected() == 0;
	parameters_.share_maps = vision_combo_.selected() == 1;

	return parameters_;
}

void create::process_event()
{
	int mousex, mousey;
	SDL_GetMouseState(0, &mousex,&mousey);
	tooltips::process(mousex, mousey);

	if(cancel_game_.pressed()) {
		set_result(QUIT);
		return;
	}

	if(launch_game_.pressed() || maps_menu_.double_clicked()) {
		if(name_entry_.text() != "") {
			set_result(CREATE);
			return;
		} else {
			gui::message_dialog(disp_, "", _("You must enter a name.")).show();
		}
	}

	if(password_button_.pressed()) {
		gui::show_dialog(disp_, NULL, _("Set Password"),
		_("Set the password that people wanting to join your game as players must enter."), gui::OK_ONLY, NULL, NULL, _("Password: "), &parameters_.password);
	}

	// Turns per game
	const int cur_turns = turns_slider_.value();

	std::stringstream buf;
	if(cur_turns < 100) {
		buf << _("Turns: ") << cur_turns;
	} else {
		buf << _("Unlimited Turns");
	}
	turns_label_.set_text(buf.str());

	countdown_init_time_label_.enable(countdown_game_.checked());
	countdown_init_time_slider_.enable(countdown_game_.checked());
	countdown_turn_bonus_label_.enable(countdown_game_.checked());
	countdown_turn_bonus_slider_.enable(countdown_game_.checked());

	countdown_reservoir_time_label_.enable(countdown_game_.checked());
	countdown_reservoir_time_slider_.enable(countdown_game_.checked());
	countdown_action_bonus_label_.enable(countdown_game_.checked());
	countdown_action_bonus_slider_.enable(countdown_game_.checked());

	if(mp_countdown_init_time_ != countdown_init_time_slider_.value()
		&& countdown_init_time_slider_.value() > countdown_reservoir_time_slider_.value())
	{
		countdown_reservoir_time_slider_.set_value(countdown_init_time_slider_.value());
	}
	if(mp_countdown_reservoir_time_ != countdown_reservoir_time_slider_.value()
		&& countdown_reservoir_time_slider_.value() < countdown_init_time_slider_.value())
	{
		countdown_init_time_slider_.set_value(countdown_reservoir_time_slider_.value());
	}
	mp_countdown_init_time_ = countdown_init_time_slider_.value();
	mp_countdown_reservoir_time_ = countdown_reservoir_time_slider_.value();

	buf.str("");
	buf <<  _("Init. Limit: ") << mp_countdown_init_time_; // << _(" sec.");
	countdown_init_time_label_.set_text(buf.str());

	const int mp_countdown_turn_bonus_val = countdown_turn_bonus_slider_.value();
	buf.str("");
	buf <<  _("Turn Bonus: ") << mp_countdown_turn_bonus_val; // << _(" sec.");
	countdown_turn_bonus_label_.set_text(buf.str());

	buf.str("");
	buf <<  _("Reservoir: ") << mp_countdown_reservoir_time_; // << _(" sec.");
	countdown_reservoir_time_label_.set_text(buf.str());

	const int mp_countdown_action_bonus_val = countdown_action_bonus_slider_.value();
	buf.str("");
	buf <<  _("Action Bonus: ") << mp_countdown_action_bonus_val; // << _(" sec.");
	countdown_action_bonus_label_.set_text(buf.str());


	// Villages can produce between 1 and 5 gold a turn
	const int village_gold = village_gold_slider_.value();
	buf.str("");
	buf << _("Village Gold: ") << village_gold;
	village_gold_label_.set_text(buf.str());

	// Experience modifier
	const int xpmod = xp_modifier_slider_.value();
	buf.str("");

#ifdef USE_TINY_GUI
	buf << N_("Exp Mod: ") << xpmod << "%";
#else
	buf << _("Experience Modifier: ") << xpmod << "%";
#endif

	xp_modifier_label_.set_text(buf.str());

	bool map_changed = map_selection_ != maps_menu_.selection();
	map_selection_ = maps_menu_.selection();

	if(map_changed) {
		generator_.assign(NULL);

		tooltips::clear_tooltips(minimap_rect_);

		const size_t select = size_t(maps_menu_.selection());

		if(select > 0 && select <= user_maps_.size()) {
			parameters_.saved_game = false;
			const config* const generic_multiplayer = game_config().child("generic_multiplayer");
			if(generic_multiplayer != NULL) {
				parameters_.scenario_data = *generic_multiplayer;
				parameters_.scenario_data["map_data"] = read_map(user_maps_[select-1]);
			}

		} else if(select > user_maps_.size() && select <= maps_menu_.nitems()-1) {
			parameters_.saved_game = false;
			size_t index = select - user_maps_.size() - 1;
			assert(index < map_index_.size());
			index = map_index_[index];
			const config::child_list& levels = game_config().get_children("multiplayer");

			if(index < levels.size()) {

				parameters_.scenario_data = *levels[index];
				std::string map_data = parameters_.scenario_data["map_data"];

				if(map_data.empty() && parameters_.scenario_data.get_attribute("map") != "") {
					map_data = read_map(parameters_.scenario_data["map"]);
				}

				// If the map should be randomly generated.
				if(parameters_.scenario_data.get_attribute("map_generation") != "") {
					generator_.assign(create_map_generator(parameters_.scenario_data["map_generation"],parameters_.scenario_data.child("generator")));
				}

#ifndef USE_TINY_GUI
				if(!parameters_.scenario_data.get_attribute("description").empty()) {
					tooltips::add_tooltip(minimap_rect_,parameters_.scenario_data["description"]);
				}
#endif
			}
		} else {
			parameters_.scenario_data.clear();
			parameters_.saved_game = true;

//			if (minimap_restorer_ != NULL)
//				minimap_restorer_->restore();
		}
	}

	if(generator_ != NULL && generator_->allow_user_config() && generator_settings_.pressed()) {
		generator_->user_config(disp_);
		map_changed = true;
	}

	if(generator_ != NULL && (map_changed || regenerate_map_.pressed())) {
		const cursor::setter cursor_setter(cursor::WAIT);

		// Generate the random map
		cursor::setter cur(cursor::WAIT);
		parameters_.scenario_data = generator_->create_scenario(std::vector<std::string>());
		map_changed = true;

		if (!parameters_.scenario_data["error_message"].empty())
			gui2::show_message(disp().video(), "map generation error", parameters_.scenario_data["error_message"]);

		// Set the scenario to have placing of sides
		// based on the terrain they prefer
		parameters_.scenario_data["modify_placing"] = "true";
	}

	if(map_changed) {
		generator_settings_.hide(generator_ == NULL);
		regenerate_map_.hide(generator_ == NULL);

		const std::string& map_data = parameters_.scenario_data["map_data"];

		std::auto_ptr<gamemap> map(NULL);
		try {
			map = std::auto_ptr<gamemap>(new gamemap(game_config(), map_data));
		} catch(incorrect_map_format_exception& e) {
			ERR_CF << "map could not be loaded: " << e.msg_ << "\n";

#ifndef USE_TINY_GUI
			tooltips::clear_tooltips(minimap_rect_);
			tooltips::add_tooltip(minimap_rect_,e.msg_);
#endif
		} catch(twml_exception& e) {
			ERR_CF <<  "map could not be loaded: " << e.dev_message << '\n';
		}

		launch_game_.enable(map.get() != NULL);

		// If there are less sides in the configuration than there are
		// starting positions, then generate the additional sides
		const int map_positions = map.get() != NULL ? map->num_valid_starting_positions() : 0;

		for(int pos =  parameters_.scenario_data.get_children("side").size(); pos < map_positions; ++pos) {
			config& side = parameters_.scenario_data.add_child("side");
			side["enemy"] = "1";
			side["side"] = lexical_cast<std::string>(pos+1);
			side["team_name"] = lexical_cast<std::string>(pos+1);
			side["canrecruit"] = "yes";
			side["controller"] = "human";
		}

#ifndef USE_TINY_GUI
		if(map.get() != NULL) {
			const surface mini(image::getMinimap(minimap_rect_.w,minimap_rect_.h,*map,0));
			SDL_Color back_color = {0,0,0,255};
			draw_centered_on_background(mini, minimap_rect_, back_color/*, video().getSurface()*/);
		}
#endif

		int nsides = parameters_.scenario_data.get_children("side").size();
		const config::child_list& player_sides = parameters_.scenario_data.get_children("side");
		for(config::child_list::const_iterator k = player_sides.begin(); k != player_sides.end(); ++k) {
			if(!utils::string_bool((**k)["allow_player"],true)) {
				nsides--;
			}
		}

		std::stringstream players;
		std::stringstream map_size;
		if(map.get() != NULL) {
			players << _("Players: ") << nsides;
			map_size << _("Size: ") << map.get()->w() << "x" << map.get()->h();
		} else {
			players << _("Error");
			map_size << "";
		}
		num_players_label_.set_text(players.str());
		map_size_label_.set_text(map_size.str());
	}

	if(map_changed || use_map_settings_.pressed()) {
		const bool map_settings = use_map_settings_.checked();

		// If the map settings are wanted use them,
		// if not properly defined fall back to the default settings
		turns_slider_.set_value(map_settings ?
			settings::get_turns(parameters_.scenario_data["turns"]) :
			preferences::turns());

		xp_modifier_slider_.set_value(map_settings ?
			settings::get_xp_modifier(parameters_.scenario_data["experience_modifier"]) :
			preferences::xp_modifier());

		random_start_time_.set_check(map_settings ?
			settings::use_random_start_time(parameters_.scenario_data["random_start_time"]) :
			preferences::random_start_time());

		// These are per player, always show values of player 1.
		/**
		 * @todo This might not be 100% correct, but at the moment
		 * it is not possible to show the fog and shroud per player.
		 * This might change in the future.
		 * NOTE when 'load game' is selected there are no sides.
		 */
		if(parameters_.scenario_data.get_children("side").size()) {

			village_gold_slider_.set_value(map_settings ?
				settings::get_village_gold((*parameters_.
					scenario_data.get_children("side").front())["village_gold"]) :
				preferences::village_gold());

			fog_game_.set_check(map_settings ?
				settings::use_fog((*parameters_.
					scenario_data.get_children("side").front())["fog"]) :
				preferences::fog());

			shroud_game_.set_check(map_settings ?
				settings::use_shroud((*parameters_.
					scenario_data.get_children("side").front())["shroud"]) :
				preferences::shroud());
		}

		// Set the widget states
		turns_slider_.enable(!map_settings);
		village_gold_slider_.enable(!map_settings);
		xp_modifier_slider_.enable(!map_settings);
		random_start_time_.enable(!map_settings);
		fog_game_.enable(!map_settings);
		shroud_game_.enable(!map_settings);
	}
}

void create::hide_children(bool hide)
{
	ui::hide_children(hide);

	maps_menu_.hide(hide);
	turns_slider_.hide(hide);
	turns_label_.hide(hide);

	countdown_init_time_slider_.hide(hide);
	countdown_init_time_label_.hide(hide);
	countdown_turn_bonus_slider_.hide(hide);
	countdown_turn_bonus_label_.hide(hide);
	countdown_reservoir_time_slider_.hide(hide);
	countdown_reservoir_time_label_.hide(hide);
	countdown_action_bonus_slider_.hide(hide);
	countdown_action_bonus_label_.hide(hide);
	countdown_game_.hide(hide);

	village_gold_slider_.hide(hide);
	village_gold_label_.hide(hide);
	xp_modifier_slider_.hide(hide);
	xp_modifier_label_.hide(hide);

	name_entry_label_.hide(hide);
	num_players_label_.hide(hide);
	map_size_label_.hide(hide);
	era_label_.hide(hide);
	map_label_.hide(hide);

	use_map_settings_.hide(hide);
	random_start_time_.hide(hide);
	fog_game_.hide(hide);
	shroud_game_.hide(hide);
	observers_game_.hide(hide);
	cancel_game_.hide(hide);
	launch_game_.hide(hide);
	regenerate_map_.hide(hide || generator_ == NULL);
	generator_settings_.hide(hide || generator_ == NULL);

	era_combo_.hide(hide);
	vision_combo_.hide(hide);
	name_entry_.hide(hide);

	if (hide) {
//		minimap_restorer_.assign(NULL);
	} else {
//		minimap_restorer_.assign(new surface_restorer(&video(), minimap_rect_));

		const std::string& map_data = parameters_.scenario_data["map_data"];

		try {
//			gamemap map(game_config(), map_data);

#ifndef USE_TINY_GUI
			gamemap map(game_config(), map_data);
			
			const surface mini(image::getMinimap(minimap_rect_.w,minimap_rect_.h,map,0));
			SDL_Color back_color = {0,0,0,255};
			draw_centered_on_background(mini, minimap_rect_, back_color/*, video().getSurface()*/);
#endif
		} catch(incorrect_map_format_exception& e) {
			ERR_CF << "map could not be loaded: " << e.msg_ << "\n";
		} catch(twml_exception& e) {
			ERR_CF <<  "map could not be loaded: " << e.dev_message << '\n';
		}

	}
}

#ifndef USE_TINY_GUI
void create::layout_children(const SDL_Rect& rect)
{
	ui::layout_children(rect);
	SDL_Rect ca = client_area();

	const int border_size = 6;
	const int column_border_size = 10;

	int xpos = ca.x;
	int ypos = ca.y;

	// Dialog title
	ypos += title().height() + border_size;

	// Name Entry
	name_entry_label_.set_location(xpos, ypos);
	name_entry_.set_location(xpos + name_entry_label_.width() + border_size, ypos);
	name_entry_.set_width(ca.w - name_entry_label_.width() - border_size);
	ypos += std::max<int>(name_entry_.height(), name_entry_label_.height()) + border_size;

	// Save ypos here (column top)
	int ypos_columntop = ypos;

	// First column: minimap & random map options
	std::pair<int,int> resolution = preferences::resolution();

	const int resolution_for_small_minimap = 880;

	const int minimap_width = resolution.first > resolution_for_small_minimap ? 200 : 130;

	SDL_Rect mmrect = { xpos, ypos, minimap_width, minimap_width };
	minimap_rect_ = mmrect;
	ypos += minimap_width + border_size;

	num_players_label_.set_location(xpos, ypos);
	ypos += num_players_label_.height() + border_size;

	map_size_label_.set_location(xpos, ypos);
	ypos += map_size_label_.height() + 2 * border_size;

	regenerate_map_.set_location(xpos, ypos);
	ypos += regenerate_map_.height() + border_size;
	generator_settings_.set_location(xpos, ypos);
	ypos += generator_settings_.height() + 2 * border_size;

	era_label_.set_location(xpos, ypos);
	ypos += era_label_.height() + border_size;
	era_combo_.set_location(xpos, ypos);
	ypos += era_combo_.height() + border_size;
	password_button_.set_location(xpos, ypos);
	ypos += password_button_.height() + border_size;

#ifdef MP_VISION_OPTIONAL
	vision_combo_.set_location(xpos, ypos);
	ypos += vision_combo_.height() + border_size;
#endif

	// Second column: map menu
	ypos = ypos_columntop;
	xpos += minimap_width + column_border_size;
	map_label_.set_location(xpos, ypos);
	ypos += map_label_.height() + border_size;

	maps_menu_.set_max_width(200);
	maps_menu_.set_max_height(ca.h + ca.y - ypos);
	maps_menu_.set_location(xpos, ypos);
	// Menu dimensions are only updated when items are set. So do this now.
	int mapsel_save = maps_menu_.selection();
	maps_menu_.set_items(map_options_);
	maps_menu_.move_selection(mapsel_save);

	// Third column: big bunch of options
	ypos = ypos_columntop;
	xpos += 200 + column_border_size;

	turns_label_.set_location(xpos, ypos);
	ypos += turns_label_.height() + border_size;
	turns_slider_.set_width(ca.w - xpos);
	turns_slider_.set_location(xpos, ypos);
	ypos += turns_slider_.height() + border_size;

	village_gold_label_.set_location(xpos, ypos);
	ypos += village_gold_label_.height() + border_size;
	village_gold_slider_.set_width(ca.w - xpos);
	village_gold_slider_.set_location(xpos, ypos);
	ypos += village_gold_slider_.height() + border_size;

	xp_modifier_label_.set_location(xpos, ypos);
	ypos += xp_modifier_label_.height() + border_size;
	xp_modifier_slider_.set_width(ca.w - xpos);
	xp_modifier_slider_.set_location(xpos, ypos);
	ypos += xp_modifier_slider_.height() + border_size;

	use_map_settings_.set_location(xpos, ypos);
	fog_game_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += use_map_settings_.height() + border_size;

	observers_game_.set_location(xpos, ypos);
	shroud_game_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += observers_game_.height() + border_size;

	countdown_game_.set_location(xpos, ypos);
	random_start_time_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += countdown_game_.height() + border_size;

	countdown_init_time_label_.set_location(xpos, ypos);
	countdown_turn_bonus_label_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += countdown_init_time_label_.height() + border_size;
	countdown_init_time_slider_.set_width(((ca.w - xpos)/2)-5);
	countdown_turn_bonus_slider_.set_width(((ca.w - xpos)/2)-5);
	countdown_init_time_slider_.set_location(xpos, ypos);
	countdown_turn_bonus_slider_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += countdown_init_time_slider_.height() + border_size;

	countdown_reservoir_time_label_.set_location(xpos, ypos);
	countdown_action_bonus_label_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += countdown_reservoir_time_label_.height() + border_size;
	countdown_reservoir_time_slider_.set_width(((ca.w - xpos)/2)-5);
	countdown_action_bonus_slider_.set_width(((ca.w - xpos)/2)-5);
	countdown_reservoir_time_slider_.set_location(xpos, ypos);
	countdown_action_bonus_slider_.set_location(xpos + (ca.w - xpos)/2 + 5, ypos);
	ypos += countdown_reservoir_time_slider_.height() + border_size;

	// OK / Cancel buttons
	gui::button* left_button = &launch_game_;
	gui::button* right_button = &cancel_game_;

#ifdef OK_BUTTON_ON_RIGHT
	std::swap(left_button,right_button);
#endif

	// Buttons
	right_button->set_location(ca.x + ca.w - right_button->width(),
	                           ca.y + ca.h - right_button->height());
	left_button->set_location(right_button->location().x - left_button->width() -
	                          gui::ButtonHPadding, ca.y + ca.h - left_button->height());
}

#else

void create::layout_children(const SDL_Rect& rect)
{
	ui::layout_children(rect);
	SDL_Rect ca = client_area();

	const int border_size = 2;
	const int column_border_size = 5;

	int xpos = ca.x;
	int ypos = ca.y;

	// OK / Cancel buttons
	gui::button* left_button = &launch_game_;
	gui::button* right_button = &cancel_game_;

#ifdef OK_BUTTON_ON_RIGHT
	std::swap(left_button,right_button);
#endif

	// Dialog title
	ypos += title().height() + border_size;

	// Name Entry
	name_entry_label_.set_location(xpos, ypos);
	name_entry_.set_location(xpos + name_entry_label_.width() + border_size, ypos);
	name_entry_.set_width(ca.w - name_entry_label_.width() - border_size);
	ypos += std::max<int>(name_entry_.height(), name_entry_label_.height()) + border_size;

	// Save ypos here (column top)
	int ypos_columntop = ypos;

	// First column: map list, era, generator
	num_players_label_.set_location(xpos, ypos);
	ypos += num_players_label_.height() + border_size;

	map_size_label_.set_location(xpos, ypos);
	ypos += map_size_label_.height() + border_size;

	map_label_.set_location(xpos, ypos);
	ypos += map_label_.height() + border_size;

	maps_menu_.set_max_width(235);
	maps_menu_.set_max_height(170); //110

	maps_menu_.set_location(xpos, ypos);
	// Menu dimensions are only updated when items are set. So do this now.
	int mapsel_save = maps_menu_.selection();
	maps_menu_.set_items(map_options_);
	maps_menu_.move_selection(mapsel_save);

	ypos += 170 + border_size;

	era_label_.set_location(xpos, ypos);
	era_combo_.set_location(xpos + era_label_.width() + border_size, ypos);
	ypos += era_combo_.height() + border_size;

	use_map_settings_.set_location(xpos, ypos);
	ypos += use_map_settings_.height() + border_size;
	
	regenerate_map_.set_location(xpos, ypos);
	regenerate_map_.hide(true);
	ypos += regenerate_map_.height() + border_size;
	generator_settings_.set_location(xpos, ypos);
	ypos += generator_settings_.height() + border_size;
	

#ifdef MP_VISION_OPTIONAL
	vision_combo_.set_location(xpos, ypos);
	ypos += vision_combo_.height() + border_size;
#endif

	// Second column: map menu
	ypos = ypos_columntop;
	xpos += column_border_size;
	// Third column: big list of options
	ypos = ypos_columntop;

#ifdef __IPHONEOS__
	#ifdef __IPAD__
		xpos += 93 + column_border_size;
	#else
		xpos += 240;
	#endif
#else	
	xpos += 93 + column_border_size;
#endif

	turns_label_.set_location(xpos, ypos);
	ypos += turns_label_.height() + border_size;
	turns_slider_.set_width(100);
	turns_slider_.set_location(xpos, ypos);
	ypos += turns_slider_.height() + border_size;

	village_gold_label_.set_location(xpos, ypos);
	ypos += village_gold_label_.height() + border_size;
	village_gold_slider_.set_width(100);
	village_gold_slider_.set_location(xpos, ypos);
	ypos += village_gold_slider_.height() + border_size;

	xp_modifier_label_.set_location(xpos, ypos);
	ypos += xp_modifier_label_.height() + border_size;
	xp_modifier_slider_.set_width(100);
	xp_modifier_slider_.set_location(xpos, ypos);
	ypos += xp_modifier_slider_.height() + border_size;

	countdown_init_time_label_.set_location(xpos, ypos);
	ypos += countdown_init_time_label_.height() + border_size;

	countdown_init_time_slider_.set_width(100);
	countdown_init_time_slider_.set_location(xpos, ypos);
	ypos += countdown_init_time_slider_.height() + border_size;

	countdown_reservoir_time_label_.set_location(xpos, ypos);
	ypos += countdown_reservoir_time_label_.height() + border_size;

	countdown_reservoir_time_slider_.set_width(100);
	countdown_reservoir_time_slider_.set_location(xpos, ypos);
	ypos += countdown_reservoir_time_slider_.height() + border_size;
	
	random_start_time_.set_location(xpos, ypos);
	ypos += random_start_time_.height() + border_size;


	ypos = ypos_columntop;
#ifdef __IPHONEOS__
	#ifdef __IPAD__
		xpos += 75;
	#else
		xpos += 120;
	#endif
#else
	xpos += 75;
#endif	

	countdown_game_.set_location(xpos, ypos);
	ypos += countdown_game_.height() + border_size;

	fog_game_.set_location(xpos, ypos);
	ypos += fog_game_.height() + border_size;

	shroud_game_.set_location(xpos, ypos);
	ypos += shroud_game_.height() + border_size;

	observers_game_.set_location(xpos, ypos);
	ypos += observers_game_.height() + border_size;

	countdown_turn_bonus_label_.set_location(xpos, ypos);
	ypos += countdown_turn_bonus_label_.height() + border_size;

	countdown_turn_bonus_slider_.set_width(100);
	countdown_turn_bonus_slider_.set_location(xpos, ypos);
	ypos += countdown_turn_bonus_slider_.height() + border_size;

	countdown_action_bonus_label_.set_location(xpos, ypos);
	ypos += countdown_action_bonus_label_.height() + border_size;

	countdown_action_bonus_slider_.set_width(100);
	countdown_action_bonus_slider_.set_location(xpos, ypos);
	ypos += countdown_action_bonus_slider_.height() + 2 * border_size;

	left_button->set_location(307, 291);
	right_button->set_location(307 + left_button->width() + 2 * border_size, 291);
}


#endif

} // namespace mp

