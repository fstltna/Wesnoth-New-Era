/* $Id: terrain.hpp 31859 2009-01-01 10:28:26Z mordante $ */
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
#ifndef TERRAIN_H_INCLUDED
#define TERRAIN_H_INCLUDED

class config;
#include "tstring.hpp"
#include "terrain_translation.hpp"

#include <map>
#include <string>
#include <vector>
#include "shared_string.hpp"

class terrain_type
{
public:

	terrain_type();
	terrain_type(const config& cfg);
	terrain_type(const terrain_type& base, const terrain_type& overlay);

	const std::string& minimap_image() const { return minimap_image_; }
	const std::string& minimap_image_overlay() const { return minimap_image_overlay_; }
	const std::string& editor_image() const { return editor_image_; }
	const t_string& name() const { return name_; }
	const std::string& id() const { return id_; }

	bool hide_in_editor() const { return hide_in_editor_; }

	//the character representing this terrain
	t_translation::t_terrain number() const { return number_; }

	//the underlying type of the terrain
	const t_translation::t_list& mvt_type() const { return mvt_type_; }
	const t_translation::t_list& def_type() const { return def_type_; }
	const t_translation::t_list& union_type() const { return union_type_; }

	bool is_nonnull() const { return  (number_ != t_translation::NONE_TERRAIN) &&
		(number_ != t_translation::VOID_TERRAIN ); }
	int light_modification() const { return light_modification_; }

	int unit_height_adjust() const { return height_adjust_; }
	double unit_submerge() const { return submerge_; }

	int gives_healing() const { return heals_; }
	bool is_village() const { return village_; }
	bool is_castle() const { return castle_; }
	bool is_keep() const { return keep_; }

	//these descriptions are shown for the terrain in the mouse over
	//depending on the owner or the village
	const t_string& income_description() const { return income_description_; }
	const t_string& income_description_ally() const { return income_description_ally_; }
	const t_string& income_description_enemy() const { return income_description_enemy_; }
	const t_string& income_description_own() const { return income_description_own_; }

	const std::string& editor_group() const { return editor_group_; }

	bool is_overlay() const { return overlay_; }
	bool is_combined() const { return combined_; }

	t_translation::t_terrain default_base() const { return editor_default_base_; }
	t_translation::t_terrain terrain_with_default_base() const;

private:
	/** The image used in the minimap */
	shared_string minimap_image_;
	shared_string minimap_image_overlay_;

	/**
	 *  The image used in the editor pallete if not defined in WML it will be
	 *  initialized with the value of minimap_image_
	 */
	shared_string editor_image_;
	shared_string id_;
	shared_string name_;

	//the 'number' is the number that represents this
	//terrain type. The 'type' is a list of the 'underlying types'
	//of the terrain. This may simply be the same as the number.
	//This is the internal number used, WML still used characters
	t_translation::t_terrain number_;
	t_translation::t_list mvt_type_;
	t_translation::t_list def_type_;
	t_translation::t_list union_type_;

	int height_adjust_;

	double submerge_;

	int light_modification_, heals_;

	shared_string income_description_;
	shared_string income_description_ally_;
	shared_string income_description_enemy_;
	shared_string income_description_own_;

	shared_string editor_group_;

	bool village_, castle_, keep_;

	bool overlay_, combined_;
	t_translation::t_terrain editor_default_base_;
	bool hide_in_editor_;
};

void create_terrain_maps(const std::vector<config*>& cfgs,
                         t_translation::t_list& terrain_list,
                         std::map<t_translation::t_terrain, terrain_type>& letter_to_terrain);

void merge_alias_lists(t_translation::t_list& first, const t_translation::t_list& second);

#endif
