/* $Id: replay.cpp 33687 2009-03-15 16:50:42Z mordante $ */
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
 *  @file replay.cpp
 *  Replay control code.
 *
 *  See http://www.wesnoth.org/wiki/ReplayWML for more info.
 */

#include "global.hpp"

#include "actions.hpp"
#include "dialogs.hpp"
#include "game_end_exceptions.hpp"
#include "game_preferences.hpp"
#include "game_events.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "map_label.hpp"
#include "replay.hpp"
#include "statistics.hpp"
#include "unit_display.hpp"
#include "wesconfig.h"
#include "serialization/binary_or_text.hpp"

#define DBG_REPLAY LOG_STREAM(debug, replay)
#define LOG_REPLAY LOG_STREAM(info, replay)
#define WRN_REPLAY LOG_STREAM(warn, replay)
#define ERR_REPLAY LOG_STREAM(err, replay)

std::string replay::last_replay_error;

//functions to verify that the unit structure on both machines is identical

static void verify(const unit_map& units, const config& cfg) {
	std::stringstream errbuf;
	LOG_REPLAY << "verifying unit structure...\n";

	const size_t nunits = lexical_cast_default<size_t>(cfg["num_units"]);
	if(nunits != units.size()) {
		errbuf << "SYNC VERIFICATION FAILED: number of units from data source differ: "
			   << nunits << " according to data source. " << units.size() << " locally\n";

		std::set<map_location> locs;
		const config::child_list& items = cfg.get_children("unit");
		for(config::child_list::const_iterator i = items.begin(); i != items.end(); ++i) {
			const map_location loc(**i, game_events::get_state_of_game());
			locs.insert(loc);

			if(units.count(loc) == 0) {
				errbuf << "data source says there is a unit at "
					   << loc << " but none found locally\n";
			}
		}

		for(unit_map::const_iterator j = units.begin(); j != units.end(); ++j) {
			if(locs.count(j->first) == 0) {
				errbuf << "local unit at " << j->first
					   << " but none in data source\n";
			}
		}
		replay::throw_error(errbuf.str());
		errbuf.clear();
	}

	const config::child_list& items = cfg.get_children("unit");
	for(config::child_list::const_iterator i = items.begin(); i != items.end(); ++i) {
		const map_location loc(**i, game_events::get_state_of_game());
		const unit_map::const_iterator u = units.find(loc);
		if(u == units.end()) {
			errbuf << "SYNC VERIFICATION FAILED: data source says there is a '"
				   << (**i)["type"] << "' (side " << (**i)["side"] << ") at "
				   << loc << " but there is no local record of it\n";
			replay::throw_error(errbuf.str());
			errbuf.clear();
		}

		config cfg;
		u->second.write(cfg);

		bool is_ok = true;
		static const std::string fields[] = {"type","hitpoints","experience","side",""};
		for(const std::string* str = fields; str->empty() == false; ++str) {
			if(cfg[*str] != (**i)[*str]) {
				errbuf << "ERROR IN FIELD '" << *str << "' for unit at "
					   << loc << " data source: '" << (**i)[*str]
					   << "' local: '" << cfg[*str] << "'\n";
				is_ok = false;
			}
		}

		if(!is_ok) {
			errbuf << "(SYNC VERIFICATION FAILED)\n";
			replay::throw_error(errbuf.str());
			errbuf.clear();
		}
	}

	LOG_REPLAY << "verification passed\n";
}

namespace {
	const unit_map* unit_map_ref = NULL;
}

static void verify_units(const config& cfg)
	{
		if(unit_map_ref != NULL) {
			verify(*unit_map_ref,cfg);
		}
	}

verification_manager::verification_manager(const unit_map& units)
{
	unit_map_ref = &units;
}

verification_manager::~verification_manager()
{
	unit_map_ref = NULL;
}

// FIXME: this one now has to be assigned with set_random_generator
// from play_level or similar.  We should surely hunt direct
// references to it from this very file and move it out of here.
replay recorder;

replay::replay() :
	cfg_(),
	pos_(0),
	current_(NULL),
	saveInfo_(),
	skip_(false),
	message_locations()
{
}

replay::replay(const config& cfg) :
	cfg_(cfg),
	pos_(0),
	current_(NULL),
	saveInfo_(),
	skip_(false),
	message_locations()
{
}

void replay::throw_error(const std::string& msg)
{
	ERR_REPLAY << msg;
	last_replay_error = msg;
	if (!game_config::ignore_replay_errors) throw replay::error(msg);
}

void replay::set_save_info(const game_state& save)
{
	saveInfo_ = save;
}

void replay::set_save_info(const game_state& save, const config::child_list& players)
{
	saveInfo_ = save;
	saveInfo_.players.clear();
	saveInfo_.load_recall_list(players);
}

void replay::set_save_info_completion(const std::string &st)
// This function is a kluge to get around the fact that replay objects carry
// around a copy of gamestate rather than a reference to the global gamestate.
// That is probably a design bug that should be fixed.
{
	saveInfo_.completion = st;
}

void replay::set_skip(bool skip)
{
	skip_ = skip;
}

bool replay::is_skipping() const
{
	return skip_;
}

void replay::save_game(const std::string& label, const config& snapshot,
                       const config& starting_pos, bool include_replay)
{
	log_scope("replay::save_game");
	saveInfo_.snapshot = snapshot;
	saveInfo_.starting_pos = starting_pos;

	if(include_replay) {
		saveInfo_.replay_data = cfg_;
	} else {
		saveInfo_.replay_data = config();
	}

	saveInfo_.label = label;

	std::string filename = label;
	if(preferences::compress_saves()) {
		filename += ".gz";
	}

	std::stringstream ss;
	{
		config_writer out(ss, preferences::compress_saves());
		::write_game(out, saveInfo_);
		finish_save_game(out, saveInfo_, saveInfo_.label);
	}
	scoped_ostream os(open_save_game(filename));
	(*os) << ss.str();
	saveInfo_.replay_data = config();
	saveInfo_.snapshot = config();
	if (!os->good()) {
		throw game::save_game_failed(_("Could not write to file"));
	}
}

void replay::add_unit_checksum(const map_location& loc,config* const cfg)
{
	if(! game_config::mp_debug) {
		return;
	}
	assert(unit_map_ref);
	config& cc = cfg->add_child("checksum");
	loc.write(cc);
	unit_map::const_iterator u = unit_map_ref->find(loc);
	assert(u != unit_map_ref->end());
	cc["value"] = get_checksum(u->second);
}

void replay::add_start()
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command(true);
	cmd->add_child("start");
}

void replay::add_recruit(int value, const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();

	config val;

	char buf[100];
	snprintf(buf,sizeof(buf),"%d",value);
	val["value"] = buf;

	loc.write(val);

	cmd->add_child("recruit",val);
}

void replay::add_recall(int value, const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;

	config* const cmd = add_command();

	config val;

	char buf[100];
	snprintf(buf,sizeof(buf),"%d",value);
	val["value"] = buf;

	loc.write(val);

	cmd->add_child("recall",val);
}

void replay::add_disband(int value)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	
	config* const cmd = add_command();

	config val;

	char buf[100];
	snprintf(buf,sizeof(buf),"%d",value);
	val["value"] = buf;

	cmd->add_child("disband",val);
}

void replay::add_countdown_update(int value, int team)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();
	config val;

	val["value"] = lexical_cast_default<std::string>(value);
	val["team"] = lexical_cast_default<std::string>(team);

	cmd->add_child("countdown_update",val);
}


void replay::add_movement(const std::vector<map_location>& steps)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	if(steps.empty()) { // no move, nothing to record
		return;
	}

	config* const cmd = add_command();

	config move;
	write_locations(steps, move);

	cmd->add_child("move",move);
}

void replay::add_attack(const map_location& a, const map_location& b, int att_weapon, int def_weapon)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	add_pos("attack",a,b);
	char buf[100];
	snprintf(buf,sizeof(buf),"%d",att_weapon);
	current_->child("attack")->values["weapon"] = buf;
	snprintf(buf,sizeof(buf),"%d",def_weapon);
	current_->child("attack")->values["defender_weapon"] = buf;
	add_unit_checksum(a,current_);
	add_unit_checksum(b,current_);
}

void replay::add_pos(const std::string& type,
                     const map_location& a, const map_location& b)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();

	config move, src, dst;
	a.write(src);
	b.write(dst);

	move.add_child("source",src);
	move.add_child("destination",dst);
	cmd->add_child(type,move);
}

void replay::add_value(const std::string& type, int value)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();

	config val;

	char buf[100];
	snprintf(buf,sizeof(buf),"%d",value);
	val["value"] = buf;

	cmd->add_child(type,val);
}

void replay::choose_option(int index)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	add_value("choose",index);
}

void replay::text_input(std::string input)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();

	config val;
	val["text"] = input;

	cmd->add_child("input",val);
}

void replay::set_random_value(const std::string& choice)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();
	config val;
	val["value"] = choice;
	cmd->add_child("random_number",val);
}

void replay::add_label(const terrain_label* label)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	assert(label);
	config* const cmd = add_command(false);

	(*cmd)["undo"] = "no";

	config val;

	label->write(val);

	cmd->add_child("label",val);
}

void replay::clear_labels(const std::string& team_name)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command(false);

	(*cmd)["undo"] = "no";
	config val;
	val["team_name"] = team_name;
	cmd->add_child("clear_labels",val);
}

void replay::add_rename(const std::string& name, const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command(false);
	(*cmd)["async"] = "yes"; // Not undoable, but depends on moves/recruits that are
	config val;
	loc.write(val);
	val["name"] = name;
	cmd->add_child("rename", val);
}

void replay::end_turn()
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();
	cmd->add_child("end_turn");
}

void replay::add_event(const std::string& name, const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command();
	config& ev = cmd->add_child("fire_event");
	ev["raise"] = name;
	if(loc.valid()) {
		config& source = ev.add_child("source");
		loc.write(source);
	}
	(*cmd)["undo"] = "no";
}

void replay::add_checksum_check(const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	if(! game_config::mp_debug) {
		return;
	}
	config* const cmd = add_command();
	add_unit_checksum(loc,cmd);
}

void replay::add_advancement(const map_location& loc)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command(false);

	config val;
	(*cmd)["undo"] = "no";
	loc.write(val);
	cmd->add_child("advance_unit",val);
}

void replay::add_chat_message_location()
{
	message_locations.push_back(pos_-1);
}

void replay::speak(const config& cfg)
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config* const cmd = add_command(false);
	if(cmd != NULL) {
		cmd->add_child("speak",cfg);
		(*cmd)["undo"] = "no";
		add_chat_message_location();
	}
}

void replay::add_chat_log_entry(const config* speak, std::stringstream& str) const
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	if (!speak)
	{
		return;
	}
	const config& cfg = *speak;
	if (!preferences::show_lobby_join(cfg["id"], cfg["message"])) return;
	if (preferences::is_ignored(cfg["id"])) return;
	const std::string& team_name = cfg["team_name"];
	if(team_name == "") {
		str << "<" << cfg["id"] << "> ";
	} else {
		str << "*" << cfg["id"] << "* ";
	}
	str << cfg["message"] << "\n";
}

void replay::remove_command(int index)
{
	cfg_.remove_child("command", index);
	std::vector<int>::reverse_iterator loc_it;
	for (loc_it = message_locations.rbegin(); loc_it != message_locations.rend() && index < *loc_it;++loc_it)
	{
		--(*loc_it);
	}
}

// cached message log
std::stringstream message_log;


std::string replay::build_chat_log()
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return "";
	
	const config::child_list& cmd = commands();
	std::vector<int>::iterator loc_it;
	int last_location = 0;
	for (loc_it = message_locations.begin(); loc_it != message_locations.end(); ++loc_it)
	{
		last_location = *loc_it;
		const config* speak = cmd[last_location]->child("speak");
		add_chat_log_entry(speak, message_log);

	}
	message_locations.clear();

#if 0
	for(config::child_list::const_iterator i = cmd.begin() + (last_location + 1); i != cmd.end(); ++i) {
		++last_location;
		const config* speak = (**i).child("speak");
		if(speak != NULL) {
			message_locations.push_back(last_location);
			add_chat_log_entry(speak, str);
		}
	}
#endif
	return message_log.str();
}

config replay::get_data_range(int cmd_start, int cmd_end, DATA_TYPE data_type)
{
	config res;

	const config::child_list& cmd = commands();
	while(cmd_start < cmd_end) {
		if ((data_type == ALL_DATA || (*cmd[cmd_start])["undo"] == "no")
			&& (*cmd[cmd_start])["sent"] != "yes")
		{
			res.add_child("command",*cmd[cmd_start]);

			if(data_type == NON_UNDO_DATA) {
				(*cmd[cmd_start])["sent"] = "yes";
			}
		}

		++cmd_start;
	}

	return res;
}

void replay::undo()
{
	// KP: added flag to ignore replay data
	if(preferences::is_multiplayer() == false)
		return;
	
	config::child_itors cmd = cfg_.child_range("command");
	std::vector<config::child_iterator> async_cmds;
	// Remember commands not yet synced and skip over them.
	// We assume that all already sent (sent=yes) data isn't undoable
	// even if not marked explicitely with undo=no.

	/**
	 * @todo Change undo= to default to "no" and explicitely mark all
	 * undoable commands with yes.
	 */
	while(cmd.first != cmd.second && ((**(cmd.second-1))["undo"] == "no"
		|| (**(cmd.second-1))["async"] == "yes"
		|| (**(cmd.second-1))["sent"] == "yes"))
	{
		if ((**(cmd.second-1))["async"] == "yes")
			async_cmds.push_back(cmd.second-1);
		--cmd.second;
	}

	if(cmd.first != cmd.second) {
		config* child;
		config& cmd_second = (**(cmd.second-1));
		if ((child = cmd_second.child("move")) != NULL)
		{
			// A unit's move is being undone.
			// Repair unsynced cmds whose locations depend on that unit's location.
			const std::vector<map_location> steps =
					parse_location_range((*child)["x"],(*child)["y"]);

			if(steps.empty()) {
				ERR_REPLAY << "trying to undo a move using an empty path";
				return; // nothing to do, I suppose.
			}

			const map_location src = steps.front();
			const map_location dst = steps.back();

			for (std::vector<config::child_iterator>::iterator async_cmd =
				 async_cmds.begin(); async_cmd != async_cmds.end(); async_cmd++)
			{
				config* async_child;
				if ((async_child = (***async_cmd).child("rename")) != NULL)
				{
					map_location aloc(*async_child, game_events::get_state_of_game());
					if (dst == aloc)
					{
						src.write(*async_child);
					}
				}
			}
		}
		else if ((child = cmd_second.child("recruit")) != NULL
			|| (child = cmd_second.child("recall")) != NULL)
		{
			// A unit is being un-recruited or un-recalled.
			// Remove unsynced commands that would act on that unit.
			map_location src(*child, game_events::get_state_of_game());
			for (std::vector<config::child_iterator>::iterator async_cmd =
				 async_cmds.begin(); async_cmd != async_cmds.end(); async_cmd++)
			{
				config* async_child;
				if ((async_child = (***async_cmd).child("rename")) != NULL)
				{
					map_location aloc(*async_child, game_events::get_state_of_game());
					if (src == aloc)
					{
						remove_command(*async_cmd - cmd.first);
					}
				}
			}
		}
	}

	remove_command(cmd.second - cmd.first - 1);
	current_ = NULL;
	set_random(NULL);
}

const config::child_list& replay::commands() const
{
	return cfg_.get_children("command");
}

int replay::ncommands()
{
	return commands().size();
}

config* replay::add_command(bool update_random_context)
{
	pos_ = ncommands()+1;
	current_ = &cfg_.add_child("command");
	if(update_random_context)
		set_random(current_);

	return current_;
}

void replay::start_replay()
{
	pos_ = 0;
}

void replay::revert_action()
{
	if (pos_ > 0)
		--pos_;
}

config* replay::get_next_action()
{
	if(pos_ >= commands().size())
		return NULL;

	LOG_REPLAY << "up to replay action " << pos_ + 1 << "/" << commands().size() << "\n";

	current_ = commands()[pos_];
	set_random(current_);
	++pos_;
	return current_;
}

void replay::pre_replay()
{
	if ((rng::random() == NULL) && (commands().size() > 0)){
		if (at_end())
		{
			add_command(true);
		}
		else
		{
			set_random(commands()[pos_]);
		}
	}
}

bool replay::at_end() const
{
	return pos_ >= commands().size();
}

void replay::set_to_end()
{
	pos_ = commands().size();
	current_ = NULL;
	set_random(NULL);
}

void replay::clear()
{
	message_locations.clear();
	message_log.str(std::string());
	cfg_ = config();
	pos_ = 0;
	current_ = NULL;
	set_random(NULL);
	skip_ = 0;
}

bool replay::empty()
{
	return commands().empty();
}

void replay::add_config(const config& cfg, MARK_SENT mark)
{
	for(config::const_child_itors i = cfg.child_range("command"); i.first != i.second; ++i.first) {
		config& cfg = cfg_.add_child("command",**i.first);
		if (cfg.child("speak"))
		{
			pos_ = ncommands();
			add_chat_message_location();
		}
		if(mark == MARK_AS_SENT) {
			cfg["sent"] = "yes";
		}
	}
}

namespace {

replay* replay_src = NULL;

struct replay_source_manager
{
	replay_source_manager(replay* o) : old_(replay_src)
	{
		replay_src = o;
	}

	~replay_source_manager()
	{
		replay_src = old_;
	}

private:
	replay* const old_;
};

}

replay& get_replay_source()
{
	if(replay_src != NULL) {
		return *replay_src;
	} else {
		return recorder;
	}
}

static void check_checksums(game_display& disp,const unit_map& units,const config& cfg)
{
	if(! game_config::mp_debug) {
		return;
	}
	for(config::child_list::const_iterator ci = cfg.get_children("checksum").begin(); ci != cfg.get_children("checksum").end(); ++ci) {
		map_location loc(**ci, game_events::get_state_of_game());
		unit_map::const_iterator u = units.find(loc);
		if(u == units.end()) {
			std::stringstream message;
			message << "non existant unit to checksum at " << loc.x+1 << "," << loc.y+1 << "!";
			disp.add_chat_message(time(NULL), "verification", 1, message.str(),
					game_display::MESSAGE_PRIVATE, false);
			continue;
		}
		if(get_checksum(u->second) != (**ci)["value"]) {
			std::stringstream message;
			message << "checksum mismatch at " << loc.x+1 << "," << loc.y+1 << "!";
			disp.add_chat_message(time(NULL), "verification", 1, message.str(),
					game_display::MESSAGE_PRIVATE, false);
		}
	}
}

bool do_replay(game_display& disp, const gamemap& map,
	unit_map& units, std::vector<team>& teams, int team_num,
	const gamestatus& state, game_state& state_of_game, replay* obj)
{
	log_scope("do replay");

	const replay_source_manager replaymanager(obj);

//	replay& replayer = (obj != NULL) ? *obj : recorder;

	if (!get_replay_source().is_skipping()){
		disp.recalculate_minimap();
	}

	const rand_rng::set_random_generator generator_setter(&get_replay_source());

	update_locker lock_update(disp.video(),get_replay_source().is_skipping());
	return do_replay_handle(disp, map, units, teams, team_num, state, state_of_game,
						   std::string(""));
}

bool do_replay_handle(game_display& disp, const gamemap& map,
					  unit_map& units, std::vector<team>& teams, int team_num,
					  const gamestatus& state, game_state& state_of_game,
					  const std::string& do_untill)
{
	//a list of units that have promoted from the last attack
	std::deque<map_location> advancing_units;

	end_level_exception* delayed_exception = 0;

	team& current_team = teams[team_num-1];

	for(;;) {
		config* const cfg = get_replay_source().get_next_action();
		config* child;


		//do we need to recalculate shroud after this action is processed?

		bool fix_shroud = false;
		if (cfg)
		{
			DBG_REPLAY << "Replay data:\n" << *cfg << "\n";
		}
		else
		{
			DBG_REPLAY << "Repaly data at end\n";
		}


		//if we are expecting promotions here
		if(advancing_units.empty() == false) {
			if(cfg == NULL) {
				replay::throw_error("promotion expected, but none found\n");
			}

			//if there is a promotion, we process it and go onto the next command
			//but if this isn't a promotion, we just keep waiting for the promotion
			//command -- it may have been mixed up with other commands such as messages
			if((child = cfg->child("choose")) != NULL) {

				const int val = lexical_cast_default<int>((*child)["value"]);

				dialogs::animate_unit_advancement(units,advancing_units.front(),disp,val);

				advancing_units.pop_front();

				//if there are no more advancing units, then we check for victory,
				//in case the battle that led to advancement caused the end of scenario
				if(advancing_units.empty()) {
					check_victory(units, teams, disp);
				}

				continue;
			}
		}


		//if there is nothing more in the records
		if(cfg == NULL) {
			//replayer.set_skip(false);
			THROW_END_LEVEL_DELETE(delayed_exception);
			return false;
		}

		// We return if caller wants it for this tag
		if (!do_untill.empty()
			&& cfg->child(do_untill) != NULL)
		{
			get_replay_source().revert_action();
			THROW_END_LEVEL_DELETE(delayed_exception);
			return false;
		}

		//if there is an empty command tag, create by pre_replay() or a start tag
		else if ( (cfg->all_children().size() == 0) || (cfg->child("start") != NULL) ){
			//do nothing

		} else if((child = cfg->child("speak")) != NULL) {
			const std::string& team_name = (*child)["team_name"];
			const std::string& speaker_name = (*child)["id"];
			const std::string& message = (*child)["message"];
			//if (!preferences::show_lobby_join(speaker_name, message)) return;
			bool is_whisper = (speaker_name.find("whisper: ") == 0);
			get_replay_source().add_chat_message_location();
			if (!get_replay_source().is_skipping() || is_whisper) {
				const int side = lexical_cast_default<int>((*child)["side"],0);
				disp.add_chat_message(time(NULL), speaker_name, side, message,
						(team_name == "" ? game_display::MESSAGE_PUBLIC
						: game_display::MESSAGE_PRIVATE),
						preferences::message_bell());
			}
		} else if((child = cfg->child("label")) != NULL) {

			terrain_label label(disp.labels(),*child, game_events::get_state_of_game());

			disp.labels().set_label(label.location(),
						label.text(),
						label.team_name(),
						label.colour());

		} else if((child = cfg->child("clear_labels")) != NULL) {

			disp.labels().clear(std::string((*child)["team_name"]));
		}

		else if((child = cfg->child("rename")) != NULL) {
			const map_location loc(*child, game_events::get_state_of_game());
			const std::string& name = (*child)["name"];

			unit_map::iterator u = units.find(loc);
			if(u != units.end()) {
				if(u->second.unrenamable()) {
					std::stringstream errbuf;
					errbuf << "renaming unrenamable unit " << u->second.id() << "\n";
					replay::throw_error(errbuf.str());
				}
				u->second.rename(name);
			} else {
				// Users can rename units while it's being killed at another machine.
				// This since the player can rename units when it's not his/her turn.
				// There's not a simple way to prevent that so in that case ignore the
				// rename instead of throwing an OOS.
				WRN_REPLAY << "attempt to rename unit at location: "
				   << loc << ", where none exists (anymore).\n";
			}
		}

		//if there is an end turn directive
		else if(cfg->child("end_turn") != NULL) {
			child = cfg->child("verify");
			if(child != NULL) {
				verify_units(*child);
			}

			THROW_END_LEVEL_DELETE(delayed_exception);
			return true;
		}

		else if((child = cfg->child("recruit")) != NULL) {
			const std::string& recruit_num = (*child)["value"];
			const int val = lexical_cast_default<int>(recruit_num);

			map_location loc(*child, game_events::get_state_of_game());

			const std::set<shared_string>& recruits = current_team.recruits();

			if(val < 0 || static_cast<size_t>(val) >= recruits.size()) {
				std::stringstream errbuf;
				errbuf << "recruitment index is illegal: " << val
				       << " while this side only has " << recruits.size()
				       << " units available for recruitment\n";
				replay::throw_error(errbuf.str());
			}

			std::set<shared_string>::const_iterator itor = recruits.begin();
			std::advance(itor,val);
			const std::map<std::string,unit_type>::const_iterator u_type = unit_type_data::types().find_unit_type(*itor);
			if(u_type == unit_type_data::types().end()) {
				std::stringstream errbuf;
				errbuf << "recruiting illegal unit: '" << *itor << "'\n";
				replay::throw_error(errbuf.str());
			}

			unit new_unit(&units,&map,&state,&teams,&(u_type->second),team_num,true, false);
			const std::string& res = recruit_unit(map,team_num,units,new_unit,loc,false,!get_replay_source().is_skipping());
			if(!res.empty()) {
				std::stringstream errbuf;
				errbuf << "cannot recruit unit: " << res << "\n";
				replay::throw_error(errbuf.str());
			}

			if(u_type->second.cost() > current_team.gold()) {
				std::stringstream errbuf;
				errbuf << "unit '" << u_type->second.id() << "' is too expensive to recruit: "
				       << u_type->second.cost() << "/" << current_team.gold() << "\n";
				replay::throw_error(errbuf.str());
			}
			LOG_REPLAY << "recruit: team=" << team_num << " '" << u_type->second.id() << "' at (" << loc
			       << ") cost=" << u_type->second.cost() << " from gold=" << current_team.gold() << ' ';


			statistics::recruit_unit(new_unit);

			current_team.spend_gold(u_type->second.cost());
			LOG_REPLAY << "-> " << (current_team.gold()) << "\n";
			fix_shroud = !get_replay_source().is_skipping();
			check_checksums(disp,units,*cfg);
		}

		else if((child = cfg->child("recall")) != NULL) {
			player_info* player = state_of_game.get_player(current_team.save_id());
			if(player == NULL) {
				replay::throw_error("illegal recall\n");
			}

			sort_units(player->available_units);

			const std::string& recall_num = (*child)["value"];
			const int val = lexical_cast_default<int>(recall_num);

			map_location loc(*child, game_events::get_state_of_game());

			if(val >= 0 && val < int(player->available_units.size())) {
				statistics::recall_unit(player->available_units[val]);
				player->available_units[val].set_game_context(&units,&map,&state,&teams);
				recruit_unit(map,team_num,units,player->available_units[val],loc,true,!get_replay_source().is_skipping());
				player->available_units.erase(player->available_units.begin()+val);
				current_team.spend_gold(game_config::recall_cost);
			} else {
				replay::throw_error("illegal recall\n");
			}
			fix_shroud = !get_replay_source().is_skipping();
			check_checksums(disp,units,*cfg);
		}

		else if((child = cfg->child("disband")) != NULL) {
			player_info* const player = state_of_game.get_player(current_team.save_id());
			if(player == NULL) {
				replay::throw_error("illegal disband\n");
			}

			sort_units(player->available_units);
			const std::string& unit_num = (*child)["value"];
			const int val = lexical_cast_default<int>(unit_num);

			if(val >= 0 && val < int(player->available_units.size())) {
				player->available_units.erase(player->available_units.begin()+val);
			} else {
				replay::throw_error("illegal disband\n");
			}
		}
		else if((child = cfg->child("countdown_update")) != NULL) {
			const std::string& num = (*child)["value"];
			const int val = lexical_cast_default<int>(num);
			const std::string& tnum = (*child)["team"];
			const int tval = lexical_cast_default<int>(tnum,-1);
			if ( (tval<0)  || (static_cast<size_t>(tval) > teams.size()) ) {
				std::stringstream errbuf;
				errbuf << "Illegal countdown update \n"
					<< "Received update for :" << tval << " Current user :"
					<< team_num << "\n" << " Updated value :" << val;

				replay::throw_error(errbuf.str());
			} else {
				teams[tval-1].set_countdown_time(val);
			}
		}
		else if((child = cfg->child("move")) != NULL) {
			map_location src, dst;
			std::vector<map_location> steps;
			bool need_pathfinding = false;

			const std::string& x = (*child)["x"];
			const std::string& y = (*child)["y"];

			if(!x.empty() && !y.empty()) {
				// Normal [move] format, we just parse the path
				steps =	parse_location_range(x,y);

				if(steps.empty()) {
					replay::throw_error("incorrect path data found in [move]\n");
				}

				src = steps.front();
				dst = steps.back();
			}
			else {
				// This is 1.5.12-1.6RC1 [move] format, parse the old data
				const config* const destination = child->child("destination");
				const config* const source = child->child("source");

				if(source == NULL || destination == NULL) {
					replay::throw_error("no path or destination/source found in movement\n");
				}

				src = map_location(*source, game_events::get_state_of_game());
				dst = map_location(*destination, game_events::get_state_of_game());

				// 1.6RC1 needed pathfinding to generate the path data
				need_pathfinding = true;
			}


			if (src == dst) {
				WRN_REPLAY << "Warning: Move with identical source and destination. Skipping...";
				continue;
			}

			unit_map::iterator u = units.find(dst);
			if(u != units.end()) {
				std::stringstream errbuf;
				errbuf << "destination already occupied: "
				       << dst << '\n';
				replay::throw_error(errbuf.str());
			}
			u = units.find(src);
			if(u == units.end()) {
				std::stringstream errbuf;
				errbuf << "unfound location for source of movement: "
				       << src << " -> " << dst << '\n';
				replay::throw_error(errbuf.str());
			}

			// backwards compatibility code for 1.6RC1
			// NOTE: This will still OOS sometimes when ambushing AI
			// but if the RC1 replay worked before,
			// then it will continue to work with RC2
			if(need_pathfinding) {
				// search path assuming current_team as viewing team
				const shortest_path_calculator calc(u->second, current_team, units, teams, map);
				std::set<map_location> allowed_teleports;
				if(u->second.get_ability_bool("teleport",src)) {
					// search all known empty friendly villages
					for(std::set<map_location>::const_iterator i = current_team.villages().begin();
						i != current_team.villages().end(); ++i) {
						if (current_team.is_enemy(u->second.side()) && current_team.fogged(*i))
						continue;

						unit_map::const_iterator occupant = find_visible_unit(units, *i, map,teams, current_team);
						if (occupant != units.end() && occupant != u)
							continue;

						allowed_teleports.insert(*i);
					}
				}

				steps = a_star_search(src, dst, 10000.0, &calc, map.w(), map.h(), &allowed_teleports).steps;

				if (steps.empty()) {
					replay::throw_error("Pathfinding fails in the backwards compatibility code for 1.6RC1");
				}
			}

			::move_unit(&disp, map, units, teams, steps, NULL, NULL, NULL, true, true, true);

			//NOTE: The AI fire sighetd event whem moving in the FoV of team 1
			// (supposed to be the human player in SP)
			// That's ugly but let's try to make the replay works like that too
			if(team_num != 1 && teams.front().fog_or_shroud() && !teams.front().fogged(dst)
					 && (current_team.is_ai() || current_team.is_network_ai()))
			{
				// the second parameter is impossible to know
				// and the AI doesn't use it too in the local version
				game_events::fire("sighted",dst);
			}
		}

		else if((child = cfg->child("attack")) != NULL) {
			const config* const destination = child->child("destination");
			const config* const source = child->child("source");
			check_checksums(disp,units,*cfg);

			if(destination == NULL || source == NULL) {
				replay::throw_error("no destination/source found in attack\n");
			}

			//we must get locations by value instead of by references, because the iterators
			//may become invalidated later
			const map_location src(*source, game_events::get_state_of_game());
			const map_location dst(*destination, game_events::get_state_of_game());

			const std::string& weapon = (*child)["weapon"];
			const int weapon_num = lexical_cast_default<int>(weapon);

			const std::string& def_weapon = (*child)["defender_weapon"];
			int def_weapon_num = -1;
			if (def_weapon.empty()) {
				// Let's not gratuitously destroy backwards compat.
				ERR_REPLAY << "Old data, having to guess weapon\n";
			} else {
				def_weapon_num = lexical_cast_default<int>(def_weapon);
			}

			unit_map::iterator u = units.find(src);
			if(u == units.end()) {
				replay::throw_error("unfound location for source of attack\n");
			}

			if(size_t(weapon_num) >= u->second.attacks().size()) {
				replay::throw_error("illegal weapon type in attack\n");
			}

			unit_map::const_iterator tgt = units.find(dst);

			if(tgt == units.end()) {
				std::stringstream errbuf;
				errbuf << "unfound defender for attack: " << src << " -> " << dst << '\n';
				replay::throw_error(errbuf.str());
			}

			if(def_weapon_num >=
					static_cast<int>(tgt->second.attacks().size())) {

				replay::throw_error("illegal defender weapon type in attack\n");
			}

			DBG_REPLAY << "Attacker XP (before attack): " << u->second.experience() << "\n";;

			DELAY_END_LEVEL(delayed_exception, attack(disp, map, teams, src, dst, weapon_num, def_weapon_num, units, state, !get_replay_source().is_skipping()));

			DBG_REPLAY << "Attacker XP (after attack): " << u->second.experience() << "\n";;

			u = units.find(src);
			tgt = units.find(dst);

			if(u != units.end() && u->second.advances()) {
				advancing_units.push_back(u->first);
			}

			DBG_REPLAY << "advancing_units.size: " << advancing_units.size() << "\n";
			if(tgt != units.end() && tgt->second.advances()) {
				advancing_units.push_back(tgt->first);
			}

			//check victory now if we don't have any advancements. If we do have advancements,
			//we don't check until the advancements are processed.
			if(advancing_units.empty()) {
				check_victory(units, teams, disp);
			}
			fix_shroud = !get_replay_source().is_skipping();
		} else if((child = cfg->child("fire_event")) != NULL) {
			for(config::child_list::const_iterator v = child->get_children("set_variable").begin(); v != child->get_children("set_variable").end(); ++v) {
				state_of_game.set_variable((**v)["name"],(**v)["value"]);
			}
			const std::string event = (*child)["raise"];
			//exclude these events here, because in a replay proper time of execution can't be
			//established and therefore we fire those events inside play_controller::init_side
			if ((event != "side turn") && (event != "turn 1") && (event != "new turn") && (event != "turn refresh")){
				const config* const source = child->child("source");
				if(source != NULL) {
					game_events::fire(event, map_location(*source, game_events::get_state_of_game()));
				} else {
					game_events::fire(event);
				}
			}

		} else if((child = cfg->child("advance_unit")) != NULL) {
			const map_location loc(*child, game_events::get_state_of_game());
			advancing_units.push_back(loc);

		} else {
			if(! cfg->child("checksum")) {
				replay::throw_error("unrecognized action:\n" + cfg->debug());
			} else {
				check_checksums(disp,units,*cfg);
			}
		}

		//Check if we should refresh the shroud, and redraw the minimap/map tiles.
		//This is needed for shared vision to work properly.
		if(fix_shroud && clear_shroud(disp,map,units,teams,team_num-1) && !recorder.is_skipping()) {
			disp.recalculate_minimap();
			disp.invalidate_game_status();
			disp.invalidate_all();
			disp.draw();
		}

		child = cfg->child("verify");
		if(child != NULL) {
			verify_units(*child);
		}
	}

	return false; /* Never attained, but silent a gcc warning. --Zas */
}

replay_network_sender::replay_network_sender(replay& obj) : obj_(obj), upto_(obj_.ncommands())
{
}

replay_network_sender::~replay_network_sender()
{
	commit_and_sync();
}

void replay_network_sender::sync_non_undoable()
{
	if(network::nconnections() > 0) {
		config cfg;
		const config& data = cfg.add_child("turn",obj_.get_data_range(upto_,obj_.ncommands(),replay::NON_UNDO_DATA));
		if(data.empty() == false) {
			network::send_data(cfg, 0, true);
		}
	}
}

void replay_network_sender::commit_and_sync()
{
	if(network::nconnections() > 0) {
		config cfg;
		const config& data = cfg.add_child("turn",obj_.get_data_range(upto_,obj_.ncommands()));
		if(data.empty() == false) {
			network::send_data(cfg, 0, true);
		}

		upto_ = obj_.ncommands();
	}
}
