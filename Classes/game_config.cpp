/* $Id: game_config.cpp 34194 2009-03-27 22:13:44Z mordante $ */
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
#include "game_config.hpp"

#include "config.hpp"
#include "gettext.hpp"
#include "log.hpp"
#include "util.hpp"
#include "version.hpp"
#include "wesconfig.h"
#include "serialization/string_utils.hpp"
#ifdef HAVE_REVISION
#include "revision.hpp"
#endif /* HAVE_REVISION */


#define DBG_NG LOG_STREAM(debug, engine)
#define ERR_NG LOG_STREAM(err, engine)

namespace game_config
{
	int base_income = 2;
	int village_income = 1;
	int poison_amount= 8;
	int rest_heal_amount= 2;
	int recall_cost = 20;
	int kill_experience = 8;
	unsigned lobby_refresh = 2000;
	const int gold_carryover_percentage = 80;
	const bool gold_carryover_add = false;
	const std::string version = VERSION;
#ifdef REVISION
	const std::string revision = VERSION " (" REVISION ")";
#else
	const std::string revision = VERSION;
#endif
	std::string wesnothd_name;
	bool debug = false, editor = false, ignore_replay_errors = false, mp_debug = false, exit_at_end = false, no_delay = false, small_gui = false, disable_autosave = false;

	bool use_dummylocales = false;

	int cache_compression_level = 6;

	shared_string game_icon = "wesnoth-icon-small.png", game_title, game_logo, title_music, lobby_music;
	int title_logo_x = 0, title_logo_y = 0, title_buttons_x = 0, title_buttons_y = 0, title_buttons_padding = 0,
	    title_tip_x = 0, title_tip_width = 0, title_tip_padding = 0;

	shared_string terrain_mask_image = "terrain/alphamask.png";
	shared_string grid_image = "terrain/grid.png";
	shared_string unreachable_image = "terrain/darken.png";    /**< overlay image for unreachable tiles. */
	shared_string linger_image = "terrain/darken-linger.png";  /**< overlay image for tiles in linger mode. */

	shared_string energy_image = "misc/bar-energy.png";
	shared_string moved_ball_image = "misc/ball-moved.png";
	shared_string unmoved_ball_image = "misc/ball-unmoved.png";
	shared_string partmoved_ball_image = "misc/ball-partmoved.png";
	shared_string enemy_ball_image = "misc/ball-enemy.png";
	shared_string ally_ball_image = "misc/ball-ally.png";
	shared_string flag_image = "flags/flag-1.png:150,flags/flag-2.png:150,flags/flag-3.png:150,flags/flag-4.png:150";
	shared_string flag_icon_image = "flags/flag_icon.png";
	shared_string flag_rgb = "flag_green";
	std::vector<Uint32> defense_color_scale;

	double hp_bar_scaling = 0.666;
	double xp_bar_scaling = 0.5;

	shared_string cross_image = "misc/cross.png";

	std::vector<std::string> foot_speed_prefix;
	std::string foot_teleport_enter = "footsteps/teleport-in.png";
	std::string foot_teleport_exit = "footsteps/teleport-out.png";

	shared_string observer_image = "misc/eye.png";
	shared_string tod_bright_image = "misc/tod-bright.png";
	shared_string unchecked_menu_image = "buttons/checkbox.png";
	shared_string checked_menu_image = "buttons/checkbox-pressed.png";
	shared_string wml_menu_image = "buttons/WML-custom.png";

	shared_string level_image;
	shared_string ellipsis_image;

	shared_string default_victory_music;
	shared_string default_defeat_music;

	std::map<std::string, color_range > team_rgb_range;
	std::map<std::string, t_string > team_rgb_name;

	std::map<std::string, std::vector<Uint32> > team_rgb_colors;

	const version_info wesnoth_version(VERSION);
	const version_info min_savegame_version(MIN_SAVEGAME_VERSION);
	const std::string  test_version("test");

	const std::string observer_team_name = "observer";

	const size_t max_loop = 65536;

	namespace sounds {
		const std::string turn_bell = "bell.wav",
		timer_bell = "timer.wav",
		receive_message = "chat-1.ogg,chat-2.ogg,chat-3.ogg",
		receive_message_highlight = "chat-highlight.ogg",
		receive_message_friend = "chat-friend.ogg",
		receive_message_server = "receive.wav",
		user_arrive = "arrive.wav",
		user_leave = "leave.wav",
		game_user_arrive = "join.wav",
		game_user_leave = "leave.wav";

		const std::string button_press = "button.wav",
		checkbox_release = "checkbox.wav",
		slider_adjust = "slider.wav",
		menu_expand = "expand.wav",
		menu_contract = "contract.wav",
		menu_select = "select.wav";
	}



#ifdef __AMIGAOS4__
	std::string path = "PROGDIR:";
#else
#ifdef WESNOTH_PATH
	std::string path = WESNOTH_PATH;
#else
	std::string path = "";
#endif
#endif

	std::string preferences_dir = "";

	std::vector<server_info> server_list;

	void load_config(const config* cfg)
	{
		if(cfg == NULL)
			return;

		const config& v = *cfg;

		base_income = lexical_cast_default<int>(v["base_income"], 2);
		village_income = lexical_cast_default<int>(v["village_income"], 1);
		poison_amount = lexical_cast_default<int>(v["poison_amount"], 8);
		rest_heal_amount = lexical_cast_default<int>(v["rest_heal_amount"], 2);
		recall_cost = lexical_cast_default<int>(v["recall_cost"], 20);
		kill_experience = lexical_cast_default<int>(v["kill_experience"], 8);
		lobby_refresh = lexical_cast_default<unsigned>(v["lobby_refresh"], 2000);

		game_icon = v["icon"];
		game_title = v["title"];
		game_logo = v["logo"];
		title_music = v["title_music"];
		lobby_music = v["lobby_music"];

		title_logo_x = lexical_cast_default<int>(v["logo_x"]);
		title_logo_y = lexical_cast_default<int>(v["logo_y"]);
		title_buttons_x = lexical_cast_default<int>(v["buttons_x"]);
		title_buttons_y = lexical_cast_default<int>(v["buttons_y"]);
		title_buttons_padding = lexical_cast_default<int>(v["buttons_padding"]);

		title_tip_x = lexical_cast_default<int>(v["tip_x"]);
		title_tip_width = lexical_cast_default<int>(v["tip_width"]);
		title_tip_padding = lexical_cast_default<int>(v["tip_padding"]);


		energy_image = v["energy_image"];
		moved_ball_image = v["moved_ball_image"];
		unmoved_ball_image = v["unmoved_ball_image"];
		partmoved_ball_image = v["partmoved_ball_image"];
		enemy_ball_image = v["enemy_ball_image"];
		ally_ball_image = v["ally_ball_image"];
		flag_image = v["flag_image"];
		flag_icon_image = v["flag_icon_image"];
		cross_image = v["cross_image"];

		hp_bar_scaling = lexical_cast_default<double>(v["hp_bar_scaling"]);
		xp_bar_scaling = lexical_cast_default<double>(v["xp_bar_scaling"]);

		foot_speed_prefix = utils::split(v["footprint_prefix"]);
		foot_teleport_enter = v["footprint_teleport_enter"];
		foot_teleport_exit = v["footprint_teleport_exit"];

		terrain_mask_image = v["terrain_mask_image"];
		grid_image = v["grid_image"];
		unreachable_image = v["unreachable_image"];

		observer_image = v["observer_image"];
		tod_bright_image = v["tod_bright_image"];

		level_image = v["level_image"];
		ellipsis_image = v["ellipsis_image"];
		default_victory_music = v["default_victory_music"];
		default_defeat_music = v["default_defeat_music"];

		add_color_info(v);

		flag_rgb = v["flag_rgb"];
		if( !flag_rgb.size()){
			flag_rgb="flag_green";
		}
		defense_color_scale = string2rgb(v["defense_color_scale"]);
		if (defense_color_scale.empty()) {
			defense_color_scale.push_back(0x00FFFF00);
		}

		server_list.clear();
		const std::vector<config *> &servers = v.get_children("server");
		std::vector<config *>::const_iterator server;
		for(server = servers.begin(); server != servers.end(); ++server) {
			server_info sinf;
			sinf.name = (**server)["name"];
			sinf.address = (**server)["address"];
			server_list.push_back(sinf);
		}
		if (!v["wesnothd_name"].empty())
		{
			game_config::wesnothd_name = v["wesnothd_name"];
		}
	}

	void add_color_info(const config& v){
		const config::child_list& team_colors = v.get_children("color_range");
		for(config::child_list::const_iterator teamC = team_colors.begin(); teamC != team_colors.end(); ++teamC) {
			if(!(**teamC)["id"].empty() && !(**teamC)["rgb"].empty()){
				std::string id = (**teamC)["id"];
				std::vector<Uint32> temp = string2rgb((**teamC)["rgb"]);
				team_rgb_range.insert(std::make_pair(id,color_range(temp)));
				team_rgb_name[id] = (**teamC)["name"];
				//generate palette of same name;
				std::vector<Uint32> tp = palette(team_rgb_range[id]);
				if(tp.size()){
					team_rgb_colors.insert(std::make_pair(id,tp));
					//if this is being used, output log of palette for artists use.
					DBG_NG << "color palette creation:\n";
					std::stringstream str;
					str << id <<" = ";
					for(std::vector<Uint32>::const_iterator r=tp.begin();r!=tp.end();r++){
						int red = ((*r) & 0x00FF0000)>>16;
						int green = ((*r) & 0x0000FF00)>>8;
						int blue = ((*r) & 0x000000FF);
						if(r!=tp.begin()) {
							str<<",";
						}
						str << red << "," << green << "," << blue;
					}
					DBG_NG << str.str() <<"\n";
				}
			}
		}

		const config::child_list& colors = v.get_children("color_palette");
		for(config::child_list::const_iterator cp = colors.begin(); cp != colors.end(); ++cp) {
			for(string_map::const_iterator rgb_it = (*cp)->values.begin(); rgb_it != (*cp)->values.end(); ++rgb_it) {
				try {
					team_rgb_colors.insert(std::make_pair(rgb_it->first,string2rgb(rgb_it->second)));
				} catch(bad_lexical_cast&) {
					//throw config::error(_("Invalid team color: ") + rgb_it->second);
					ERR_NG << "Invalid team color: " << rgb_it->second << "\n";
				}
			}
		}
	}

	const color_range& color_info(const std::string& name)
	{
		// KP: get rid of trailing \n
		std::string checkName = name;
		int pos = checkName.find('\n');
		if (pos != std::string::npos)
		{
			checkName = checkName.substr(0, pos);
		}
		std::map<std::string, color_range>::const_iterator i = team_rgb_range.find(checkName);
		if(i == team_rgb_range.end()) {
			try {
				team_rgb_range.insert(std::make_pair(name,color_range(string2rgb(checkName))));
				return color_info(checkName);
			} catch(bad_lexical_cast&) {
				//ERR_NG << "Invalid color range: " << name;
				//return color_info();
				throw config::error(_("Invalid color range: ") + name);
			}
		}
		return i->second;
	}

	const std::vector<Uint32>& tc_info(const std::string& name)
	{
		std::map<std::string, std::vector<Uint32> >::const_iterator i = team_rgb_colors.find(name);
		if(i == team_rgb_colors.end()) {
			try {
				team_rgb_colors.insert(std::make_pair(name,string2rgb(name)));
				return tc_info(name);
			} catch(bad_lexical_cast&) {
				static std::vector<Uint32> stv;
				//throw config::error(_("Invalid team color: ") + name);
				ERR_NG << "Invalid team color: " << name << "\n";
				return stv;
			}
		}
		return i->second;
	}

	bool is_compatible_savegame_version(const std::string& v)
	{
		bool ret = v==version;
		if(ret) {
			return true;
		} else {
			// do not load if too old, if either the savegame or the current game
			// has the version 'test' allow loading
			try {
				ret = !(v < min_savegame_version.str() && test_version != v && test_version != version);
			} catch(version_info::not_sane_exception&) {
				ERR_NG << "invalid version_info from saved game: '" << v << "'\n";
				ret = false;
			}
		}
		return ret;
	}

} // game_config
