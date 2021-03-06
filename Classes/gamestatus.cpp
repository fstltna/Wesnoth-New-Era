/* $Id: gamestatus.cpp 33687 2009-03-15 16:50:42Z mordante $ */
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
 * @file gamestatus.cpp
 * Maintain status of a game, load&save games.
 */

#include "global.hpp"

#include "gettext.hpp"
#include "log.hpp"
#include "game_preferences.hpp"
#include "replay.hpp"
#include "statistics.hpp"
#include "unit_id.hpp"
#include "wesconfig.h"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"
#include "wml_exception.hpp"
#include "formula_string_utils.hpp"

#ifndef _MSC_VER
#include <sys/time.h>
#endif

#define DBG_NG lg::debug(lg::engine)
#define LOG_NG lg::info(lg::engine)
#define WRN_NG lg::warn(lg::engine)
#define ERR_NG lg::err(lg::engine)

#ifdef _WIN32
#include <windows.h>

/**
 * conv_ansi_utf8()
 *   - Convert a string between ANSI encoding (for Windows filename) and UTF-8
 *  string &name
 *     - filename to be converted
 *  bool a2u
 *     - if true, convert the string from ANSI to UTF-8.
 *     - if false, reverse. (convert it from UTF-8 to ANSI)
 */
void conv_ansi_utf8(std::string &name, bool a2u) {
	int wlen = MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0,
                                   name.c_str(), -1, NULL, 0);
	if (wlen == 0) return;
	WCHAR *wc = new WCHAR[wlen];
	if (wc == NULL) return;
	if (MultiByteToWideChar(a2u ? CP_ACP : CP_UTF8, 0, name.c_str(), -1,
                            wc, wlen) == 0) {
		delete wc;
		return;
	}
	int alen = WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
                                   NULL, 0, NULL, NULL);
	if (alen == 0) {
		delete wc;
		return;
	}
	CHAR *ac = new CHAR[alen];
	if (ac == NULL) {
		delete wc;
		return;
	}
	WideCharToMultiByte(!a2u ? CP_ACP : CP_UTF8, 0, wc, wlen,
                        ac, alen, NULL, NULL);
	delete wc;
	if (ac == NULL) {
		return;
	}
	name = ac;
	delete ac;

	return;
}

void replace_underbar2space(std::string &name) {
    LOG_NG << "conv(A2U)-from:[" << name << "]" << std::endl;
    conv_ansi_utf8(name, true);
    LOG_NG << "conv(A2U)-to:[" << name << "]" << std::endl;
    LOG_NG << "replace_underbar2space-from:[" << name << "]" << std::endl;
    std::replace(name.begin(), name.end(), '_', ' ');
    LOG_NG << "replace_underbar2space-to:[" << name << "]" << std::endl;
}

static void replace_space2underbar(std::string &name) {
    LOG_NG << "conv(U2A)-from:[" << name << "]" << std::endl;
    conv_ansi_utf8(name, false);
    LOG_NG << "conv(U2A)-to:[" << name << "]" << std::endl;
    LOG_NG << "replace_underbar2space-from:[" << name << "]" << std::endl;
    std::replace(name.begin(), name.end(), ' ', '_');
    LOG_NG << "replace_underbar2space-to:[" << name << "]" << std::endl;
}
#else /* ! _WIN32 */
void replace_underbar2space(shared_string &name) {
	std::string temp = name;
    std::replace(temp.begin(),temp.end(),'_',' ');
	name = temp;
}
void replace_underbar2space(std::string &name) {
    std::replace(name.begin(),name.end(),'_',' ');
}
static void replace_space2underbar(shared_string &name) {
	std::string temp = name;
    std::replace(temp.begin(),temp.end(),' ','_');
	name = temp;
}
static void replace_space2underbar(std::string &name) {
    std::replace(name.begin(),name.end(),' ','_');
}
#endif /* _WIN32 */

static void extract_summary_from_config(config& cfg_save, config& cfg_summary);
static void extract_summary_data_from_save(const game_state& gamestate, config& out);

player_info* game_state::get_player(const shared_string& id) {
	std::map< shared_string, player_info >::iterator found = players.find(id);
	if (found == players.end()) {
		LOG_STREAM(warn, engine) << "player " << id << " does not exist.\n";
		return NULL;
	} else
		return &found->second;
}

#ifdef __UNUSED__
std::string generate_game_uuid()
{
	struct timeval ts;
	std::stringstream uuid;
	gettimeofday(&ts, NULL);

	uuid << preferences::login() << "@" << ts.tv_sec << "." << ts.tv_usec;

	return uuid.str();
}
#endif

void gamestatus::add_time_area(const config& cfg)
{
	areas_.push_back(area_time_of_day());
	area_time_of_day &area = areas_.back();
	area.id   = cfg["id"];
	area.xsrc = cfg["x"];
	area.ysrc = cfg["y"];
	std::vector<map_location> const& locs = parse_location_range(area.xsrc, area.ysrc);
	std::copy(locs.begin(), locs.end(), std::inserter(area.hexes, area.hexes.end()));
	time_of_day::parse_times(cfg, area.times);
}

void gamestatus::add_time_area(const shared_string& id, const std::set<map_location>& locs,
                               const config& time_cfg)
{
	areas_.push_back(area_time_of_day());
	area_time_of_day& area = areas_.back();
	area.id = id;
	area.hexes = locs;
	time_of_day::parse_times(time_cfg, area.times);
}

void gamestatus::remove_time_area(const shared_string& area_id)
{
	if(area_id.empty()) {
		areas_.clear();
	} else {
		// search for all time areas that match the id.
		std::vector<area_time_of_day>::iterator i = areas_.begin();
		while(i != areas_.end()) {
			if((*i).id == area_id) {
				i = areas_.erase(i);
			} else {
				++i;
			}
		}
	}
}

gamestatus::gamestatus(const config& time_cfg, int num_turns, game_state* s_o_g) :
		teams(0),
		times_(),
		areas_(),
		turn_(1),
		numTurns_(num_turns),
		currentTime_(0),
		state_of_game_(s_o_g)
{
	shared_string turn_at = time_cfg["turn_at"];
	shared_string current_tod = time_cfg["current_tod"];
	shared_string random_start_time = time_cfg["random_start_time"];
	if (s_o_g)
	{
	    turn_at = utils::interpolate_variables_into_string(turn_at, *s_o_g);
		current_tod = utils::interpolate_variables_into_string(current_tod, *s_o_g);

	}

	if(turn_at.empty() == false) {
		turn_ = atoi(turn_at.c_str());
	}

	time_of_day::parse_times(time_cfg,times_);

	set_start_ToD(const_cast<config&>(time_cfg),s_o_g);

	const config::child_list& times_range = time_cfg.get_children("time_area");
	for(config::child_list::const_iterator t = times_range.begin(); t != times_range.end(); ++t)
		this->add_time_area(**t);
}

void gamestatus::write(config& cfg) const
{
	std::stringstream buf;
	buf << turn_;
	cfg["turn_at"] = buf.str();
	buf.str(std::string());
	buf << numTurns_;
	cfg["turns"] = buf.str();
	buf.str(std::string());
	buf << currentTime_;
	cfg["current_tod"] = buf.str();

	std::vector<time_of_day>::const_iterator t;
	for(t = times_.begin(); t != times_.end(); ++t) {
		t->write(cfg.add_child("time"));
	}


	for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
		config& area = cfg.add_child("time_area");
		area["x"] = i->xsrc;
		area["y"] = i->ysrc;
		for(t = i->times.begin(); t != i->times.end(); ++t) {
			t->write(area.add_child("time"));
		}

	}
}

time_of_day gamestatus::get_time_of_day_turn(int nturn) const
{
	VALIDATE(times_.size(), _("No time of day has been defined."));

	int time = (currentTime_ + nturn  - turn())% times_.size();

	if (time < 0)
	{
		time += times_.size();
	}

	return times_[time];
}

time_of_day gamestatus::get_time_of_day() const
{
	VALIDATE(times_.size(), _("No time of day has been defined."));

	return times_[currentTime_];
}

time_of_day gamestatus::get_previous_time_of_day() const
{
	return get_time_of_day_turn(turn()-1);
}

time_of_day gamestatus::get_time_of_day(int illuminated, const map_location& loc, int n_turn) const
{
	time_of_day res = get_time_of_day_turn(n_turn);

	if(loc.valid()) {
		for(std::vector<area_time_of_day>::const_iterator i = areas_.begin(); i != areas_.end(); ++i) {
			if(i->hexes.count(loc) == 1) {

				VALIDATE(i->times.size(), _("No time of day has been defined."));

				res = i->times[(n_turn-1)%i->times.size()];
				break;
			}
		}
	}

	if(illuminated) {
		res.bonus_modified=illuminated;
		res.lawful_bonus += illuminated;
	}
	return res;
}

time_of_day gamestatus::get_time_of_day(int illuminated, const map_location& loc) const
{
	return get_time_of_day(illuminated,loc,turn());
}

bool gamestatus::set_time_of_day(int newTime)
{
	// newTime can come from network so have to take run time test
	if( newTime >= static_cast<int>(times_.size())
	  || newTime < 0)
	{
		return false;
	}

	currentTime_ = newTime;

	return true;
}

bool gamestatus::is_start_ToD(const shared_string& random_start_time)
{
	return !random_start_time.empty()
			&& utils::string_bool(random_start_time, true);
}

void gamestatus::set_start_ToD(config &level, game_state* s_o_g)
{
	if (!level["current_tod"].empty())
	{
		set_time_of_day(atoi(level["current_tod"].c_str()));
		return;
	}
	shared_string random_start_time = level["random_start_time"];
	if (s_o_g)
	{
		random_start_time = utils::interpolate_variables_into_string(random_start_time, *s_o_g);
	}
	if (gamestatus::is_start_ToD(random_start_time))
	{
		std::vector<shared_string> start_strings =
			utils::split_shared(random_start_time, ',', utils::STRIP_SPACES | utils::REMOVE_EMPTY);

		if (utils::string_bool(random_start_time,false))
		{
			// We had boolean value
			set_time_of_day(rand()%times_.size());
		}
		else
		{
			set_time_of_day(atoi(start_strings[rand()%start_strings.size()].c_str()) - 1);
		}
	}
	else
	{
		// We have to set right ToD for oldsaves

		set_time_of_day((turn() - 1) % times_.size());
	}
	// Setting ToD to level data

	std::stringstream buf;
	buf << currentTime_;
	level["current_tod"] = buf.str();

}

void gamestatus::next_time_of_day()
{
	VALIDATE(times_.size(), _("No time of day has been defined."));

	currentTime_ = (currentTime_ + 1)%times_.size();
}

size_t gamestatus::turn() const
{
	return turn_;
}

int gamestatus::number_of_turns() const
{
	return numTurns_;
}
void gamestatus::modify_turns(const shared_string& mod)
{
	numTurns_ = std::max<int>(utils::apply_modifier(numTurns_,mod,0),-1);
}
void gamestatus::add_turns(int num)
{
	numTurns_ = std::max<int>(numTurns_ + num,-1);
}

void gamestatus::set_turn(unsigned int num)
{
	VALIDATE(times_.size(), _("No time of day has been defined."));
	const unsigned int old_num = turn_;
	// Correct ToD
	currentTime_ = (num  - 1) % times_.size();
	if (currentTime_ < 0) {
		currentTime_ += times_.size();
	}
	if(static_cast<int>(num) > numTurns_ && numTurns_ != -1) {
		this->add_turns(numTurns_ - num);
	}
	turn_ = num;

	LOG_NG << "changed current turn number from " << old_num << " to " << num << '\n';
}

bool gamestatus::next_turn()
{
	next_time_of_day();
	++turn_;
	return numTurns_ == -1 || turn_ <= size_t(numTurns_);
}

static player_info read_player(const config* cfg)
{
	player_info res;

	res.name = (*cfg)["name"];

	res.gold = atoi((*cfg)["gold"].c_str());
	res.gold_add = utils::string_bool((*cfg)["gold_add"]);

	const config::child_list& units = cfg->get_children("unit");
	for(config::child_list::const_iterator i = units.begin(); i != units.end(); ++i) {
		res.available_units.push_back(unit(**i,false));
	}

	res.can_recruit.clear();

	const std::string& can_recruit_str = (*cfg)["can_recruit"];
	if(can_recruit_str != "") {
		const std::vector<std::string> can_recruit = utils::split(can_recruit_str);
		std::copy(can_recruit.begin(),can_recruit.end(),std::inserter(res.can_recruit,res.can_recruit.end()));
	}

	return res;
}

game_state::game_state()  :
		label(),
		version(),
		campaign_type(),
		campaign_define(),
		campaign_xtra_defines(),
		campaign(),
		history(),
		abbrev(),
		scenario(),
		next_scenario(),
		completion(),
		end_text(),
		end_text_duration(),
		players(),
		scoped_variables(),
		wml_menu_items(),
		difficulty("NORMAL"),
		replay_data(),
		starting_pos(),
		snapshot(),
		last_selected(map_location::null_location),
		rng_(),
		variables(),
		temporaries(),
		generator_setter(&recorder)
		{}


game_state::game_state(const config& cfg, bool show_replay) :
		label(cfg["label"]),
		version(cfg["version"]),
		campaign_type(cfg["campaign_type"]),
		campaign_define(cfg["campaign_define"]),
		campaign_xtra_defines(utils::split_shared(cfg["campaign_extra_defines"])),
		campaign(cfg["campaign"]),
		history(cfg["history"]),
		abbrev(cfg["abbrev"]),
		scenario(cfg["scenario"]),
		next_scenario(cfg["next_scenario"]),
		completion(cfg["completion"]),
		end_text(cfg["end_text"]),
		end_text_duration(lexical_cast_default<unsigned int>(cfg["end_text_duration"])),
		players(),
		scoped_variables(),
		wml_menu_items(),
		difficulty(cfg["difficulty"]),
		replay_data(),
		starting_pos(),
		snapshot(),
		last_selected(map_location::null_location),
		rng_(cfg),
		variables(),
		temporaries(),
		generator_setter(&recorder)
{
	n_unit::id_manager::instance().set_save_id(lexical_cast_default<size_t>(cfg["next_underlying_unit_id"],0));
	log_scope("read_game");

	const config* snapshot = cfg.child("snapshot");
	const config* replay_start = cfg.child("replay_start");

	// We have to load era id for MP games so they can load correct era.


	if ((snapshot != NULL) && (!snapshot->empty()) && (!show_replay)) {

		this->snapshot = *snapshot;

		rng_.seed_random(lexical_cast_default<unsigned>((*snapshot)["random_calls"]));

		// Midgame saves have the recall list stored in the snapshot.
		load_recall_list(snapshot->get_children("player"));

	} else {
		// Start of scenario save, replays and MP campaign network next scenario
		// have the recall list stored in root of the config.
		// Edit:
		// Unfortunately, the replay savegame format is not homogeneous. Some savegames
		// have the player information stored in the starting position, others in the
		// root of the config. If the starting position player information is available
		// it will be preferred.
		if (replay_start != NULL){
			// Check if we find some player information in the starting position
			const config::child_list& cfg_players = (*replay_start).get_children("player");
			if (!cfg_players.empty())
				load_recall_list(cfg_players);
			else
				load_recall_list(cfg.get_children("player"));
		}
		else{
			load_recall_list(cfg.get_children("player"));
		}
	}

	LOG_NG << "scenario: '" << scenario << "'\n";
	LOG_NG << "next_scenario: '" << next_scenario << "'\n";

	if(difficulty.empty()) {
		difficulty = "NORMAL";
	}

	if(campaign_type.empty()) {
		campaign_type = "scenario";
	}

	const config* const vars = cfg.child("variables");
	if(vars != NULL) {
		set_variables(*vars);
	}
	set_menu_items(cfg.get_children("menu_item"));

	const config* const replay = cfg.child("replay");
	if(replay != NULL) {
		replay_data = *replay;
	}

	if(replay_start != NULL) {
		starting_pos = *replay_start;
		//This is a quick hack to make replays for campaigns work again:
		//The [player] information needs to be stored somewhere within the gamestate,
		//because we need it later on when creating the replay savegame.
		//We therefore put it inside the starting_pos, so it doesn't get lost.
		//See also playcampaign::play_game, where after finishing the scenario the replay
		//will be saved.
		if(!starting_pos.empty()) {
			config::child_list player_list = cfg.get_children("player");
			for (config::child_list::const_iterator p = player_list.begin(); p != player_list.end(); p++){
				config& cfg_player = starting_pos.add_child("player");
				cfg_player.merge_with(**p);
			}
		}
	}

	if(cfg.child("statistics")) {
		statistics::fresh_stats();
		statistics::read_stats(*cfg.child("statistics"));
	}
}

static void write_player(const player_info& player, config& cfg)
{
	cfg["name"] = player.name;

	char buf[50];
	snprintf(buf,sizeof(buf),"%d",player.gold);

	cfg["gold"] = buf;

	cfg["gold_add"] = player.gold_add ? "yes" : "no";

	for(std::vector<unit>::const_iterator i = player.available_units.begin();
	    i != player.available_units.end(); ++i) {
		config new_cfg;
		i->write(new_cfg);
		cfg.add_child("unit",new_cfg);
		DBG_NG << "added unit '" << new_cfg["id"] << "' to player '" << player.name << "'\n";
	}

	std::stringstream can_recruit;
	std::copy(player.can_recruit.begin(),player.can_recruit.end(),std::ostream_iterator<std::string>(can_recruit,","));
	std::string can_recruit_str = can_recruit.str();

	// Remove the trailing comma
	if(can_recruit_str.empty() == false) {
		can_recruit_str.resize(can_recruit_str.size()-1);
	}

	cfg["can_recruit"] = can_recruit_str;
}

void write_players(game_state& gamestate, config& cfg)
{
	for(std::map<shared_string, player_info>::const_iterator i=gamestate.players.begin();
		i!=gamestate.players.end(); ++i)
	{
		config new_cfg;
		write_player(i->second, new_cfg);
		new_cfg["save_id"]=i->first;
		cfg.add_child("player", new_cfg);
	}
}

static void write_player(config_writer &out, const player_info& player)
{
	out.write_key_val("name", player.name);

	char buf[50];
	snprintf(buf,sizeof(buf),"%d",player.gold);

	out.write_key_val("gold", buf);

	const std::string gold_add = player.gold_add ? "true" : "false";
	out.write_key_val("gold_add", gold_add);

	for(std::vector<unit>::const_iterator i = player.available_units.begin();
	    i != player.available_units.end(); ++i) {
		config new_cfg;
		i->write(new_cfg);
		out.write_child("unit",new_cfg);
		DBG_NG << "added unit '" << new_cfg["id"] << "' to player '" << player.name << "'\n";
	}

	std::stringstream can_recruit;
	std::copy(player.can_recruit.begin(),player.can_recruit.end(),std::ostream_iterator<std::string>(can_recruit,","));
	std::string can_recruit_str = can_recruit.str();

	// Remove the trailing comma
	if(can_recruit_str.empty() == false) {
		can_recruit_str.resize(can_recruit_str.size()-1);
	}

	out.write_key_val("can_recruit", can_recruit_str);
}

/** @deprecated, use other write_game below. */
void write_game(const game_state& gamestate, config& cfg, WRITE_GAME_MODE mode)
{
	log_scope("write_game");
	cfg["label"] = gamestate.label;
	cfg["history"] = gamestate.history;
	cfg["abbrev"] = gamestate.abbrev;
	cfg["version"] = game_config::version;

	cfg["scenario"] = gamestate.scenario;
	cfg["next_scenario"] = gamestate.next_scenario;

	cfg["completion"] = gamestate.completion;

	cfg["campaign"] = gamestate.campaign;

	cfg["campaign_type"] = gamestate.campaign_type;

	cfg["difficulty"] = gamestate.difficulty;

	cfg["campaign_define"] = gamestate.campaign_define;
	cfg["campaign_extra_defines"] = utils::join(gamestate.campaign_xtra_defines);
	cfg["next_underlying_unit_id"] = lexical_cast<std::string>(n_unit::id_manager::instance().get_save_id());

	cfg["random_seed"] = lexical_cast<std::string>(gamestate.rng().get_random_seed());
	cfg["random_calls"] = lexical_cast<std::string>(gamestate.rng().get_random_calls());

	cfg["end_text"] = gamestate.end_text;
	cfg["end_text_duration"] = str_cast<unsigned int>(gamestate.end_text_duration);

	cfg.add_child("variables",gamestate.get_variables());

	for(std::map<shared_string, wml_menu_item *>::const_iterator j=gamestate.wml_menu_items.begin();
	    j!=gamestate.wml_menu_items.end(); ++j) {
		config new_cfg;
		new_cfg["id"]=j->first;
		new_cfg["image"]=j->second->image;
		new_cfg["description"]=j->second->description;
		new_cfg["needs_select"]= (j->second->needs_select) ? "yes" : "no";
		if(!j->second->show_if.empty())
			new_cfg.add_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			new_cfg.add_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			new_cfg.add_child("command", j->second->command);
		cfg.add_child("menu_item", new_cfg);
	}

	for(std::map<shared_string, player_info>::const_iterator i=gamestate.players.begin();
	    i!=gamestate.players.end(); ++i) {
		config new_cfg;
		write_player(i->second, new_cfg);
		new_cfg["save_id"]=i->first;
		cfg.add_child("player", new_cfg);
	}

	if(mode == WRITE_FULL_GAME) {
		if(gamestate.replay_data.child("replay") == NULL) {
			cfg.add_child("replay",gamestate.replay_data);
		}

		cfg.add_child("snapshot",gamestate.snapshot);
		cfg.add_child("replay_start",gamestate.starting_pos);
		cfg.add_child("statistics",statistics::write_stats());
	}
}

void write_game(config_writer &out, const game_state& gamestate, WRITE_GAME_MODE mode)
{
	log_scope("write_game");

	out.write_key_val("label", gamestate.label);
	out.write_key_val("history", gamestate.history);
	out.write_key_val("abbrev", gamestate.abbrev);
	out.write_key_val("version", game_config::version);
	out.write_key_val("scenario", gamestate.scenario);
	out.write_key_val("next_scenario", gamestate.next_scenario);
	out.write_key_val("completion", gamestate.completion);
	out.write_key_val("campaign", gamestate.campaign);
	out.write_key_val("campaign_type", gamestate.campaign_type);
	out.write_key_val("difficulty", gamestate.difficulty);
	out.write_key_val("campaign_define", gamestate.campaign_define);
	out.write_key_val("campaign_extra_defines", utils::join(gamestate.campaign_xtra_defines));
	out.write_key_val("random_seed", lexical_cast<std::string>(gamestate.rng().get_random_seed()));
	out.write_key_val("random_calls", lexical_cast<std::string>(gamestate.rng().get_random_calls()));
	out.write_key_val("next_underlying_unit_id", lexical_cast<std::string>(n_unit::id_manager::instance().get_save_id()));
	out.write_key_val("end_text", gamestate.end_text);
	out.write_key_val("end_text_duration", str_cast<unsigned int>(gamestate.end_text_duration));
	out.write_child("variables", gamestate.get_variables());

	for(std::map<shared_string, wml_menu_item *>::const_iterator j=gamestate.wml_menu_items.begin();
	    j!=gamestate.wml_menu_items.end(); ++j) {
		out.open_child("menu_item");
		out.write_key_val("id", j->first);
		out.write_key_val("image", j->second->image);
		out.write_key_val("description", j->second->description);
		out.write_key_val("needs_select", (j->second->needs_select) ? "yes" : "no");
		if(!j->second->show_if.empty())
			out.write_child("show_if", j->second->show_if);
		if(!j->second->filter_location.empty())
			out.write_child("filter_location", j->second->filter_location);
		if(!j->second->command.empty())
			out.write_child("command", j->second->command);
		out.close_child("menu_item");
	}

	for(std::map<shared_string, player_info>::const_iterator i=gamestate.players.begin();
	    i!=gamestate.players.end(); ++i) {
		out.open_child("player");
		out.write_key_val("save_id", i->first);
		write_player(out, i->second);
		out.close_child("player");
	}

	if(mode == WRITE_FULL_GAME) {
		if(gamestate.replay_data.child("replay") == NULL) {
			out.write_child("replay", gamestate.replay_data);
		}

		out.write_child("snapshot",gamestate.snapshot);
		out.write_child("replay_start",gamestate.starting_pos);
		out.open_child("statistics");
		statistics::write_stats(out);
		out.close_child("statistics");
	}
}

/**
 * A structure for comparing to save_info objects based on their modified time.
 * If the times are equal, will order based on the name.
 */
struct save_info_less_time {
	bool operator()(const save_info& a, const save_info& b) const {
		if (a.time_modified > b.time_modified) {
		        return true;
		} else if (a.time_modified < b.time_modified) {
			return false;
		// Special funky case; for files created in the same second,
		// a replay file sorts less than a non-replay file.  Prevents
		// a timing-dependent bug where it may look like, at the end
		// of a scenario, the replay and the autosave for the next
		// scenario are displayed in the wrong order.
		} else if (a.name.find(_(" replay"))==std::string::npos && b.name.find(_(" replay"))!=std::string::npos) {
			return true;
		} else if (a.name.find(_(" replay"))!=std::string::npos && b.name.find(_(" replay"))==std::string::npos) {
			return false;
		} else {
			return  a.name.get() > b.name.get();
		}
	}
};

std::vector<save_info> get_saves_list(const shared_string *dir, const shared_string* filter)
{
	// Don't use a reference, it seems to break on arklinux with GCC-4.3.
	const std::string saves_dir = (dir) ? (*dir).get() : get_saves_dir();

	std::vector<shared_string> saves;
	get_files_in_dir(saves_dir,&saves);

	std::vector<save_info> res;
	for(std::vector<shared_string>::iterator i = saves.begin(); i != saves.end(); ++i) {
		if(filter && std::search(i->begin(), i->end(), filter->begin(), filter->end()) == i->end()) {
			continue;
		}

		const time_t modified = file_create_time(saves_dir + "/" + *i);

		std::string tempStr = *i;
		replace_underbar2space(tempStr);
		*i = tempStr;
		res.push_back(save_info(*i,modified));
	}

	std::sort(res.begin(),res.end(),save_info_less_time());

	return res;
}

bool save_game_exists(const shared_string& name)
{
	std::string fname = name;
	replace_space2underbar(fname);

	if(preferences::compress_saves()) {
		fname += ".gz";
	}

	return file_exists(get_saves_dir() + "/" + fname);
}

void delete_game(const shared_string& name)
{
	std::string modified_name = name;
	replace_space2underbar(modified_name);

	remove((get_saves_dir() + "/" + name).c_str());
	remove((get_saves_dir() + "/" + modified_name).c_str());
}

void read_save_file(const shared_string& name, config& cfg, std::string* error_log)
{
	shared_string modified_name = name;
	replace_space2underbar(modified_name);

	// Try reading the file both with and without underscores
	scoped_istream file_stream = istream_file(get_saves_dir() + "/" + modified_name);
	if (file_stream->fail())
		file_stream = istream_file(get_saves_dir() + "/" + name);

	cfg.clear();
	try{
		if(is_gzip_file(name)) {
			read_gz(cfg, *file_stream, error_log);
		} else {
			detect_format_and_read(cfg, *file_stream, error_log);
		}
	} catch (config::error &err)
	{
		ERR_NG << err.message;
		throw game::load_game_failed();
	}

	if(cfg.empty()) {
		ERR_NG << "Could not parse file data into config\n";
		throw game::load_game_failed();
	}
}

static void copy_era(config &cfg)
{
	if (cfg.child("replay_start")
		&& cfg.child("replay_start")->child("era")
		&& cfg.child("snapshot"))
	{
		config *snapshot = cfg.child("snapshot");
		snapshot->add_child("era",*cfg.child("replay_start")->child("era"));
	}
}

void load_game(const shared_string& name, game_state& gamestate, std::string* error_log)
{
	log_scope("load_game");

	config cfg;
	read_save_file(name,cfg,error_log);
	copy_era(cfg);

	gamestate = game_state(cfg);
}

void load_game_summary(const shared_string& name, config& cfg_summary, std::string* error_log){
	log_scope("load_game_summary");

	config cfg;
	read_save_file(name,cfg,error_log);

	extract_summary_from_config(cfg, cfg_summary);
}

// Throws game::save_game_failed
scoped_ostream open_save_game(const shared_string &label)
{
	shared_string name = label;
	replace_space2underbar(name);

	try {
		return scoped_ostream(ostream_file(get_saves_dir() + "/" + name));
	} catch(io_exception& e) {
		throw game::save_game_failed(e.what());
	}
}

void finish_save_game(config_writer &out, const game_state& gamestate, const shared_string &label)
{
	std::string name = label;
	std::replace(name.begin(),name.end(),' ','_');
	std::string fname(get_saves_dir() + "/" + name);

	try {
		if(!out.good()) {
			throw game::save_game_failed(_("Could not write to file"));
		}

		config& summary = save_summary(label);
		extract_summary_data_from_save(gamestate,summary);
		const int mod_time = static_cast<int>(file_create_time(fname));
		summary["mod_time"] = str_cast(mod_time);
		write_save_index();
	} catch(io_exception& e) {
		throw game::save_game_failed(e.what());
	}
}

// Throws game::save_game_failed
void save_game(const game_state& gamestate)
{
	std::string filename = gamestate.label;
	if(preferences::compress_saves()) {
		filename += ".gz";
	}

	scoped_ostream os(open_save_game(filename));
	std::stringstream ss;
	{
		config_writer out(ss, preferences::compress_saves());
		write_game(out, gamestate);
		finish_save_game(out, gamestate, gamestate.label);
	}
	(*os) << ss.str();
	if (!os->good()) {
		throw game::save_game_failed(_("Could not write to file"));
	}
}

namespace {
bool save_index_loaded = false;
config save_index_cfg;
}

static config& save_index()
{
	if(save_index_loaded == false) {
		try {
			scoped_istream stream = istream_file(get_save_index_file());
			detect_format_and_read(save_index_cfg, *stream);
		} catch(io_exception& e) {
			ERR_NG << "error reading save index: '" << e.what() << "'\n";
		} catch(config::error&) {
			ERR_NG << "error parsing save index config file\n";
			save_index_cfg.clear();
		}
		// KP: clear all "map_data" values, for next save
		save_index_cfg.recursive_clear_value("map_data");
		save_index_loaded = true;
	}

	return save_index_cfg;
}

config& save_summary(shared_string save)
{
	/*
	 * All saves are .gz files now so make sure we use that name when opening
	 * a file. If not some parts of the code use the name with and some parts
	 * without the .gz suffix.
	 */
	if(save.length() < 3 || save.substr(save.length() - 3) != ".gz") {
		save += ".gz";
	}

	config& cfg = save_index();
	config* res = cfg.find_child("save","save",save);
	if(res == NULL) {
		res = &cfg.add_child("save");
		(*res)["save"] = save;
	}

	return *res;
}

void write_save_index()
{
	log_scope("write_save_index()");
	try {
		scoped_ostream stream = ostream_file(get_save_index_file());
		write(*stream, save_index());
	} catch(io_exception& e) {
		ERR_NG << "error writing to save index file: '" << e.what() << "'\n";
	}
}

void extract_summary_data_from_save(const game_state& gamestate, config& out)
{
	const bool has_replay = gamestate.replay_data.empty() == false;
	const bool has_snapshot = gamestate.snapshot.child("side") != NULL;

	out["replay"] = has_replay ? "yes" : "no";
	out["snapshot"] = has_snapshot ? "yes" : "no";

	out["label"] = gamestate.label;
	out["campaign"] = gamestate.campaign;
	out["campaign_type"] = gamestate.campaign_type;
	out["scenario"] = gamestate.scenario;
	out["difficulty"] = gamestate.difficulty;
	out["version"] = gamestate.version;
	out["corrupt"] = "";

	if(has_snapshot) {
		out["turn"] = gamestate.snapshot["turn_at"];
		if(gamestate.snapshot["turns"] != "-1") {
			out["turn"] = out["turn"] + "/" + gamestate.snapshot["turns"];
		}
	}

	// Find the first human leader so we can display their icon in the load menu.

	/** @todo Ideally we should grab all leaders if there's more than 1 human player? */
	std::string leader;

	for(std::map<shared_string, player_info>::const_iterator p = gamestate.players.begin();
	    p!=gamestate.players.end(); ++p) {
		for(std::vector<unit>::const_iterator u = p->second.available_units.begin(); u != p->second.available_units.end(); ++u) {
			if(u->can_recruit()) {
				leader = u->type_id();
			}
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config& snapshot = has_snapshot ? gamestate.snapshot : gamestate.starting_pos;
		const config::child_list& sides = snapshot.get_children("side");
		for(config::child_list::const_iterator s = sides.begin(); s != sides.end() && leader.empty(); ++s) {

			if((**s)["controller"] != "human") {
				continue;
			}

			if(utils::string_bool((**s)["shroud"])) {
				shrouded = true;
			}

			const config::child_list& units = (**s).get_children("unit");
			for(config::child_list::const_iterator u = units.begin(); u != units.end(); ++u) {
				if(utils::string_bool( (**u)["canrecruit"], false) == true) {
					leader = (**u)["id"];
					break;
				}
			}
		}
	}

	out["leader"] = leader;
	out["map_data"] = "";
	// KP: do not save map data summary, because it takes too long to parse
/*
	if(!shrouded) {
		if(has_snapshot) {
			if(gamestate.snapshot.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = gamestate.snapshot["map_data"];
			}
		} else if(has_replay) {
			if(gamestate.starting_pos.find_child("side","shroud","yes") == NULL) {
				out["map_data"] = gamestate.starting_pos["map_data"];
			}
		}
	}
 */
}

void extract_summary_from_config(config& cfg_save, config& cfg_summary)
{
	const config* cfg_snapshot = cfg_save.child("snapshot");
	const config* cfg_replay_start = cfg_save.child("replay_start");

	const bool has_replay = (cfg_save.child("replay") != NULL && !cfg_save.child("replay")->empty());
	const bool has_snapshot = (cfg_snapshot != NULL) && (cfg_snapshot->child("side") != NULL);

	cfg_summary["replay"] = has_replay ? "yes" : "no";
	cfg_summary["snapshot"] = has_snapshot ? "yes" : "no";

	cfg_summary["label"] = cfg_save["label"];
	cfg_summary["campaign_type"] = cfg_save["campaign_type"];
	cfg_summary["scenario"] = cfg_save["scenario"];
	cfg_summary["campaign"] = cfg_save["campaign"];
	cfg_summary["difficulty"] = cfg_save["difficulty"];
	cfg_summary["version"] = cfg_save["version"];
	cfg_summary["corrupt"] = "";

	if(has_snapshot) {
		cfg_summary["turn"] = (*cfg_snapshot)["turn_at"];
		if((*cfg_snapshot)["turns"] != "-1") {
			cfg_summary["turn"] = cfg_summary["turn"] + "/" + (*cfg_snapshot)["turns"];
		}
	}

	// Find the first human leader so we can display their icon in the load menu.

	/** @todo Ideally we should grab all leaders if there's more than 1 human player? */
	std::string leader;

	const config::child_list& players = cfg_save.get_children("player");

	for(config::child_list::const_iterator i = players.begin(); i != players.end(); ++i) {
		if (utils::string_bool( (**i)["canrecruit"], false) == true){
			leader = (**i)["save_id"];
		}
	}

	bool shrouded = false;

	if(leader == "") {
		const config* snapshot = has_snapshot ? cfg_snapshot : cfg_replay_start;
		if (snapshot != NULL){
			const config::child_list& sides = snapshot->get_children("side");
			for(config::child_list::const_iterator s = sides.begin(); s != sides.end() && leader.empty(); ++s) {

				if((**s)["controller"] != "human") {
					continue;
				}

				if(utils::string_bool((**s)["shroud"])) {
					shrouded = true;
				}

				const config::child_list& units = (**s).get_children("unit");
				for(config::child_list::const_iterator u = units.begin(); u != units.end(); ++u) {
					if(utils::string_bool( (**u)["canrecruit"], false) == true) {
						leader = (**u)["id"];
						break;
					}
				}
			}
		}
	}

	cfg_summary["leader"] = leader;
	cfg_summary["map_data"] = "";

	// KP: do not save map data summary, because it takes too long to parse
	/*
	if(!shrouded) {
		if(has_snapshot) {
			if(cfg_snapshot->find_child("side","shroud","yes") == NULL) {
				cfg_summary["map_data"] = (*cfg_snapshot)["map_data"];
			}
		} else if(has_replay) {
			if(cfg_replay_start->find_child("side","shroud","yes") == NULL) {
				cfg_summary["map_data"] = (*cfg_replay_start)["map_data"];
			}
		}
	}
	 */
}

//t_string& game_state::get_variable(const std::string& key)
shared_string& game_state::get_variable(const shared_string& key)
{
	return variable_info(key, true, variable_info::TYPE_SCALAR).as_scalar();
}

//const t_string& game_state::get_variable_const(const std::string& key) const
const shared_string& game_state::get_variable_const(const shared_string& key) const
{
	variable_info to_get(key, false, variable_info::TYPE_SCALAR);
	if(!to_get.is_valid) return temporaries[key];
	return to_get.as_scalar();
}

config& game_state::get_variable_cfg(const shared_string& key)
{
	return variable_info(key, true, variable_info::TYPE_CONTAINER).as_container();
}

variable_info::array_range game_state::get_variable_cfgs(const shared_string& key)
{
	return variable_info(key, true, variable_info::TYPE_ARRAY).as_array();
}

void game_state::set_variable(const shared_string& key, const shared_string& value)
{
	get_variable(key) = value;
}

config& game_state::add_variable_cfg(const shared_string& key, const config& value)
{
	variable_info to_add(key, true, variable_info::TYPE_ARRAY);
	return to_add.vars->add_child(to_add.key, value);
}

void game_state::clear_variable_cfg(const shared_string& varname)
{
	variable_info to_clear(varname, false, variable_info::TYPE_CONTAINER);
	if(!to_clear.is_valid) return;
	if(to_clear.explicit_index) {
		to_clear.vars->remove_child(to_clear.key, to_clear.index);
	} else {
		to_clear.vars->clear_children(to_clear.key);
	}
}

void game_state::clear_variable(const shared_string& varname)
{
	variable_info to_clear(varname, false);
	if(!to_clear.is_valid) return;
	if(to_clear.explicit_index) {
		to_clear.vars->remove_child(to_clear.key, to_clear.index);
	} else {
		to_clear.vars->clear_children(to_clear.key);
		to_clear.vars->values.erase(to_clear.key);
	}
}

void game_state::load_recall_list(const config::child_list& players)
{
	if(!players.empty()) {
		for(config::child_list::const_iterator i = players.begin(); i != players.end(); ++i) {
			std::string save_id = (**i)["save_id"];

			if(save_id.empty()) {
				ERR_NG << "Corrupted player entry: NULL save_id" << std::endl;
			} else {
				player_info player = read_player(*i);
				this->players.insert(std::pair<std::string, player_info>(save_id,player));
			}
		}
	}
}

static void clear_wmi(std::map<shared_string, wml_menu_item*>& gs_wmi) {
	std::map<shared_string, wml_menu_item*>::iterator itor = gs_wmi.begin();
	for(itor = gs_wmi.begin(); itor != gs_wmi.end(); ++itor) {
		delete itor->second;
	}
	gs_wmi.clear();
}

game_state::game_state(const game_state& state) :
	/* default construct everything to silence compiler warnings. */
	variable_set(),
	label(),
	version(),
	campaign_type(),
	campaign_define(),
	campaign_xtra_defines(),
	campaign(),
	history(),
	abbrev(),
	scenario(),
	next_scenario(),
	completion(),
	end_text(),
	end_text_duration(),
	players(),
	scoped_variables(),
	wml_menu_items(),
	difficulty(),
	replay_data(),
	starting_pos(),
	snapshot(),
	last_selected(),
	rng_(),
	variables(),
	temporaries(),
	generator_setter(&recorder)
{
	*this = state;
}

game_state& game_state::operator=(const game_state& state)
{
	if(this == &state) {
		return *this;
	}

	history = state.history;
	abbrev = state.abbrev;
	label = state.label;
	version = state.version;
	campaign_type = state.campaign_type;
	campaign_define = state.campaign_define;
	campaign_xtra_defines = state.campaign_xtra_defines;
	campaign = state.campaign;
	scenario = state.scenario;
	completion = state.completion;
	end_text = state.end_text;
	end_text_duration = state.end_text_duration;
	rng_ = state.rng_;
	players = state.players;
	scoped_variables = state.scoped_variables;

	clear_wmi(wml_menu_items);
	std::map<shared_string, wml_menu_item*>::const_iterator itor;
	for (itor = state.wml_menu_items.begin(); itor != state.wml_menu_items.end(); ++itor) {
		wml_menu_item*& mref = wml_menu_items[itor->first];
		mref = new wml_menu_item(*(itor->second));
	}

	difficulty = state.difficulty;
	replay_data = state.replay_data;
	starting_pos = state.starting_pos;
	snapshot = state.snapshot;
	last_selected = state.last_selected;
	set_variables(state.get_variables());

	return *this;
}

game_state::~game_state() {
	clear_wmi(wml_menu_items);
}

void game_state::set_variables(const config& vars) {
	if(!variables.empty()) {
		WRN_NG << "clobbering the game_state variables\n";
		WRN_NG << variables;
	}
	variables = vars;
}

void game_state::set_menu_items(const config::child_list& menu_items) {
	clear_wmi(wml_menu_items);
	for(config::const_child_iterator i=menu_items.begin(); i != menu_items.end(); ++i) {
		//const std::string& id = (**i)["id"].base_str();
		const std::string& id = (**i)["id"];
		wml_menu_item*& mref = wml_menu_items[id];
		if(mref == NULL) {
			mref = new wml_menu_item(id, *i);
		} else {
			WRN_NG << "duplicate menu item (" << id << ") while loading gamestate\n";
		}
	}
}

void player_info::debug(){
	LOG_NG << "Debugging player\n";
	LOG_NG << "\tName: " << name << "\n";
	LOG_NG << "\tGold: " << gold << "\n";
	LOG_NG << "\tAvailable units:\n";
	for (std::vector<unit>::const_iterator u = available_units.begin(); u != available_units.end(); u++){
		LOG_NG << "\t\t" + u->name() + "\n";
	}
	LOG_NG << "\tEnd available units\n";
}

wml_menu_item::wml_menu_item(const shared_string& id, const config* cfg) :
		name(),
		image(),
		description(),
		needs_select(false),
		show_if(),
		filter_location(),
		command()

{
	std::stringstream temp;
	temp << "menu item";
	if(!id.empty()) {
		temp << ' ' << id;
	}
	name = temp.str();
	if(cfg != NULL) {
		image = (*cfg)["image"];
		description = (*cfg)["description"];
		needs_select = utils::string_bool((*cfg)["needs_select"], false);
		config const* temp;
		if((temp = (*cfg).child("show_if")) != NULL) show_if = *temp;
		if((temp = (*cfg).child("filter_location")) != NULL) filter_location = *temp;
		if((temp = (*cfg).child("command")) != NULL) command = *temp;
	}
}
