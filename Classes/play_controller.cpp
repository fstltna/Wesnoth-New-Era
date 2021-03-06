/* $Id: play_controller.cpp 33687 2009-03-15 16:50:42Z mordante $ */
/*
   Copyright (C) 2006 - 2009 by Joerg Hinrichs <joerg.hinrichs@alice-dsl.de>
   wesnoth playlevel Copyright (C) 2003 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 *  @file play_controller.cpp
 *  Handle input via mouse & keyboard, events, schedule commands.
 */

#include "play_controller.hpp"
#include "dialogs.hpp"
#include "config_adapter.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "sound.hpp"
#include "unit_id.hpp"
#include "terrain_filter.hpp"

#include "memory_wrapper.h"

#define LOG_NG std::cerr //LOG_STREAM(info, engine)

play_controller::play_controller(const config& level, game_state& state_of_game,
		int ticks, int num_turns, const config& game_config, CVideo& video,
		bool skip_replay) :
	controller_base(ticks, game_config, video),
	verify_manager_(units_),
	team_manager_(teams_),
	prefs_disp_manager_(),
	tooltips_manager_(),
	events_manager_(),
	halo_manager_(),
	labels_manager_(),
	help_manager_(&game_config, &map_),
	mouse_handler_(NULL, teams_, units_, map_, status_, undo_stack_, redo_stack_),
	menu_handler_(NULL, units_, teams_, level, map_, game_config, status_, state_of_game, undo_stack_, redo_stack_),
	soundsources_manager_(),
	gui_(),
	statistics_context_(level["name"]),
	level_(level),
	teams_(),
	gamestate_(state_of_game),
	status_(level, num_turns, &state_of_game),
	map_(game_config, level["map_data"]),
	units_(),
	undo_stack_(),
	redo_stack_(),
	xp_mod_(atoi(level["experience_modifier"].c_str()) > 0 ? atoi(level["experience_modifier"].c_str()) : 100),
	loading_game_(level["playing_team"].empty() == false),
	first_human_team_(-1),
	player_number_(1),
	first_player_(lexical_cast_default<unsigned int, std::string>(level_["playing_team"], 0) + 1),
	start_turn_(status_.turn()),
	is_host_(true),
	skip_replay_(skip_replay),
	linger_(false),
	first_turn_(true),
	savenames_(),
	wml_commands_(),
	victory_music_(),
	defeat_music_()
{
	// Setup victory and defeat music
	set_victory_music_list(level_["victory_music"]);
	set_defeat_music_list(level_["defeat_music"]);

	status_.teams = &teams_;
	game_config::add_color_info(level);
	hotkey::deactivate_all_scopes();
	hotkey::set_scope_active(hotkey::SCOPE_GENERAL);
	hotkey::set_scope_active(hotkey::SCOPE_GAME);
	init(video);
}

play_controller::~play_controller(){
}

void play_controller::init(CVideo& video){
	util::scoped_resource<loadscreen::global_loadscreen_manager*, util::delete_item> scoped_loadscreen_manager;
	loadscreen::global_loadscreen_manager* loadscreen_manager = loadscreen::global_loadscreen_manager::get();
	if (!loadscreen_manager)
	{
		scoped_loadscreen_manager.assign(new loadscreen::global_loadscreen_manager(video));
		loadscreen_manager = scoped_loadscreen_manager.get();
	}

	loadscreen::global_loadscreen->set_progress(50, _("Loading level"));
	// If the recorder has no event, adds an "game start" event
	// to the recorder, whose only goal is to initialize the RNG
	if(recorder.empty()) {
		recorder.add_start();
	} else {
		recorder.pre_replay();
	}
	recorder.set_skip(false);

	const config::child_list& unit_cfg = level_.get_children("side");
	const bool snapshot = utils::string_bool(level_["snapshot"]);

	if(utils::string_bool(level_["modify_placing"])) {
		LOG_NG << "modifying placing...\n";
		place_sides_in_preferred_locations(map_,unit_cfg);
	}



	LOG_NG << "initialized teams... "    << (SDL_GetTicks() - ticks_) << "\n";
	loadscreen::global_loadscreen->set_progress(60, _("Initializing teams"));


	// This *needs* to be created before the show_intro and show_map_scene
	// as that functions use the manager state_of_game
	// Has to be done before registering any events!
	events_manager_.reset(new game_events::manager(level_,map_,
                                                   units_,teams_, gamestate_,status_));

	std::set<std::string> seen_save_ids;

	for(config::child_list::const_iterator ui = unit_cfg.begin(); ui != unit_cfg.end(); ++ui) {
		std::string save_id = get_unique_saveid(**ui, seen_save_ids);
		seen_save_ids.insert(save_id);
		if (first_human_team_ == -1){
			first_human_team_ = get_first_human_team(ui, unit_cfg);
		}
		get_player_info(**ui, gamestate_, save_id, teams_, level_, map_, units_, status_, snapshot);
	}

	LOG_NG << "loading units..." << (SDL_GetTicks() - ticks_) << "\n";
	loadscreen::global_loadscreen->set_progress(70, _("Loading units"));
	preferences::encounter_recruitable_units(teams_);
	preferences::encounter_start_units(units_);
	preferences::encounter_recallable_units(gamestate_);
	preferences::encounter_map_terrain(map_);


	loadscreen::global_loadscreen->set_progress(80, _("Initializing display"));
	LOG_NG << "initializing display... " << (SDL_GetTicks() - ticks_) << "\n";

	const config* theme_cfg = get_theme(game_config_, level_["theme"]);
	if (theme_cfg)
		gui_.reset(new game_display(units_,video,map_,status_,teams_,*theme_cfg, game_config_, level_));
	else
		gui_.reset(new game_display(units_,video,map_,status_,teams_,config(), game_config_, level_));
	loadscreen::global_loadscreen->set_progress(90, _("Initializing display"));
	mouse_handler_.set_gui(gui_.get());
	menu_handler_.set_gui(gui_.get());
	events_manager_->set_gui(*gui_);
	theme::set_known_themes(&game_config_);

	LOG_NG << "done initializing display... " << (SDL_GetTicks() - ticks_) << "\n";

	if(first_human_team_ != -1) {
		gui_->set_team(first_human_team_);
	}
	else if (team_manager_.is_observer())
	{
		// Find first team that is allowed to be observered.
		// If not set here observer would be without fog untill
		// the first turn of observerable side
		size_t i;
		for (i=0;i < teams_.size();++i)
		{
			if (!teams_[i].get_disallow_observers())
			{
				gui_->set_team(i);
			}
		}
	}

	browse_ = true;

	init_managers();
	// add era events for MP game
	const config* era_cfg = level_.child("era");
	if (era_cfg != NULL) {
		game_events::add_events(era_cfg->get_children("event"),"era_events");
	}



	loadscreen::global_loadscreen->set_progress(100, _("Starting game"));
	loadscreen_manager->reset();
}

void play_controller::init_managers(){
	LOG_NG << "initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
	prefs_disp_manager_.reset(new preferences::display_manager(gui_.get()));
	tooltips_manager_.reset(new tooltips::manager(gui_->video()));
	soundsources_manager_.reset(new soundsource::manager(*gui_));

	events_manager_->set_soundsource(*soundsources_manager_);

	halo_manager_.reset(new halo::manager(*gui_));
	LOG_NG << "done initializing managers... " << (SDL_GetTicks() - ticks_) << "\n";
	
	memory_stats("Finished loading level");
}

static int placing_score(const config& side, const gamemap& map, const map_location& pos)
{
	int positions = 0, liked = 0;
	const t_translation::t_list terrain = t_translation::read_list(side["terrain_liked"]);

	for(int i = pos.x-8; i != pos.x+8; ++i) {
		for(int j = pos.y-8; j != pos.y+8; ++j) {
			const map_location pos(i,j);
			if(map.on_board(pos)) {
				++positions;
				if(std::count(terrain.begin(),terrain.end(),map[pos])) {
					++liked;
				}
			}
		}
	}

	return (100*liked)/positions;
}

struct placing_info {

	placing_info() :
		side(0),
		score(0),
		pos()
	{
	}

	int side, score;
	map_location pos;
};

static bool operator<(const placing_info& a, const placing_info& b) { return a.score > b.score; }

void play_controller::place_sides_in_preferred_locations(gamemap& map, const config::child_list& sides)
{
	std::vector<placing_info> placings;

	const int num_pos = map.num_valid_starting_positions();

	for(config::child_list::const_iterator s = sides.begin(); s != sides.end(); ++s) {
		const int side_num = s - sides.begin() + 1;
		for(int p = 1; p <= num_pos; ++p) {
			const map_location& pos = map.starting_position(p);
			const int score = placing_score(**s,map,pos);
			placing_info obj;
			obj.side = side_num;
			obj.score = score;
			obj.pos = pos;
			placings.push_back(obj);
		}
	}

	std::sort(placings.begin(),placings.end());
	std::set<int> placed;
	std::set<map_location> positions_taken;

	for(std::vector<placing_info>::const_iterator i = placings.begin(); i != placings.end() && placed.size() != sides.size(); ++i) {
		if(placed.count(i->side) == 0 && positions_taken.count(i->pos) == 0) {
			placed.insert(i->side);
			positions_taken.insert(i->pos);
			map.set_starting_position(i->side,i->pos);
			LOG_NG << "placing side " << i->side << " at " << i->pos << '\n';
		}
	}
}

void play_controller::objectives(){
	menu_handler_.objectives(player_number_);
}

void play_controller::show_statistics(){
	menu_handler_.show_statistics(gui_->viewing_team()+1);
}

void play_controller::unit_list(){
	menu_handler_.unit_list();
}

void play_controller::status_table(){
	menu_handler_.status_table();
}

void play_controller::save_game(){
	menu_handler_.save_game("",gui::OK_CANCEL);
}

void play_controller::save_replay(){
	menu_handler_.save_game("", gui::OK_CANCEL, false, true);
}

void play_controller::save_map(){
	menu_handler_.save_map();
}

void play_controller::load_game(){
	draw_wait_cursor();
	menu_handler_.load_game();
}

void play_controller::preferences(){
	draw_wait_cursor();
	menu_handler_.preferences();
}

void play_controller::cycle_units(){
	mouse_handler_.cycle_units(browse_);
}

void play_controller::cycle_back_units(){
	mouse_handler_.cycle_back_units(browse_);
}

void play_controller::show_chat_log(){
	menu_handler_.show_chat_log();
}

void play_controller::show_help(){
	menu_handler_.show_help();
}

void play_controller::undo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.undo(player_number_);
}

void play_controller::redo(){
	// deselect unit (only here, not to be done when undoing attack-move)
	mouse_handler_.deselect_hex();
	menu_handler_.redo(player_number_);
}

void play_controller::show_enemy_moves(bool ignore_units){
	menu_handler_.show_enemy_moves(ignore_units, player_number_);
}

void play_controller::goto_leader(){
	menu_handler_.goto_leader(player_number_);
}

void play_controller::unit_description(){
	menu_handler_.unit_description(mouse_handler_);
}

void play_controller::toggle_grid(){
	menu_handler_.toggle_grid();
}

void play_controller::search(){
	menu_handler_.search();
}


void play_controller::fire_prestart(bool execute){
	// pre-start events must be executed before any GUI operation,
	// as those may cause the display to be refreshed.
	if (execute){
		update_locker lock_display(gui_->video());
		game_events::fire("prestart");
		// prestart event may modify start turn with WML, reflect any changes.
		start_turn_ = status_.turn();
	}
}

void play_controller::fire_start(bool execute){
	if(execute) {
		game_events::fire("start");
		// start event may modify start turn with WML, reflect any changes.
		start_turn_ = status_.turn();
		gamestate_.set_variable("turn_number", str_cast<size_t>(start_turn_));
		first_turn_ = true;
	} else {
		first_turn_ = false;
	}
}

void play_controller::init_gui(){
	gui_->begin_game();
	gui_->adjust_colours(0,0,0);

	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		::clear_shroud(*gui_,map_,units_,teams_,(t-teams_.begin()));
	}
}

void play_controller::init_side(const unsigned int team_index, bool /*is_replay*/){
	log_scope("player turn");
	team& current_team = teams_[team_index];

	mouse_handler_.set_team(team_index+1);

	// If we are observers we move to watch next team if it is allowed
	if (team_manager_.is_observer()
		&& !current_team.get_disallow_observers()) {
		gui_->set_team(size_t(team_index));
	}
		gui_->set_playing_team(size_t(team_index));

	std::stringstream player_number_str;
	player_number_str << player_number_;
	gamestate_.set_variable("side_number",player_number_str.str());
	gamestate_.last_selected = map_location::null_location;

	/*
		Normally, events must not be actively fired through replays, because
		they have been recorded previously and therefore will get executed anyway.
		Firing them in the normal code would lead to double execution.
		However, the following events are different in that they need to be executed _now_
		(before calculation of income and healing) or we will risk OOS errors if we manipulate
		these informations inside the events and in the replay have a different order of execution.
	*/
	bool real_side_change = true;
	if(first_turn_) {
		first_turn_ = false;
		game_events::fire("turn " + str_cast<size_t>(start_turn_));
		game_events::fire("new turn");
		game_events::fire("side turn");
	} else if (team_index != (first_player_ - 1) || status_.turn() > start_turn_) {
		// Fire side turn event only if real side change occurs,
		// not counting changes from void to a side
		game_events::fire("side turn");
	} else {
		real_side_change = false;
	}

	// We want to work out if units for this player should get healed,
	// and the player should get income now.
	// Healing/income happen if it's not the first turn of processing,
	// or if we are loading a game, and this is not the player it started with.
	const bool turn_refresh = status_.turn() > start_turn_ || (loading_game_ && team_index != (first_player_ - 1));

	if(turn_refresh) {
		for(unit_map::iterator i = units_.begin(); i != units_.end(); ++i) {
			if(i->second.side() == static_cast<size_t>(player_number_)) {
				i->second.new_turn();
			}
		}

		current_team.new_turn();

		// If the expense is less than the number of villages owned,
		// then we don't have to pay anything at all
		const int expense = team_upkeep(units_,player_number_) -
								current_team.villages().size();
		if(expense > 0) {
			current_team.spend_gold(expense);
		}

		calculate_healing((*gui_),map_,units_,player_number_,teams_, !skip_replay_);
		reset_resting(units_, player_number_);
	}
	if(turn_refresh || real_side_change) {
		game_events::fire("turn refresh");
	}

	const time_of_day &tod = status_.get_time_of_day();
	current_team.set_time_of_day(int(status_.turn()), tod);

	if(team_index == first_player_ - 1)
		sound::play_sound(tod.sounds, sound::SOUND_SOURCES);

	if (!recorder.is_skipping()){
		::clear_shroud(*gui_,map_,units_,teams_,team_index);
		gui_->invalidate_all();
	}

	if (!recorder.is_skipping() && !skip_replay_){
		gui_->scroll_to_leader(units_, player_number_,game_display::ONSCREEN,false);
	}
}

void play_controller::finish_side_turn(){
	for(unit_map::iterator uit = units_.begin(); uit != units_.end(); ++uit) {
		if(uit->second.side() == player_number_)
			uit->second.end_turn();
	}

	// This implements "delayed map sharing."
	// It is meant as an alternative to shared vision.
	if(current_team().copy_ally_shroud()) {
		gui_->recalculate_minimap();
		gui_->invalidate_all();
	}

	mouse_handler_.deselect_hex();
	n_unit::id_manager::instance().reset_fake();
	game_events::pump();
}

void play_controller::finish_turn(){
	std::stringstream event_stream;
	event_stream << status_.turn();

	{
		LOG_NG << "turn event..." << (recorder.is_skipping() ? "skipping" : "no skip") << "\n";
		update_locker lock_display(gui_->video(),recorder.is_skipping());
		const std::string turn_num = event_stream.str();
		gamestate_.set_variable("turn_number",turn_num);
		game_events::fire("turn " + turn_num);
		game_events::fire("new turn");
	}
}

bool play_controller::enemies_visible() const
{
	// If we aren't using fog/shroud, this is easy :)
	if(current_team().uses_fog() == false && current_team().uses_shroud() == false)
		return true;

	// See if any enemies are visible
	for(unit_map::const_iterator u = units_.begin(); u != units_.end(); ++u)
		if(current_team().is_enemy(u->second.side()) && !gui_->fogged(u->first))
			return true;

	return false;
}

bool play_controller::execute_command(hotkey::HOTKEY_COMMAND command, int index)
{
	if(index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if(i < savenames_.size() && !savenames_[i].empty()) {
			// Load the game by throwing load_game_exception
			throw game::load_game_exception(savenames_[i],false,false);

		} else if (i < wml_commands_.size() && wml_commands_[i] != NULL) {
			if(gamestate_.last_selected.valid() && wml_commands_[i]->needs_select) {
				recorder.add_event("select", gamestate_.last_selected);
			}
			map_location const& menu_hex = mouse_handler_.get_last_hex();
			recorder.add_event(wml_commands_[i]->name, menu_hex);
			if(game_events::fire(wml_commands_[i]->name, menu_hex)) {
				// The event has mutated the gamestate
				apply_shroud_changes(undo_stack_, gui_.get(), map_,
					units_, teams_, (player_number_ - 1));
				undo_stack_.clear();
			}
			return true;
		}
	}
	return command_executor::execute_command(command, index);
}

bool play_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	if(index >= 0) {
		unsigned i = static_cast<unsigned>(index);
		if((i < savenames_.size() && !savenames_[i].empty())
		|| (i < wml_commands_.size() && wml_commands_[i] != NULL)) {
			return true;
		}
	}
	switch(command) {

	// Commands we can always do:
	case hotkey::HOTKEY_LEADER:
	case hotkey::HOTKEY_CYCLE_UNITS:
	case hotkey::HOTKEY_CYCLE_BACK_UNITS:
	case hotkey::HOTKEY_ZOOM_IN:
	case hotkey::HOTKEY_ZOOM_OUT:
	case hotkey::HOTKEY_ZOOM_DEFAULT:
	case hotkey::HOTKEY_FULLSCREEN:
	case hotkey::HOTKEY_SCREENSHOT:
	case hotkey::HOTKEY_MAP_SCREENSHOT:
	case hotkey::HOTKEY_ACCELERATED:
	case hotkey::HOTKEY_SAVE_MAP:
	case hotkey::HOTKEY_TOGGLE_GRID:
	case hotkey::HOTKEY_MOUSE_SCROLL:
	case hotkey::HOTKEY_STATUS_TABLE:
	case hotkey::HOTKEY_MUTE:
	case hotkey::HOTKEY_PREFERENCES:
	case hotkey::HOTKEY_OBJECTIVES:
	case hotkey::HOTKEY_UNIT_LIST:
	case hotkey::HOTKEY_STATISTICS:
	case hotkey::HOTKEY_QUIT_GAME:
	case hotkey::HOTKEY_SEARCH:
	case hotkey::HOTKEY_HELP:
	case hotkey::HOTKEY_USER_CMD:
	case hotkey::HOTKEY_CUSTOM_CMD:
	case hotkey::HOTKEY_AI_FORMULA:
	case hotkey::HOTKEY_CLEAR_MSG:
#ifdef USRCMD2
//%%
	case hotkey::HOTKEY_USER_CMD_2:
	case hotkey::HOTKEY_USER_CMD_3:
#endif
		return true;

	// Commands that have some preconditions:
	case hotkey::HOTKEY_SAVE_GAME:
	case hotkey::HOTKEY_SAVE_REPLAY:
		return !events::commands_disabled;

	case hotkey::HOTKEY_SHOW_ENEMY_MOVES:
	case hotkey::HOTKEY_BEST_ENEMY_MOVES:
		return !linger_ && enemies_visible();

	case hotkey::HOTKEY_LOAD_GAME:
		return network::nconnections() == 0; // Can only load games if not in a network game

	case hotkey::HOTKEY_CHAT_LOG:
		return network::nconnections() > 0;

	case hotkey::HOTKEY_REDO:
		return !linger_ && !redo_stack_.empty() && !events::commands_disabled && !browse_;
	case hotkey::HOTKEY_UNDO:
		return !linger_ && !undo_stack_.empty() && !events::commands_disabled && !browse_;

	case hotkey::HOTKEY_UNIT_DESCRIPTION:
		return menu_handler_.current_unit(mouse_handler_) != units_.end();

	case hotkey::HOTKEY_RENAME_UNIT:
		return !events::commands_disabled &&
			menu_handler_.current_unit(mouse_handler_) != units_.end() &&
			!(menu_handler_.current_unit(mouse_handler_)->second.unrenamable()) &&
			menu_handler_.current_unit(mouse_handler_)->second.side() == gui_->viewing_team()+1 &&
			teams_[menu_handler_.current_unit(mouse_handler_)->second.side() - 1].is_human();
	// KP: added
	case hotkey::HOTKEY_END_UNIT_TURN:
			return !events::commands_disabled &&
			menu_handler_.current_unit(mouse_handler_) != units_.end() &&
			menu_handler_.current_unit(mouse_handler_)->second.side() == gui_->viewing_team()+1 &&
			teams_[menu_handler_.current_unit(mouse_handler_)->second.side() - 1].is_human();
			
	default:
		return false;
	}
}

void play_controller::enter_textbox()
{
	if(menu_handler_.get_textbox().active() == false) {
		return;
	}

	const std::string str = menu_handler_.get_textbox().box()->text();
	const unsigned int team_num = player_number_;
	events::mouse_handler& mousehandler = mouse_handler_;

	switch(menu_handler_.get_textbox().mode()) {
	case gui::TEXTBOX_SEARCH:
		menu_handler_.do_search(str);
		menu_handler_.get_textbox().close(*gui_);
		break;
	case gui::TEXTBOX_MESSAGE:
		menu_handler_.do_speak();
		menu_handler_.get_textbox().close(*gui_);  //need to close that one after executing do_speak() !
		break;
	case gui::TEXTBOX_COMMAND:
		menu_handler_.get_textbox().close(*gui_);
		menu_handler_.do_command(str, team_num, mousehandler);
		break;
	case gui::TEXTBOX_AI:
		menu_handler_.get_textbox().close(*gui_);
		menu_handler_.do_ai_formula(str, team_num, mousehandler);
		break;
	default:
		menu_handler_.get_textbox().close(*gui_);
		LOG_STREAM(err, display) << "unknown textbox mode\n";
	}

}

team& play_controller::current_team()
{
	assert(player_number_ > 0 && player_number_ <= teams_.size());
	return teams_[player_number_-1];
}

const team& play_controller::current_team() const
{
	assert(player_number_ > 0 && player_number_ <= teams_.size());
	return teams_[player_number_-1];
}

int play_controller::find_human_team_before(const size_t team_num) const
{
	if (team_num > teams_.size())
		return -2;

	int human_side = -2;
	for (int i = team_num-2; i > -1; --i) {
		if (teams_[i].is_human()) {
			human_side = i;
			break;
		}
	}
	if (human_side == -2) {
		for (size_t i = teams_.size()-1; i > team_num-1; --i) {
			if (teams_[i].is_human()) {
				human_side = i;
				break;
			}
		}
	}
	return human_side+1;
}

void play_controller::slice_before_scroll() {
	soundsources_manager_->update();
}

void play_controller::slice_end() {
	if(!browse_ && current_team().objectives_changed()) {
		dialogs::show_objectives(*gui_, level_, current_team().objectives());
		current_team().reset_objectives_changed();
	}
}

events::mouse_handler& play_controller::get_mouse_handler_base() {
	return mouse_handler_;
}

game_display& play_controller::get_display() {
	return *gui_;
}

bool play_controller::have_keyboard_focus()
{
	return !menu_handler_.get_textbox().active();
}

void play_controller::process_keydown_event(const SDL_Event& event) {
	if(event.key.keysym.sym == SDLK_ESCAPE) {
		menu_handler_.get_textbox().close(*gui_);
	} else if(event.key.keysym.sym == SDLK_TAB) {
		menu_handler_.get_textbox().tab(teams_, units_, *gui_);
	} else if(event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) {
		enter_textbox();
	}
}

void play_controller::process_keyup_event(const SDL_Event& event) {
	// If the user has pressed 1 through 9, we want to show
	// how far the unit can move in that many turns
	if(event.key.keysym.sym >= '1' && event.key.keysym.sym <= '7') {
		const int new_path_turns = (event.type == SDL_KEYDOWN) ?
		                           event.key.keysym.sym - '1' : 0;

		if(new_path_turns != mouse_handler_.get_path_turns()) {
			mouse_handler_.set_path_turns(new_path_turns);

			const unit_map::iterator u = mouse_handler_.selected_unit();

			if(u != units_.end()) {
				const bool teleport = u->second.get_ability_bool("teleport",u->first);

				// if it's not the unit's turn, we reset its moves
				unit_movement_resetter move_reset(u->second, u->second.side() != player_number_);

				mouse_handler_.set_current_paths(paths(map_,units_,u->first,
				                       teams_,false,teleport, teams_[gui_->viewing_team()],
				                       mouse_handler_.get_path_turns()));
				gui_->highlight_reach(mouse_handler_.get_current_paths());
			}
		}
	}
}

void play_controller::post_mouse_press(const SDL_Event& /*event*/)
{
	if (mouse_handler_.get_undo()){
		mouse_handler_.set_undo(false);
		menu_handler_.undo(player_number_);
	}
}

static void trim_items(std::vector<shared_string>& newitems) {
	if (newitems.size() > 5) {
		std::vector<shared_string> subitems;
		subitems.push_back(newitems[0]);
		subitems.push_back(newitems[1]);
		subitems.push_back(newitems[newitems.size() / 3]);
		subitems.push_back(newitems[newitems.size() * 2 / 3]);
		subitems.push_back(newitems.back());
		newitems = subitems;
	}
}

void play_controller::expand_autosaves(std::vector<shared_string>& items)
{
	savenames_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "AUTOSAVES") {
			items.erase(items.begin() + i);
			std::vector<shared_string> newitems;
			std::vector<shared_string> newsaves;
			for (unsigned int turn = status_.turn(); turn != 0; turn--) {
				std::string name = gamestate_.label + "-" + _("Auto-Save") + lexical_cast<std::string>(turn);
				if (save_game_exists(name)) {
					if(preferences::compress_saves()) {
						newsaves.push_back(name + ".gz");
					} else {
						newsaves.push_back(name);
					}
					newitems.push_back(_("Back to turn ") + lexical_cast<std::string>(turn));
				}
			}

			// Make sure list doesn't get too long: keep top two,
			// midpoint and bottom.
			trim_items(newitems);
			trim_items(newsaves);

			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			savenames_.insert(savenames_.end(), newsaves.begin(), newsaves.end());
			break;
		}
		savenames_.push_back("");
	}
}

void play_controller::expand_wml_commands(std::vector<shared_string>& items)
{
	wml_commands_.clear();
	for (unsigned int i = 0; i < items.size(); ++i) {
		if (items[i] == "wml") {
			items.erase(items.begin() + i);
			std::map<shared_string, wml_menu_item*>& gs_wmi = gamestate_.wml_menu_items;
			if(gs_wmi.empty())
				break;
			std::vector<shared_string> newitems;

			char buf[50];
			const map_location& hex = mouse_handler_.get_last_hex();
			snprintf(buf,sizeof(buf),"%d",hex.x+1);
			gamestate_.set_variable("x1", buf);
			snprintf(buf,sizeof(buf),"%d",hex.y+1);
			gamestate_.set_variable("y1", buf);
			scoped_xy_unit highlighted_unit("unit", hex.x, hex.y, units_);

			std::map<shared_string, wml_menu_item*>::iterator itor;
			for (itor = gs_wmi.begin(); itor != gs_wmi.end()
				&& newitems.size() < MAX_WML_COMMANDS; ++itor) {
				config& show_if = itor->second->show_if;
				config filter_location = itor->second->filter_location;
				if ((show_if.empty()
					|| game_events::conditional_passed(&units_, &show_if))
				&& (filter_location.empty()
					|| terrain_filter(&filter_location, map_, status_, units_)(hex))
				&& (!itor->second->needs_select
					|| gamestate_.last_selected.valid()))
				{
					wml_commands_.push_back(itor->second);
					std::string newitem = itor->second->description;

					// Prevent accidental hotkey binding by appending a space
					newitem.push_back(' ');
					newitems.push_back(newitem);
				}
			}
			items.insert(items.begin()+i, newitems.begin(), newitems.end());
			break;
		}
		wml_commands_.push_back(NULL);
	}
}

void play_controller::show_menu(const std::vector<shared_string>& items_arg, int xloc, int yloc, bool context_menu)
{
	std::vector<shared_string> items = items_arg;
	hotkey::HOTKEY_COMMAND command;
	std::vector<shared_string>::iterator i = items.begin();
	while(i != items.end()) {
		if (*i == "AUTOSAVES") {
			// Autosave visibility is similar to LOAD_GAME hotkey
			command = hotkey::HOTKEY_LOAD_GAME;
		} else {
			command = hotkey::get_hotkey(*i).get_id();
		}
		// Remove WML commands if they would not be allowed here
		if(*i == "wml") {
			if(!context_menu || gui_->viewing_team() != gui_->playing_team()
			|| events::commands_disabled || !teams_[gui_->viewing_team()].is_human()) {
				i = items.erase(i);
				continue;
			}
		// Remove commands that can't be executed or don't belong in this type of menu
		} else if(!can_execute_command(command)
		|| (context_menu && !in_context_menu(command))) {
			i = items.erase(i);
			continue;
		}
		++i;
	}

	// Add special non-hotkey items to the menu and remember their indeces
	expand_autosaves(items);
	expand_wml_commands(items);

	if(items.empty())
		return;

	command_executor::show_menu(items, xloc, yloc, context_menu, *gui_);
	gui_->draw();
}

bool play_controller::in_context_menu(hotkey::HOTKEY_COMMAND command) const
{
	switch(command) {
	// Only display these if the mouse is over a castle or keep tile
	case hotkey::HOTKEY_RECRUIT:
	case hotkey::HOTKEY_REPEAT_RECRUIT:
	case hotkey::HOTKEY_RECALL: {
		// last_hex_ is set by mouse_events::mouse_motion
		// Enable recruit/recall on castle/keep tiles
		const unit_map::const_iterator leader = team_leader(player_number_,units_);
		if (leader != units_.end()) {
			return can_recruit_on(map_, leader->first, mouse_handler_.get_last_hex());
		} else {
			return false;
		}
	}
	default:
		return true;
	}
}

std::string play_controller::get_action_image(hotkey::HOTKEY_COMMAND command, int index) const
{
	if(index >= 0 && index < static_cast<int>(wml_commands_.size())) {
		wml_menu_item* const& wmi = wml_commands_[index];
		if(wmi != NULL) {
			return wmi->image.empty() ? game_config::wml_menu_image : wmi->image;
		}
	}
	return command_executor::get_action_image(command, index);
}

hotkey::ACTION_STATE play_controller::get_action_state(hotkey::HOTKEY_COMMAND command, int /*index*/) const
{
	switch(command) {
	case hotkey::HOTKEY_DELAY_SHROUD:
		return current_team().auto_shroud_updates() ? hotkey::ACTION_OFF : hotkey::ACTION_ON;
	default:
		return hotkey::ACTION_STATELESS;
	}
}

namespace {
	static const std::string empty_str = "";
}

const std::string& play_controller::select_victory_music() const
{
	if(victory_music_.empty())
		return empty_str;

	const size_t p = gamestate_.rng().get_random() % victory_music_.size();
	assert(p < victory_music_.size());
	return victory_music_[p];
}

const std::string& play_controller::select_defeat_music() const
{
	if(defeat_music_.empty())
		return empty_str;

	const size_t p = gamestate_.rng().get_random() % defeat_music_.size();
	assert(p < defeat_music_.size());
	return defeat_music_[p];
}


void play_controller::set_victory_music_list(const std::string& list)
{
	victory_music_ = utils::split(list);
	if(victory_music_.empty())
		victory_music_ = utils::split(game_config::default_victory_music);
}

void play_controller::set_defeat_music_list(const std::string& list)
{
	defeat_music_  = utils::split(list);
	if(defeat_music_.empty())
		defeat_music_ = utils::split(game_config::default_defeat_music);
}

