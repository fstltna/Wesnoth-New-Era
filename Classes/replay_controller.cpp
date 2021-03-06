/* $Id: replay_controller.cpp 36212 2009-06-14 21:59:53Z soliton $ */
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

#include "global.hpp"

#include "config_adapter.hpp"
#include "game_end_exceptions.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "replay_controller.hpp"


#define DBG_NG LOG_STREAM(debug, engine)
#define DBG_REPLAY LOG_STREAM(debug, replay)
#define LOG_REPLAY LOG_STREAM(info, replay)
#define ERR_REPLAY LOG_STREAM(err, replay)

LEVEL_RESULT play_replay_level(const config& game_config,
		const config* level, CVideo& video, game_state& state_of_game)
{
	try{
		const int ticks = SDL_GetTicks();
		const int num_turns = atoi((*level)["turns"].c_str());
		DBG_NG << "creating objects... " << (SDL_GetTicks() - ticks) << "\n";
		replay_controller replaycontroller(*level, state_of_game, ticks, num_turns, game_config, video);
		DBG_NG << "created objects... " << (SDL_GetTicks() - replaycontroller.get_ticks()) << "\n";
		const events::command_disabler disable_commands;

		//replay event-loop
		for (;;){
			replaycontroller.play_slice();
		}
	}
	catch(end_level_exception&){
		DBG_NG << "play_replay_level: end_level_exception\n";
	} catch (replay::error&) {
	}

	return VICTORY;
}

replay_controller::replay_controller(const config& level,
		game_state& state_of_game, const int ticks, const int num_turns,
		const config& game_config, CVideo& video) :
	play_controller(level, state_of_game, ticks, num_turns, game_config, video, false),
	teams_start_(),
	gamestate_start_(state_of_game),
	status_start_(level, num_turns, &state_of_game),
	units_start_(),
	current_turn_(1),
	delay_(0),
	is_playing_(false),
	show_everything_(false),
	show_team_(state_of_game.campaign_type == "multiplayer" ? 0 : 1),
	skip_next_turn_(false)
{
	init();
	gamestate_start_ = gamestate_;
}

replay_controller::~replay_controller(){
	//YogiHH
	//not absolutely sure if this is needed, but it makes me feel a lot better ;-)
	//feel free to delete this if it is not necessary
	gui_->get_theme().theme_reset().detach_handler(this);
}

bool replay_controller::continue_replay() {
	return gui::dialog(*gui_,"",_("The file you have tried to load is corrupt."
			" Continue playing?"),gui::OK_CANCEL).show();
}

void replay_controller::init(){
	DBG_REPLAY << "in replay_controller::init()...\n";

	//guarantee the cursor goes back to 'normal' at the end of the level
	const cursor::setter cursor_setter(cursor::NORMAL);
	init_replay_display();

	try {
		fire_prestart(true);
	} catch (replay::error&) {
		if(!continue_replay()) {
			throw;
		}
	}
	init_gui();
	statistics::fresh_stats();
	victory_conditions::set_victory_when_enemies_defeated(
		utils::string_bool(level_["victory_when_enemies_defeated"], true)
	);

	DBG_REPLAY << "first_time..." << (recorder.is_skipping() ? "skipping" : "no skip") << "\n";

	try {
		fire_start(!loading_game_);
	} catch (replay::error&) {
		if(!continue_replay()) {
			throw;
		}
	}
	update_gui();

	units_start_ = units_;
	teams_start_ = teams_;
}

void replay_controller::init_gui(){
	DBG_NG << "Initializing GUI... " << (SDL_GetTicks() - ticks_) << "\n";
	play_controller::init_gui();

	if (show_team_)
		gui_->set_team(show_team_ - 1, show_everything_);
	else
		gui_->set_team(0, show_everything_);

	gui_->scroll_to_leader(units_, player_number_, display::WARP);
	update_locker lock_display((*gui_).video(),false);
	for(std::vector<team>::iterator t = teams_.begin(); t != teams_.end(); ++t) {
		t->reset_objectives_changed();
	}
}

void replay_controller::init_replay_display(){
	DBG_REPLAY << "initializing replay-display... " << (SDL_GetTicks() - ticks_) << "\n";

	rebuild_replay_theme();
	gui_->get_theme().theme_reset().attach_handler(this);
	DBG_REPLAY << "done initializing replay-display... " << (SDL_GetTicks() - ticks_) << "\n";
}

void replay_controller::rebuild_replay_theme(){
	const config* theme_cfg = get_theme(game_config_, level_["theme"]);
	if (theme_cfg) {
		const config* replay_theme_cfg = theme_cfg->child("resolution")->child("replay");
		if (NULL != replay_theme_cfg)
			gui_->get_theme().modify(replay_theme_cfg);
		//Make sure we get notified if the theme is redrawn completely. That way we have
		//a chance to restore the replay controls of the theme as well.
		gui_->invalidate_theme();
	}
}

void replay_controller::reset_replay(){
	gui::button* b = gui_->find_button("button-playreplay");
	if (b != NULL) { b->release(); }
	b = gui_->find_button("button-stopreplay");
	if (b != NULL) { b->release(); }
	gui_->clear_chat_messages();
	is_playing_ = false;
	player_number_ = 1;
	current_turn_ = 1;
	recorder.start_replay();
	units_ = units_start_;
	status_ = status_start_;
	gamestate_ = gamestate_start_;
	teams_ = teams_start_;
	statistics::fresh_stats();
	if (events_manager_ ){
		// NOTE: this double reset is required so that the new
		// instance of game_events::manager isn't created before the
		// old manager is actually destroyed (triggering an assertion
		// failure)
		events_manager_.reset();
		events_manager_.reset(new game_events::manager(level_,map_,
								units_,teams_, gamestate_,status_));
		events_manager_->set_gui(*gui_);
		events_manager_->set_soundsource(*soundsources_manager_);
	}

	gui_->new_turn();
	gui_->invalidate_game_status();
	events::raise_draw_event();
	(*gui_).invalidate_all();
	(*gui_).draw();

	try {
		fire_prestart(true);
		fire_start(!loading_game_);
	} catch (replay::error&) {
		if(!continue_replay()) {
			throw;
		}
	}
	gui_->new_turn();
	gui_->invalidate_game_status();
	events::raise_draw_event();
	(*gui_).invalidate_all();
	(*gui_).draw();
	b = gui_->find_button("button-resetreplay");
	if (b != NULL) { b->release(); }
}

void replay_controller::stop_replay(){
	is_playing_ = false;
	gui::button* b = gui_->find_button("button-playreplay");
	if (b != NULL) { b->release(); }
}

void replay_controller::replay_next_turn(){
	is_playing_ = true;
	play_turn();

 	if (!skip_replay_){
		gui_->scroll_to_leader(units_, player_number_,game_display::ONSCREEN,false);
	}
	is_playing_ = false;
	gui::button* b = gui_->find_button("button-nextturn");
	if (b != NULL) { b->release(); }
}

void replay_controller::replay_next_side(){
	is_playing_ = true;
	play_side(player_number_ - 1, false);

	if (static_cast<size_t>(player_number_) > teams_.size()) {
		player_number_ = 1;
		current_turn_++;
	}

	if (!skip_replay_) {
		gui_->scroll_to_leader(units_, player_number_,game_display::ONSCREEN,false);
	}

	is_playing_ = false;
	gui::button* b = gui_->find_button("button-nextside");
	if (b != NULL) { b->release(); }
}

void replay_controller::replay_show_everything(){
	show_everything_ = true;
	show_team_ = 0;
	update_teams();
	update_gui();
}

void replay_controller::replay_show_each(){
	show_everything_ = false;
	show_team_ = 0;
	update_teams();
	update_gui();
}

void replay_controller::replay_show_team1(){
	show_everything_ = false;
	show_team_ = 1;
	update_teams();
	update_gui();
}

void replay_controller::replay_skip_animation(){
	recorder.set_skip(!recorder.is_skipping());
	skip_replay_ = !skip_replay_;
}

void replay_controller::play_replay(){
	gui::button* b = gui_->find_button("button-stopreplay");
	if (b != NULL) { b->release(); }
	if (recorder.at_end()){
		return;
	}

	try{
		is_playing_ = true;

		DBG_REPLAY << "starting main loop\n" << (SDL_GetTicks() - ticks_) << "\n";
		for(; !recorder.at_end() && is_playing_; first_player_ = 1) {
			try{
				play_turn();
			}
			catch (replay::error&) //when user due to error want stop playing
			{
				is_playing_ = false;
			}
		} //end for loop
		is_playing_ = false;
	}
	catch(end_level_exception& e){
		if (e.result == QUIT) { throw e; }
	}
}

void replay_controller::play_turn(){
	if (recorder.at_end()){
		return;
	}

	LOG_REPLAY << "turn: " << current_turn_ << "\n";

	gui_->new_turn();
	gui_->invalidate_game_status();
	events::raise_draw_event();

	bool last_team = false;

	while ( (!last_team) && (!recorder.at_end()) && is_playing_ ){
		last_team = static_cast<size_t>(player_number_) == teams_.size();
		play_side(player_number_ - 1, false);
		play_slice();
	}
}

void replay_controller::play_side(const unsigned int /*team_index*/, bool){
	if (recorder.at_end()){
		return;
	}

	DBG_REPLAY << "Status turn number: " << status_.turn() << "\n";
	DBG_REPLAY << "Replay_Controller turn number: " << current_turn_ << "\n";
	DBG_REPLAY << "Player number: " << player_number_ << "\n";

	try{
		// If a side is empty skip over it.
		if (!current_team().is_empty()) {
			statistics::reset_turn_stats(current_team().save_id());

			try {
				if (skip_next_turn_) {
					skip_next_turn_ = false;
					throw end_turn_exception();
				}
				play_controller::init_side(player_number_ - 1, true);

				DBG_REPLAY << "doing replay " << player_number_ << "\n";
				try {
					::do_replay(*gui_, map_, units_, teams_,
							player_number_, status_, gamestate_);
				} catch(replay::error&) {
					if(!continue_replay()) {
						throw;
					}
				}
			} catch(end_turn_exception) {
			}

			finish_side_turn();

			// This is necessary for replays in order to show possible movements.
			for (unit_map::iterator uit = units_.begin(); uit != units_.end(); ++uit) {
				if (uit->second.side() != static_cast<size_t>(player_number_)) {
					uit->second.new_turn();
				}
			}
		}

		player_number_++;

		if (static_cast<size_t>(player_number_) > teams_.size()) {
			status_.next_turn();
			try {
				finish_turn();
			} catch (end_turn_exception) {
				skip_next_turn_ = true;
			}
			player_number_ = 1;
			current_turn_++;
		}

		update_teams();
		update_gui();
	}
	catch (replay::error&) //if replay throws an error, we don't want to get thrown out completely
	{
		is_playing_ = false;
	}
	catch(end_level_exception& e){
		//VICTORY/DEFEAT end_level_exception shall not return to title screen
		if ((e.result != VICTORY) && (e.result != DEFEAT)) { throw e; }
	}
}

void replay_controller::update_teams(){
	int next_team = player_number_;
	if(static_cast<size_t>(next_team) > teams_.size()) {
		next_team = 1;
	}

	if (!show_team_)
		gui_->set_team(next_team - 1, show_everything_);

	::clear_shroud(*gui_, map_, units_, teams_,
		next_team - 1);
	gui_->set_playing_team(next_team - 1);
	gui_->invalidate_all();
}

void replay_controller::update_gui(){
	(*gui_).recalculate_minimap();
	(*gui_).redraw_minimap();
	(*gui_).invalidate_all();
	events::raise_draw_event();
	(*gui_).draw();
}

void replay_controller::preferences(){
	play_controller::preferences();
	init_replay_display();
	update_gui();
}

void replay_controller::show_statistics(){
	menu_handler_.show_statistics(gui_->playing_team()+1);
}

void replay_controller::handle_generic_event(const std::string& /*name*/){
	rebuild_replay_theme();
}

bool replay_controller::can_execute_command(hotkey::HOTKEY_COMMAND command, int index) const
{
	bool result = play_controller::can_execute_command(command,index);

	switch(command) {

	//commands we can always do
	case hotkey::HOTKEY_PLAY_REPLAY:
	case hotkey::HOTKEY_RESET_REPLAY:
	case hotkey::HOTKEY_STOP_REPLAY:
	case hotkey::HOTKEY_REPLAY_NEXT_TURN:
	case hotkey::HOTKEY_REPLAY_NEXT_SIDE:
	case hotkey::HOTKEY_REPLAY_SHOW_EVERYTHING:
	case hotkey::HOTKEY_REPLAY_SHOW_EACH:
	case hotkey::HOTKEY_REPLAY_SHOW_TEAM1:
	case hotkey::HOTKEY_REPLAY_SKIP_ANIMATION:
	case hotkey::HOTKEY_SAVE_GAME:
	case hotkey::HOTKEY_SAVE_REPLAY:
	case hotkey::HOTKEY_CHAT_LOG:
		return true;

	default:
		return result;
	}
}
