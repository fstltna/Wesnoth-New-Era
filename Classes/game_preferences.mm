/* $Id: game_preferences.cpp 33728 2009-03-16 18:09:00Z mordante $ */
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

#include "global.hpp"

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "foreach.hpp"
#include "game_preferences.hpp"
#include "gamestatus.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "map.hpp"
#include "network.hpp" // ping_timeout
#include "settings.hpp"
#include "wml_exception.hpp"

#import <UIKit/UIKit.h>

namespace {

bool message_private_on = false;

bool haloes = true;

std::set<shared_string> encountered_units_set;
std::set<t_translation::t_terrain> encountered_terrains_set;
std::set<std::string> achievements_set;
int player_side;
int turn_kills;
int total_kills;

std::map<std::string, std::vector<std::string> > history_map;
const unsigned max_history_saved = 50;

std::set<std::string> friends;
std::set<std::string> ignores;
bool friends_initialized = false;
bool ignores_initialized = false;

bool authenticated = false;
	
bool isMultiplayer = false;
} // anon namespace

namespace preferences {

manager::manager() :
	base()
{
	set_music_volume(music_volume());
	set_sound_volume(sound_volume());

	set_show_haloes(utils::string_bool(preferences::get("show_haloes"), true));
	if(!utils::string_bool(preferences::get("remember_timer_settings"), false)) {
		preferences::erase("mp_countdown_init_time");
		preferences::erase("mp_countdown_reservoir_time");
		preferences::erase("mp_countdown_turn_bonus");
		preferences::erase("mp_countdown_action_bonus");
	}

	const std::vector<std::string> v = utils::split(preferences::get("encountered_units"));
	std::copy(v.begin(), v.end(),
			std::inserter(encountered_units_set, encountered_units_set.begin()));
	
	const std::vector<std::string> a = utils::split(preferences::get("achievements"));
	std::copy(a.begin(), a.end(),
			  std::inserter(achievements_set, achievements_set.begin()));
	total_kills = lexical_cast_default<unsigned int>(preferences::get("kills"), 0);
	turn_kills = 0;
	player_side = 0;

	const t_translation::t_list terrain =
			t_translation::read_list(preferences::get("encountered_terrain_list"));
	std::copy(terrain.begin(), terrain.end(),
			std::inserter(encountered_terrains_set, encountered_terrains_set.begin()));

	config* history = preferences::get_child("history");
	if (history) {
/* Structure of the history
	[history]
		[history_id]
			[line]
				message = foobar
			[/line]
*/
		const config::child_map& history_id_list = history->all_children();
		typedef std::pair<std::string, config::child_list> hack;
		//typedef std::pair<shared_string, config::child_list> hack;
		foreach(const hack& history_id, history_id_list) {

			std::vector<std::string> current_history;
			foreach(const config* history_id_child, history_id.second) {

				const config::child_list& line = history_id_child->get_children("line");
				foreach(const config* line_data, line) {

					current_history.push_back((*line_data)["message"]);
				}
			}

			history_map[history_id.first] = current_history;
		}
	}

	network::ping_timeout = get_ping_timeout();
}

manager::~manager()
{
	std::vector<std::string> v;
	std::copy(encountered_units_set.begin(), encountered_units_set.end(), std::back_inserter(v));
	preferences::set("encountered_units", utils::join(v));
	
	// KP: added for saving achievements
	std::vector<std::string> a;
	std::copy(achievements_set.begin(), achievements_set.end(), std::back_inserter(a));
	preferences::set("achievements", utils::join(a));
	preferences::set("kills", lexical_cast<std::string>(total_kills));
	
	t_translation::t_list terrain;
	std::copy(encountered_terrains_set.begin(), encountered_terrains_set.end(),
			  std::back_inserter(terrain));
	preferences::set("encountered_terrain_list", t_translation::write_list(terrain));

/* Structure of the history
	[history]
		[history_id]
			[line]
				message = foobar
			[/line]
*/
	config history;
	typedef std::pair<std::string, std::vector<std::string> > hack;
	foreach(const hack& history_id, history_map) {

		config history_id_cfg; // [history_id]
		foreach(const std::string& line, history_id.second) {
			config cfg; // [line]

			cfg["message"] = line;
			history_id_cfg.add_child("line", cfg);
		}

		history.add_child(history_id.first, history_id_cfg);
	}
	preferences::set_child("history", history);

	history_map.clear();
	encountered_units_set.clear();
	achievements_set.clear();
	total_kills = 0;
	turn_kills = 0;
	encountered_terrains_set.clear();
	set_ping_timeout(network::ping_timeout);
}

bool is_authenticated() {
	return authenticated;
}

void admin_authentication(const std::string& sender, const std::string& message) {
	if(sender != "server") return;
	if(message.find("You are now recognized as an administrator.") == 0) {
		authenticated = true;
	} else if(message.find("You are no longer recognized as an administrator.") == 0) {
		authenticated = false;
	}
}

static void initialize_friends() {
	if(!friends_initialized) {
		std::vector<std::string> names = utils::split(preferences::get("friends"));
		std::set<std::string> tmp(names.begin(), names.end());
		friends.swap(tmp);

		friends_initialized = true;
	}
}

const std::set<std::string> & get_friends() {
	initialize_friends();
	return friends;
}

static void initialize_ignores() {
	if(!ignores_initialized) {
		std::vector<std::string> names = utils::split(preferences::get("ignores"));
		std::set<std::string> tmp(names.begin(), names.end());
		ignores.swap(tmp);

		ignores_initialized = true;
	}
}

const std::set<std::string> & get_ignores() {
	return ignores;
}

bool add_friend(const std::string& nick) {
	if (!utils::isvalid_wildcard(nick)) return false;
	friends.insert(nick);
	preferences::set("friends", utils::join(friends));
	return true;
}

bool add_ignore(const std::string& nick) {
	if (!utils::isvalid_wildcard(nick)) return false;
	ignores.insert(nick);
	preferences::set("ignores", utils::join(ignores));
	return true;
}

void remove_friend(const std::string& nick) {
	std::set<std::string>::iterator i = friends.find(nick);
	if(i != friends.end()) {
		friends.erase(i);
		preferences::set("friends", utils::join(friends));
	}
}

void remove_ignore(const std::string& nick) {
	std::set<std::string>::iterator i = ignores.find(nick);
	if(i != ignores.end()) {
		ignores.erase(i);
		preferences::set("ignores", utils::join(ignores));
	}
}

void clear_friends() {
	preferences::set("friends", "");
}

void clear_ignores() {
	preferences::set("ignores", "");
}

bool is_friend(const std::string& nick) {
	initialize_friends();
	return friends.find(nick) != friends.end();
}

bool is_ignored(const std::string& nick) {
	initialize_ignores();
	return ignores.find(nick) != ignores.end();
}

bool show_lobby_join(const std::string& sender, const std::string& message) {
	// If it's actually not a lobby join message return true (show it).
	if (sender != "server" || message.find("has logged into the lobby") == std::string::npos) return true;
	if (lobby_joins() == SHOW_NONE) return false;
	if (lobby_joins() == SHOW_ALL) return true;
	const std::string::const_iterator i =
			std::find(message.begin(), message.end(), ' ');
	const std::string joiner(message.begin(), i);
	if (lobby_joins() == SHOW_FRIENDS && is_friend(joiner)) return true;
	return false;
}

int lobby_joins()
{
    if(preferences::get("lobby_joins") == "friends") {
		return SHOW_FRIENDS;
	} else if(preferences::get("lobby_joins") == "all") {
		return SHOW_ALL;
	} else if(preferences::get("lobby_joins") == "none") {
		return SHOW_NONE;
	} else {
		return SHOW_FRIENDS;
	}
}


void _set_lobby_joins(int show)
{
	if (show == SHOW_FRIENDS) {
		preferences::set("lobby_joins", "friends");
	} else if (show == SHOW_ALL) {
		preferences::set("lobby_joins", "all");
	} else if (show == SHOW_NONE) {
		preferences::set("lobby_joins", "none");
	}
}

bool sort_list()
{
	return utils::string_bool(preferences::get("sort_list"), true);
}


void _set_sort_list(bool sort)
{
	preferences::set("sort_list", sort ? "yes" : "no");
}

bool iconize_list()
{
	return utils::string_bool(preferences::get("iconize_list"), true);
}

void _set_iconize_list(bool sort)
{
	preferences::set("iconize_list", sort ? "yes" : "no");
}

const std::vector<game_config::server_info>& server_list()
{
	static std::vector<game_config::server_info> pref_servers;
	if(pref_servers.empty()) {
		std::vector<game_config::server_info> &game_servers = game_config::server_list;
		VALIDATE(game_servers.size() > 0, _("No server has been defined."));
		pref_servers.insert(pref_servers.begin(), game_servers.begin(), game_servers.end());
		const std::vector<config *> &user_servers = get_prefs()->get_children("server");
		std::vector<config *>::const_iterator server;
		for(server = user_servers.begin(); server != user_servers.end(); ++server) {
			game_config::server_info sinf;
			sinf.name = (**server)["name"];
			sinf.address = (**server)["address"];
			pref_servers.push_back(sinf);
		}
	}
	return pref_servers;
}

std::string network_host()
{
	const std::string res = preferences::get("host");
	if(res.empty()) {
		return server_list().front().address;
	} else {
		return res;
	}
}

void set_network_host(const std::string& host)
{
	preferences::set("host", host);
}

unsigned int get_ping_timeout()
{
	return lexical_cast_default<unsigned>(preferences::get("ping_timeout"), 0);
}

void set_ping_timeout(unsigned int timeout)
{
	network::ping_timeout = timeout;
	preferences::set("ping_timeout", lexical_cast<std::string>(timeout));
}

std::string campaign_server()
{
	if(!preferences::get("campaign_server").empty()) {
		return preferences::get("campaign_server");
	} else {
		return "add-ons.wesnoth.org";
	}
}

void set_campaign_server(const std::string& host)
{
	preferences::set("campaign_server", host);
}

std::string login()
{
	const std::string res = preferences::get("login");
	if(res.empty()) {
/*		
		NSString *nsName = [[UIDevice currentDevice] name];
		if (!nsName)
			return ("Player");
		std::string res2;
		const char *cName = [nsName UTF8String];
		if (cName == NULL)
			return ("Player");
		res2 = cName;

		if(res2.empty()) {
			return _("Player");
		}
		return res2;
 */
		return ("Player");
	}

	return res;
}

void set_login(const std::string& username)
{
	preferences::set("login", username);
}

namespace prv {
	std::string password;
}

std::string password()
{
	if(remember_password()) {
		return preferences::get("password");
	} else {
		return prv::password;
	}
}

void set_password(const std::string& password)
{
	prv::password = password;
	if(remember_password()) {
		preferences::set("password", password);
	}
}

std::string sync_login_str()
{
	const std::string res = preferences::get("sync_login");
	if(res.empty()) 
	{
		return ("");
	}
	
	return res;
}
	
void set_sync_login(const std::string& username)
{
	preferences::set("sync_login", username);
}
	
std::string sync_password()
{
	const std::string res = preferences::get("sync_password");
	if(res.empty()) 
	{
		return ("");
	}
	
	return res;
}

void set_sync_password(const std::string& password)
{
	preferences::set("sync_password", password);
}


bool remember_password()
{
	return utils::string_bool(preferences::get("remember_password"), false);
}

void set_remember_password(bool remember)
{
	preferences::set("remember_password", remember ? "yes" : "no");
	preferences::set("password", remember ? prv::password : "");
}

bool turn_dialog()
{
	return utils::string_bool(preferences::get("turn_dialog"), false);
}

void set_turn_dialog(bool ison)
{
	preferences::set("turn_dialog", ison ? "yes" : "no");
}

bool show_combat()
{
	return utils::string_bool(preferences::get("show_combat"), true);
}

bool allow_observers()
{
	return utils::string_bool(preferences::get("allow_observers"), true);
}

void set_allow_observers(bool value)
{
	preferences::set("allow_observers", value ? "yes" : "no");
}

bool use_map_settings()
{
	return utils::string_bool(preferences::get("mp_use_map_settings"), true);
}

void set_use_map_settings(bool value)
{
	preferences::set("mp_use_map_settings", value ? "yes" : "no");
}

int mp_server_warning_disabled()
{
	return lexical_cast_default<int>(preferences::get("mp_server_warning_disabled"), 0);
}

void set_mp_server_warning_disabled(int value)
{
	preferences::set("mp_server_warning_disabled", lexical_cast<std::string>(value));
}

void set_mp_server_program_name(const std::string& path)
{
	if (path.empty())
	{
		preferences::clear("mp_server_program_name");
	}
	else
	{
		preferences::set("mp_server_program_name", path);
	}
}

std::string get_mp_server_program_name()
{
	return preferences::get("mp_server_program_name");
}

bool random_start_time()
{
	return settings::use_random_start_time(preferences::get("mp_random_start_time"));
}

void set_random_start_time(bool value)
{
	preferences::set("mp_random_start_time", value ? "yes" : "no");
}

bool fog()
{
	return settings::use_fog(preferences::get("mp_fog"));
}

void set_fog(bool value)
{
	preferences::set("mp_fog", value ? "yes" : "no");
}

bool shroud()
{
	return settings::use_shroud(preferences::get("mp_shroud"));
}

void set_shroud(bool value)
{
	preferences::set("mp_shroud", value ? "yes" : "no");
}

int turns()
{
	return settings::get_turns(preferences::get("mp_turns"));
}

void set_turns(int value)
{
	preferences::set("mp_turns", lexical_cast<std::string>(value));
}

bool skip_mp_replay()
{
	return utils::string_bool(preferences::get("skip_mp_replay"), false);
}

void set_skip_mp_replay(bool value)
{
	preferences::set("skip_mp_replay", value ? "yes" : "no");
}

bool countdown()
{
	return utils::string_bool(preferences::get("mp_countdown"), false);
}

void set_countdown(bool value)
{
	preferences::set("mp_countdown", value ? "yes" : "no");
}

int countdown_init_time()
{
	return lexical_cast_in_range<int>
		(preferences::get("mp_countdown_init_time"), 270, 0, 1500);
}

void set_countdown_init_time(int value)
{
	preferences::set("mp_countdown_init_time", lexical_cast<std::string>(value));
}

int countdown_reservoir_time()
{
	return lexical_cast_in_range<int>(
		preferences::get("mp_countdown_reservoir_time"), 330, 30, 1500);
}

void set_countdown_reservoir_time(int value)
{
	preferences::set("mp_countdown_reservoir_time", lexical_cast<std::string>(value));
}

int countdown_turn_bonus()
{
	return lexical_cast_in_range<int>(
		preferences::get("mp_countdown_turn_bonus"), 60, 0, 300);
}

void set_countdown_turn_bonus(int value)
{
	preferences::set("mp_countdown_turn_bonus", lexical_cast<std::string>(value));
}

int countdown_action_bonus()
{
	return lexical_cast_in_range<int>(
		preferences::get("mp_countdown_action_bonus"), 13, 0, 30);
}

void set_countdown_action_bonus(int value)
{
	preferences::set("mp_countdown_action_bonus", lexical_cast<std::string>(value));
}

int village_gold()
{
	return settings::get_village_gold(preferences::get("mp_village_gold"));
}

void set_village_gold(int value)
{
	preferences::set("mp_village_gold", lexical_cast<std::string>(value));
}

int xp_modifier()
{
	return settings::get_xp_modifier(preferences::get("mp_xp_modifier"));
}

void set_xp_modifier(int value)
{
	preferences::set("mp_xp_modifier", lexical_cast<std::string>(value));
}

int era()
{
	return lexical_cast_default<int>(preferences::get("mp_era"), 0);
}

void set_era(int value)
{
	preferences::set("mp_era", lexical_cast<std::string>(value));
}

int map()
{
	return lexical_cast_default<int>(preferences::get("mp_map"), 0);
}

void set_map(int value)
{
	preferences::set("mp_map", lexical_cast<std::string>(value));
}

bool show_ai_moves()
{
	return utils::string_bool(preferences::get("show_ai_moves"), true);
}

void set_show_ai_moves(bool value)
{
	preferences::set("show_ai_moves", value ? "yes" : "no");
}

void set_show_side_colours(bool value)
{
	preferences::set("show_side_colours", value ? "yes" : "no");
}

bool show_side_colours()
{
	return utils::string_bool(preferences::get("show_side_colours"), true);
}

void set_save_replays(bool value)
{
	preferences::set("save_replays", value ? "yes" : "no");
}

bool save_replays()
{
	// KP: no saving replays on iPhone
	//return utils::string_bool(preferences::get("save_replays"), false);
	return false;
}

void set_delete_saves(bool value)
{
	preferences::set("delete_saves", value ? "yes" : "no");
}

bool delete_saves()
{
	return utils::string_bool(preferences::get("delete_saves"), false);
}

void set_ask_delete_saves(bool value)
{
	preferences::set("ask_delete", value ? "yes" : "no");
}

bool ask_delete_saves()
{
	return utils::string_bool(preferences::get("ask_delete"), true);
}

int autosavemax()
{
	return lexical_cast_default<int>(preferences::get("auto_save_max"), 10);
}

void set_autosavemax(int value)
{
	preferences::set("auto_save_max", lexical_cast<std::string>(value));
}

std::string client_type()
{
	return preferences::get("client_type") == "ai" ? "ai" : "human";
}

std::string clock_format()
{
/*	
	if (clock_format_cache.size() > 0)
		return clock_format
	if(preferences::get("clock_format").size())
		return preferences::get("clock_format");
	else
		preferences::set("clock_format", "%H:%M");
*/
	// KP: save 14% cpu
	return "%H:%M";
}

std::string theme()
{
	if(non_interactive()) {
		static const std::string null_theme = "null";
		return null_theme;
	}

	std::string res = preferences::get("theme");
	if(res.empty()) {
		return "Default";
	}

	return res;
}

void set_theme(const std::string& theme)
{
	if(theme != "null") {
		preferences::set("theme", theme);
	}
}

bool show_floating_labels()
{
	return utils::string_bool(preferences::get("floating_labels"), true);
}

void set_show_floating_labels(bool value)
{
	preferences::set("floating_labels", value ? "yes" : "no");
}

bool message_private()
{
	return message_private_on;
}

void set_message_private(bool value)
{
	message_private_on = value;
}

bool show_tip_of_day()
{
	return utils::string_bool(preferences::get("tip_of_day"), true);
}

bool show_haloes()
{
	return haloes;
}

void set_show_haloes(bool value)
{
	haloes = value;
	preferences::set("show_haloes", value ? "yes" : "no");
}

bool flip_time()
{
	return utils::string_bool(preferences::get("flip_time"), false);
}

void set_flip_time(bool value)
{
	preferences::set("flip_time", value ? "yes" : "no");
}

bool has_upload_log()
{
	return preferences::get("upload_log_new").empty() == false;
}

bool upload_log()
{
	return utils::string_bool(preferences::get("upload_log_new"), true);
}

void set_upload_log(bool value)
{
	preferences::set("upload_log_new", value ? "yes" : "no");
}

std::string upload_id()
{
	// We create a unique id for each person, *when asked for* to increase
	// randomness.
	if (preferences::get("upload_id") == "") {
		srand(time(NULL));
		preferences::set("upload_id",
			lexical_cast<std::string>(rand())
				 + lexical_cast<std::string>(SDL_GetTicks()));
	}
	return preferences::get("upload_id");
}

bool compress_saves()
{
	return utils::string_bool(preferences::get("compress_saves"), true);
}

bool startup_effect()
{
	return utils::string_bool(preferences::get("startup_effect"), true);
}

std::string get_chat_timestamp(const time_t& t) {
	if (chat_timestamping()) {
		return lg::get_timestamp(t, clock_format()) + " ";
	}
	return "";
}

bool chat_timestamping() {
	return utils::string_bool(preferences::get("chat_timestamp"), false);
}

void set_chat_timestamping(bool value) {
	preferences::set("chat_timestamp", value ? "yes" : "no");
}

int chat_lines()
{
	return lexical_cast_default<int>(preferences::get("chat_lines"), 6);
}

void set_chat_lines(int lines)
{
	preferences::set("chat_lines", lexical_cast<std::string>(lines));
}

std::set<shared_string> &encountered_units() {
	return encountered_units_set;
}

std::set<t_translation::t_terrain> &encountered_terrains() {
	return encountered_terrains_set;
}
	
std::string custom_command() {
	return preferences::get("custom_command");
}

void set_custom_command(const std::string& command) {
	return preferences::set("custom_command", command);
}

/**
 * Returns a pointer to the history vector associated with given id
 * making a new one if it doesn't exist.
 *
 * @todo FIXME only used for gui2. Could be used for the above histories.
 */
std::vector<std::string>* get_history(const std::string& id) {
	return &history_map[id];
}

bool green_confirm()
{
	std::string confirmation = preferences::get("confirm_end_turn");

	if (confirmation == "green" || confirmation == "yes")
		return true;
	return false;
}

bool yellow_confirm()
{
	return preferences::get("confirm_end_turn") == "yellow";
}

bool confirm_no_moves()
{
	//This is very non-intrusive so it is on by default
	const std::string confirmation = preferences::get("confirm_end_turn");
	return confirmation == "no_moves" || confirmation.empty();
}


void encounter_recruitable_units(std::vector<team>& teams){
	for (std::vector<team>::iterator help_team_it = teams.begin();
		help_team_it != teams.end(); help_team_it++) {
		help_team_it->log_recruitable();
		std::copy(help_team_it->recruits().begin(), help_team_it->recruits().end(),
				  std::inserter(encountered_units_set, encountered_units_set.begin()));
	}
}

void encounter_start_units(unit_map& units){
	for (unit_map::const_iterator help_unit_it = units.begin();
		 help_unit_it != units.end(); help_unit_it++) {
		const std::string name = help_unit_it->second.type_id();
		encountered_units_set.insert(name);
	}
}

void encounter_recallable_units(game_state& gamestate){
	for(std::map<shared_string, player_info>::iterator pi = gamestate.players.begin(); pi!=gamestate.players.end(); ++pi) {
		for(std::vector<unit>::iterator help_recall_it = pi->second.available_units.begin(); help_recall_it != pi->second.available_units.end(); help_recall_it++) {
			encountered_units_set.insert(help_recall_it->type_id());
		}
	}
}

void encounter_map_terrain(gamemap& map){
	for (int map_x = 0; map_x < map.w(); map_x++) {
		for (int map_y = 0; map_y < map.h(); map_y++) {
			const t_translation::t_terrain t = map.get_terrain(map_location(map_x, map_y));
			preferences::encountered_terrains().insert(t);
			const t_translation::t_list& underlaying_list = map.underlying_union_terrain(map_location(map_x, map_y));
			for (std::vector<t_translation::t_terrain>::const_iterator ut = underlaying_list.begin(); ut != underlaying_list.end(); ut++) {
				preferences::encountered_terrains().insert(*ut);
			};
		}
	}
}

	
// KP: checks if the achievement has been earned already
bool achievement_earned(int achievement)
{
	if (achievements_set.find(achievement_name(achievement)) != achievements_set.end())
		return true;
	
	return false;
}

// KP: records the achievement
void achievement_add(int achievement)
{
	achievements_set.insert(achievement_name(achievement));
	
	// keep prefs up to date
	std::vector<std::string> a;
	std::copy(achievements_set.begin(), achievements_set.end(), std::back_inserter(a));
	preferences::set("achievements", utils::join(a));
	preferences::set("kills", lexical_cast<std::string>(total_kills));
	preferences::write_preferences();
}

void add_kill()
{
	total_kills++;
	if (total_kills >= 1000 && !achievement_earned(ACHIEVEMENT_PREDATOR))
		earn_achievement(ACHIEVEMENT_PREDATOR);
	else if (total_kills >= 500 && !achievement_earned(ACHIEVEMENT_BONE_CRUSHER))
		earn_achievement(ACHIEVEMENT_BONE_CRUSHER);
	else if (total_kills >= 100 && !achievement_earned(ACHIEVEMENT_BATTLE_MASTER))
		earn_achievement(ACHIEVEMENT_BATTLE_MASTER);
	else if (total_kills >= 50 && !achievement_earned(ACHIEVEMENT_SLAYER))
		earn_achievement(ACHIEVEMENT_SLAYER);
	else if (total_kills >= 25 && !achievement_earned(ACHIEVEMENT_BLOODIED))
		earn_achievement(ACHIEVEMENT_BLOODIED);
		
	turn_kills++;
	if (turn_kills >= 5 && !achievement_earned(ACHIEVEMENT_RAMPAGE))
		earn_achievement(ACHIEVEMENT_RAMPAGE);
	else if (turn_kills >= 3 && !achievement_earned(ACHIEVEMENT_BESERK))
		earn_achievement(ACHIEVEMENT_BESERK);
	
}
	
void reset_turn_kills()
{
	turn_kills = 0;
}
	
void set_player_side(int side)
{
	player_side = side;
}

int get_player_side()
{
	if (isMultiplayer)
		return -1;
	return player_side;
}

void set_multiplayer(bool isMult)
{
	isMultiplayer = isMult;
}

bool is_multiplayer(void)
{
	return isMultiplayer;
}
	
} // preferences namespace
