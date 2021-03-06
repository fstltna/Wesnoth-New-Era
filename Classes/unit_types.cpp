/* $Id: unit_types.cpp 35616 2009-05-13 03:46:28Z alink $ */
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
 *  @file unit_types.cpp
 *  Handle unit-type specific attributes, animations, advancement.
 */

#include "global.hpp"

#include "asserts.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "log.hpp"
#include "map.hpp"
#include "unit_types.hpp"


#define ERR_CONFIG LOG_STREAM(err, config)
#define WRN_CONFIG LOG_STREAM(warn, config)
#define LOG_CONFIG LOG_STREAM(info, config)
#define DBG_CONFIG LOG_STREAM(debug, config)
#define DBG_UT LOG_STREAM(debug, engine)
#define LOG_UT LOG_STREAM(info, engine)
#define ERR_UT LOG_STREAM(err, engine)

namespace {
	std::map< std::string, std::set< std::string > > future_advancefroms;
}

attack_type::attack_type(const config& cfg) :
	aloc_(),
	dloc_(),
	attacker_(false),
	unitmap_(NULL),
	map_(NULL),
	game_status_(NULL),
	teams_(NULL),
	other_attack_(NULL),
	cfg_(cfg),
	animation_(),
	description_(cfg["description"]),
	id_(cfg["name"]),
	type_(cfg["type"]),
	icon_(cfg["icon"]),
	//range_(cfg["range"].base_str()),
	range_(cfg["range"]),
	damage_(atol(cfg["damage"].c_str())),
	num_attacks_(atol(cfg["number"].c_str())),
	attack_weight_(lexical_cast_default<double>(cfg["attack_weight"],1.0)),
	defense_weight_(lexical_cast_default<double>(cfg["defense_weight"],1.0)),
	accuracy_(atol(cfg["accuracy"].c_str())),
	parry_(atol(cfg["parry"].c_str()))

{
	if (description_.empty())
		description_ = egettext(id_.c_str());

	if(icon_.empty()){
		if (id_ != "")
			icon_ = "attacks/" + id_ + ".png";
		else
			icon_ = "attacks/blank-attack.png";
	}
}

std::string attack_type::accuracy_parry_description() const
{
	if(accuracy_ == 0 && parry_ == 0) {
		return "";
	}

	std::ostringstream s;
	if(accuracy_ > 0) {
		s << "+";
	}

	s << accuracy_ << "%";

	if(parry_ != 0) {
		s << "/";
		if(parry_ > 0) {
			s << "+";
		}

		s << parry_ << "%";
	}

	return s.str();
}

bool attack_type::matches_filter(const config& cfg,bool self) const
{
	const std::vector<std::string>& filter_range = utils::split(cfg["range"]);
	const std::vector<std::string> filter_name = utils::split(cfg["name"]);
	const std::vector<std::string> filter_type = utils::split(cfg["type"]);
	const std::string filter_special = cfg["special"];

	if(filter_range.empty() == false && std::find(filter_range.begin(),filter_range.end(),range()) == filter_range.end())
			return false;

	if(filter_name.empty() == false && std::find(filter_name.begin(),filter_name.end(),id()) == filter_name.end())
		return false;

	if(filter_type.empty() == false && std::find(filter_type.begin(),filter_type.end(),type()) == filter_type.end())
		return false;

	if(!self && filter_special.empty() == false && !get_special_bool(filter_special,true))
		return false;

	return true;
}

bool attack_type::apply_modification(const config& cfg,std::string* description)
{
	if(!matches_filter(cfg,0))
		return false;

	const std::string& set_name = cfg["set_name"];
	const t_string& set_desc = cfg["set_description"];
	const std::string& set_type = cfg["set_type"];
	const std::string& del_specials = cfg["remove_specials"];
	const config* set_specials = cfg.child("set_specials");
	const std::string& increase_damage = cfg["increase_damage"];
	const std::string& increase_attacks = cfg["increase_attacks"];
	const std::string& set_attack_weight = cfg["attack_weight"];
	const std::string& set_defense_weight = cfg["defense_weight"];
	const std::string& increase_accuracy = cfg["increase_accuracy"];
	const std::string& increase_parry = cfg["increase_parry"];

	std::stringstream desc;

	if(set_name.empty() == false) {
		id_ = set_name;
		cfg_["name"] = id_;
	}

	if(set_desc.empty() == false) {
		description_ = set_desc;
		cfg_["description"] = description_;
	}

	if(set_type.empty() == false) {
		type_ = set_type;
		cfg_["type"] = type_;
	}

	if(del_specials.empty() == false) {
		const std::vector<std::string>& dsl = utils::split(del_specials);
		config* specials = cfg_.child("specials");
		if (specials != NULL) {
			config new_specials;
			for(config::all_children_iterator s = specials->ordered_begin(); s != specials->ordered_end(); ++s) {
				const config::all_children_iterator::value_type& vp = *s;
				std::vector<std::string>::const_iterator found_id =
					std::find(dsl.begin(),dsl.end(),vp.second->get_attribute("id"));
				if (found_id == dsl.end()) {
					new_specials.add_child(vp.first,*vp.second);
				}
			}
			cfg_.clear_children("specials");
			cfg_.add_child("specials",new_specials);
		}
	}

	if (set_specials) {
		const std::string& mode = set_specials->get_attribute("mode");
		if ( mode != "append") {
			cfg_.clear_children("specials");
		}
		config* new_specials = cfg_.child("specials");
		if (new_specials == NULL) {
			cfg_.add_child("specials");
			new_specials = cfg_.child("specials");
		}
		for(config::all_children_iterator s = set_specials->ordered_begin(); s != set_specials->ordered_end(); ++s) {
			const config::all_children_iterator::value_type& value = *s;
			new_specials->add_child(value.first,*value.second);
		}
	}

	if(increase_damage.empty() == false) {
		damage_ = utils::apply_modifier(damage_, increase_damage, 1);
		cfg_["damage"] = lexical_cast_default<std::string>(damage_);

		if(description != NULL) {
			desc << (increase_damage[0] == '-' ? "" : "+") << increase_damage
				<< " " << _n("damage","damage",lexical_cast<int>(increase_damage));
		}
	}

	if(increase_attacks.empty() == false) {
		num_attacks_ = utils::apply_modifier(num_attacks_, increase_attacks, 1);
		cfg_["number"] = lexical_cast_default<std::string>(num_attacks_);

		if(description != NULL) {
			desc << (increase_attacks[0] == '-' ? "" : "+") << increase_attacks
				<< " " << _n("strike","strikes",lexical_cast<int>(increase_attacks));
		}
	}

	if(increase_accuracy.empty() == false) {
		accuracy_ = utils::apply_modifier(accuracy_, increase_accuracy, 1);
		cfg_["accuracy"] = lexical_cast_default<std::string>(accuracy_);

		if(description != NULL) {
			// Help xgettext with a directive to recognise the string as a non C printf-like string
			// xgettext:no-c-format
			desc << (increase_accuracy[0] == '-' ? "" : "+") << increase_accuracy << _("% accuracy");
		}
	}

	if(increase_parry.empty() == false) {
		parry_ = utils::apply_modifier(parry_, increase_parry, 1);
		cfg_["parry"] = lexical_cast_default<std::string>(parry_);

		if(description != NULL) {
			// xgettext:no-c-format
			desc << (increase_parry[0] == '-' ? "" : "+") << increase_parry << _("% parry");
		}
	}

	if(set_attack_weight.empty() == false) {
		attack_weight_ = lexical_cast_default<double>(set_attack_weight,1.0);
		cfg_["attack_weight"] = lexical_cast_default<std::string>(attack_weight_);
	}

	if(set_defense_weight.empty() == false) {
		defense_weight_ = lexical_cast_default<double>(set_defense_weight,1.0);
		cfg_["defense_weight"] = lexical_cast_default<std::string>(defense_weight_);
	}

	if(description != NULL) {
		*description = desc.str();
	}

	return true;
}

// Same as above, except only update the descriptions
bool attack_type::describe_modification(const config& cfg,std::string* description)
{
	if(!matches_filter(cfg,0))
		return false;

	const std::string& increase_damage = cfg["increase_damage"];
	const std::string& increase_attacks = cfg["increase_attacks"];

	std::stringstream desc;

	if(increase_damage.empty() == false) {
		if(description != NULL) {
			desc << (increase_damage[0] == '-' ? "" : "+") << increase_damage
				<< " " << _n("damage","damage",lexical_cast<int>(increase_damage));
		}
	}

	if(increase_attacks.empty() == false) {
		if(description != NULL) {
			desc << (increase_attacks[0] == '-' ? "" : "+") << increase_attacks
				<< " " << _n("strike","strikes",lexical_cast<int>(increase_attacks));
		}
	}

	if(description != NULL) {
		*description = desc.str();
	}

	return true;
}
bool attack_type::has_special_by_id(const std::string& special) const
{
	const config* abil = cfg_.child("specials");
	if(abil) {
		for(config::child_map::const_iterator i = abil->all_children().begin(); i != abil->all_children().end(); ++i) {
			for(config::child_list::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
				if((**j)["id"] == special) {
					return true;
				}
			}
		}
	}
	return false;
}

unit_movement_type::unit_movement_type(const config& cfg, const unit_movement_type* parent) :
	moveCosts_(),
	defenseMods_(),
	parent_(parent),
	cfg_()
{
	//the unit_type give its whole cfg, we don't need all that.
	//so we filter to keep only keys related to movement_type
	//FIXME: This helps but it's still not clean, both cfg use a "name" key

	const t_string& name = cfg["name"];
	if (!name.empty())
		cfg_["name"]= cfg["name"];

	const t_string& flies = cfg["flies"];
	if (!flies.empty())
		cfg_["flies"]= cfg["flies"];

	const config* movement_costs = cfg.child("movement_costs");
	if (movement_costs!=NULL)
		cfg_.add_child("movement_costs", *movement_costs);

	const config* defense = cfg.child("defense");
	if (defense!=NULL)
		cfg_.add_child("defense", *defense);

	const config* resistance = cfg.child("resistance");
	if (resistance!=NULL)
		cfg_.add_child("resistance", *resistance);
}

unit_movement_type::unit_movement_type(): moveCosts_(), defenseMods_(), parent_(NULL), cfg_()
{}

const t_string& unit_movement_type::name() const
{
	const t_string& res = cfg_.get_attribute("name");
	if(res == "" && parent_ != NULL)
		return parent_->name();
	else
		return res;
}

int unit_movement_type::movement_cost(const gamemap& map,
		t_translation::t_terrain terrain, int recurse_count) const
{
	const int impassable = 10000000;

	const std::map<t_translation::t_terrain, int>::const_iterator i =
		moveCosts_.find(terrain);

	if(i != moveCosts_.end()) {
		return i->second;
	}

	// If this is an alias, then select the best of all underlying terrains.
	const t_translation::t_list& underlying = map.underlying_mvt_terrain(terrain);
	if(underlying.size() != 1 || underlying.front() != terrain) {
		bool revert = (underlying.front() == t_translation::MINUS ? true : false);
		if(recurse_count >= 100) {
			return impassable;
		}

		int ret_value = revert?0:impassable;
		for(t_translation::t_list::const_iterator i = underlying.begin();
				i != underlying.end(); ++i) {

			if(*i == t_translation::PLUS) {
				revert = false;
				continue;
			} else if(*i == t_translation::MINUS) {
				revert = true;
				continue;
			}
			const int value = movement_cost(map,*i,recurse_count+1);
			if(value < ret_value && !revert) {
				ret_value = value;
			} else if(value > ret_value && revert) {
				ret_value = value;
			}
		}

		moveCosts_.insert(std::pair<t_translation::t_terrain, int>(terrain,ret_value));

		return ret_value;
	}

	const config* movement_costs = cfg_.child("movement_costs");

	int res = -1;

	if(movement_costs != NULL) {
		if(underlying.size() != 1) {
			LOG_STREAM(err, config) << "terrain '" << terrain << "' has "
				<< underlying.size() << " underlying names - 0 expected\n";

			return impassable;
		}

		const std::string& id = map.get_terrain_info(underlying.front()).id();

		const std::string& val = (*movement_costs)[id];

		if(val != "") {
			res = atoi(val.c_str());
		}
	}

	if(res <= 0 && parent_ != NULL) {
		res = parent_->movement_cost(map,terrain);
	}

	if(res <= 0) {
		res = impassable;
	}

	moveCosts_.insert(std::pair<t_translation::t_terrain, int>(terrain,res));

	return res;
}

int unit_movement_type::defense_modifier(const gamemap& map,
		t_translation::t_terrain terrain, int recurse_count) const
{
	const std::map<t_translation::t_terrain, int>::const_iterator i =
		defenseMods_.find(terrain);

	if(i != defenseMods_.end()) {
		return i->second;
	}

	// If this is an alias, then select the best of all underlying terrains.
	const t_translation::t_list& underlying =
		map.underlying_def_terrain(terrain);

	if(underlying.size() != 1 || underlying.front() != terrain) {
		bool revert = (underlying.front() == t_translation::MINUS ? true : false);
		if(recurse_count >= 100) {
			return 100;
		}

		int ret_value = revert?0:100;
		for(t_translation::t_list::const_iterator i = underlying.begin();
				i != underlying.end(); ++i) {

			if(*i == t_translation::PLUS) {
				revert = false;
				continue;
			} else if(*i == t_translation::MINUS) {
				revert = true;
				continue;
			}
			const int value = defense_modifier(map,*i,recurse_count+1);
			if(value < ret_value && !revert) {
				ret_value = value;
			} else if(value > ret_value && revert) {
				ret_value = value;
			}
		}

		defenseMods_.insert(std::pair<t_translation::t_terrain, int>(terrain, ret_value));

		return ret_value;
	}

	int res = -1;

	const config* const defense = cfg_.child("defense");

	if(defense != NULL) {
		if(underlying.size() != 1) {
			LOG_STREAM(err, config) << "terrain '" << terrain << "' has "
				<< underlying.size() << " underlying names - 0 expected\n";

			return 100;
		}

		const std::string& id = map.get_terrain_info(underlying.front()).id();
		const std::string& val = (*defense)[id];

		if(val != "") {
			res = atoi(val.c_str());
		}
	}

	if(res <= 0 && parent_ != NULL) {
		res = parent_->defense_modifier(map,terrain);
	}

	if(res < 0) {
		ERR_CONFIG << "Defence '" << res << "' is '< 0' reset to 0 (100% defense).\n";
		res = 0;
	}

	defenseMods_.insert(std::pair<t_translation::t_terrain, int>(terrain, res));

	return res;
}

int unit_movement_type::resistance_against(const attack_type& attack) const
{
	bool result_found = false;
	int res = 100;

	const config* const resistance = cfg_.child("resistance");
	if(resistance != NULL) {
		const std::string& val = (*resistance)[attack.type()];
		if(val != "") {
			res = atoi(val.c_str());
			result_found = true;
		}
	}

	if(!result_found && parent_ != NULL) {
		res = parent_->resistance_against(attack);
	}

	return res;
}

string_map unit_movement_type::damage_table() const
{
	string_map res;
	if(parent_ != NULL)
		res = parent_->damage_table();

	const config* const resistance = cfg_.child("resistance");
	if(resistance != NULL) {
		for(string_map::const_iterator i = resistance->values.begin(); i != resistance->values.end(); ++i) {
			res[i->first] = i->second;
		}
	}

	return res;
}

bool unit_movement_type::is_flying() const
{
	const std::string& flies = cfg_["flies"];
	if(flies == "" && parent_ != NULL)
		return parent_->is_flying();

	return utils::string_bool(flies);
}

unit_type::unit_type() :
	cfg_(),
	id_(),
	type_name_(),
	description_(),
	hitpoints_(0),
	level_(0),
	movement_(0),
	max_attacks_(0),
	cost_(0),
	usage_(),
	undead_variation_(),
	image_(),
	image_profile_(),
	flag_rgb_(),
	num_traits_(0),
	gender_types_(),
	variations_(),
	race_(NULL),
	alpha_(),
	abilities_(),
	adv_abilities_(),
	ability_tooltips_(),
	adv_ability_tooltips_(),
	zoc_(false),
	hide_help_(false),
	advances_to_(),
	advances_from_(),
	experience_needed_(0),
	alignment_(),
	movementType_(),
	possibleTraits_(),
	genders_(),
	animations_(),
	build_status_(NOT_BUILT),
	portraits_()
{
	gender_types_[0] = NULL;
	gender_types_[1] = NULL;
}

unit_type::unit_type(const unit_type& o) :
	cfg_(o.cfg_),
	id_(o.id_),
	type_name_(o.type_name_),
	description_(o.description_),
	hitpoints_(o.hitpoints_),
	level_(o.level_),
	movement_(o.movement_),
	max_attacks_(o.max_attacks_),
	cost_(o.cost_),
	usage_(o.usage_),
	undead_variation_(o.undead_variation_),
	image_(o.image_),
	image_profile_(o.image_profile_),
	flag_rgb_(o.flag_rgb_),
	num_traits_(o.num_traits_),
	variations_(o.variations_),
	race_(o.race_),
	alpha_(o.alpha_),
	abilities_(o.abilities_),
	adv_abilities_(o.adv_abilities_),
	ability_tooltips_(o.ability_tooltips_),
	adv_ability_tooltips_(o.adv_ability_tooltips_),
	zoc_(o.zoc_),
	hide_help_(o.hide_help_),
	advances_to_(o.advances_to_),
	advances_from_(o.advances_from_),
	experience_needed_(o.experience_needed_),
	alignment_(o.alignment_),
	movementType_(o.movementType_),
	possibleTraits_(o.possibleTraits_),
	genders_(o.genders_),
	animations_(o.animations_),
    build_status_(o.build_status_),
	portraits_(o.portraits_)
{
	gender_types_[0] = o.gender_types_[0] != NULL ? new unit_type(*o.gender_types_[0]) : NULL;
	gender_types_[1] = o.gender_types_[1] != NULL ? new unit_type(*o.gender_types_[1]) : NULL;

	for(variations_map::const_iterator i = o.variations_.begin(); i != o.variations_.end(); ++i) {
		variations_[i->first] = new unit_type(*i->second);
	}
}


unit_type::unit_type(const config& cfg, const movement_type_map& mv_types,
		const race_map& races, const std::vector<config*>& traits) :
	cfg_(),
	id_(),
	type_name_(),
	description_(),
	hitpoints_(0),
	level_(0),
	movement_(0),
	max_attacks_(0),
	cost_(0),
	usage_(),
	undead_variation_(),
	image_(),
	image_profile_(),
	flag_rgb_(),
	num_traits_(0),
	gender_types_(),
	variations_(),
	race_(NULL),
	alpha_(),
	abilities_(),
	adv_abilities_(),
	ability_tooltips_(),
	adv_ability_tooltips_(),
	zoc_(false),
	hide_help_(false),
	advances_to_(),
	advances_from_(),
	experience_needed_(0),
	alignment_(),
	movementType_(),
	possibleTraits_(),
	genders_(),
	animations_(),
	build_status_(NOT_BUILT),
	portraits_()
{
	build_full(cfg, mv_types, races, traits);
}

unit_type::~unit_type()
{
    if (gender_types_[unit_race::MALE] != NULL)
        delete gender_types_[unit_race::MALE];
    if (gender_types_[unit_race::FEMALE] != NULL)
        delete gender_types_[unit_race::FEMALE];

	for(variations_map::iterator i = variations_.begin(); i != variations_.end(); ++i) {
		delete i->second;
	}
}

void unit_type::set_config(const config& cfg)
{
    cfg_ = cfg;
	id_ = cfg_["id"];
}

void unit_type::build_full(const config& cfg, const movement_type_map& mv_types,
                     const race_map& races, const std::vector<config*>& traits)
{
    if ( (build_status_ == NOT_BUILT) || (build_status_ == CREATED) )
        build_help_index(cfg, mv_types, races, traits);

	movementType_ = unit_movement_type(cfg);
	alpha_ = ftofxp(1.0);

	config::const_child_iterator i;
	for(i=traits.begin(); i != traits.end(); ++i)
	{
		possibleTraits_.add_child("trait", **i);
	}
	const config::child_list& variations = cfg.get_children("variation");
	for(config::child_list::const_iterator var = variations.begin(); var != variations.end(); ++var) {
		const config& var_cfg = **var;
		if(utils::string_bool(var_cfg["inherit"])) {
			config nvar_cfg(cfg);
			nvar_cfg.merge_with(var_cfg);
			nvar_cfg.clear_children("variation");
			variations_.insert(std::pair<std::string,unit_type*>(nvar_cfg["variation_name"],
												 new unit_type(nvar_cfg,mv_types,races,traits)));
		} else {
			variations_.insert(std::pair<std::string,unit_type*>((**var)["variation_name"],
												 new unit_type(**var,mv_types,races,traits)));
		}
	}

	const std::string& align = cfg["alignment"];
	if(align == "lawful")
		alignment_ = LAWFUL;
	else if(align == "chaotic")
		alignment_ = CHAOTIC;
	else if(align == "neutral")
		alignment_ = NEUTRAL;
	else {
		LOG_STREAM(err, config) << "Invalid alignment found for " << id() << ": '" << align << "'\n";
		alignment_ = NEUTRAL;
	}

	const race_map::const_iterator race_it = races.find(cfg["race"]);
	if(race_it != races.end()) {
		race_ = &race_it->second;
		if(race_ != NULL) {
			if(race_->uses_global_traits() == false) {
				possibleTraits_.clear();
			}
			if(utils::string_bool(cfg["ignore_race_traits"])) {
				possibleTraits_.clear();
			} else {
				const config::child_list& traits = race_->additional_traits();
				for(i=traits.begin(); i != traits.end(); ++i)
				{
					if(alignment_ != NEUTRAL || ((**i)["id"]) != "fearless")
						possibleTraits_.add_child("trait", **i);
				}
			}
		}
	} else {
		static const unit_race dummy_race;
		race_ = &dummy_race;
	}

	// Insert any traits that are just for this unit type
	const config::child_list& unit_traits = cfg.get_children("trait");
	for(i=unit_traits.begin(); i != unit_traits.end(); ++i)
	{
		possibleTraits_.add_child("trait", **i);
	}

	zoc_ = utils::string_bool(cfg["zoc"], level_ > 0);

	const std::string& alpha_blend = cfg["alpha"];
	if(alpha_blend.empty() == false) {
		alpha_ = ftofxp(atof(alpha_blend.c_str()));
	}

	const std::string& move_type = cfg["movement_type"];

	const movement_type_map::const_iterator it = mv_types.find(move_type);

	if(it != mv_types.end()) {
	    DBG_UT << "setting parent for movement_type " << move_type << "\n";
		movementType_.set_parent(&(it->second));
	}
	else{
	    DBG_UT << "no parent found for movement_type " << move_type << "\n";
	}

	flag_rgb_ = cfg["flag_rgb"];
	game_config::add_color_info(cfg);


	for(config::const_child_itors range = cfg_.child_range("portrait");
	    range.first != range.second; ++range.first) {
		portraits_.push_back(tportrait(**range.first));
	}

	// Deprecation messages, only seen when unit is parsed for the first time.

	build_status_ = FULL;
}

void unit_type::build_help_index(const config& cfg, const movement_type_map& mv_types,
                     const race_map& races, const std::vector<config*>& traits)
{
    if (build_status_ == NOT_BUILT){
        set_config(cfg);
        build_created(cfg, mv_types, races, traits);
    }

	type_name_ = sgettext(cfg_["name"].c_str());
	description_ = cfg_["description"];
	hitpoints_ = lexical_cast_default<int>(cfg["hitpoints"], 1);
	level_ = lexical_cast_default<int>(cfg["level"], 0);
	movement_ = lexical_cast_default<int>(cfg["movement"], 1);
	max_attacks_ = lexical_cast_default<int>(cfg["attacks"], 1);
	cost_ = lexical_cast_default<int>(cfg["cost"], 1);
	usage_ = cfg_["usage"];
	undead_variation_ = cfg_["undead_variation"];
	image_ = cfg_["image"];
    image_profile_ = cfg_["profile"];

	const race_map::const_iterator race_it = races.find(cfg["race"]);
	if(race_it != races.end()) {
		race_ = &race_it->second;
	} else {
		static const unit_race dummy_race;
		race_ = &dummy_race;
	}

	// if num_traits is not defined, we use the num_traits from race
	num_traits_ = lexical_cast_default<unsigned int>(cfg["num_traits"], race_->num_traits());

	const std::vector<std::string> genders = utils::split(cfg["gender"]);
	for(std::vector<std::string>::const_iterator g = genders.begin(); g != genders.end(); ++g) {
		genders_.push_back(string_gender(*g));
	}
	if(genders_.empty()) {
		genders_.push_back(unit_race::MALE);
	}

	const config* abil_cfg = cfg.child("abilities");
	if(abil_cfg) {
		const config::child_map& abi = abil_cfg->all_children();
		for(config::child_map::const_iterator j = abi.begin(); j != abi.end(); ++j) {
			for(config::child_list::const_iterator k = j->second.begin(); k != j->second.end(); ++k) {
				if((**k)["name"] != "") {
					abilities_.push_back((**k)["name"]);
					ability_tooltips_.push_back((**k)["description"]);
				}
			}
		}
	}

	for(config::const_child_itors adv_itors = cfg.child_range("advancement");
	adv_itors.first != adv_itors.second; ++adv_itors.first)
	{
		for(config::const_child_itors eff_itors = (**adv_itors.first).child_range("effect");
		eff_itors.first != eff_itors.second; ++eff_itors.first)
		{
			const config* abil_cfg = (**eff_itors.first).child("abilities");
			if(!abil_cfg || (**eff_itors.first)["apply_to"] != "new_ability") {
				continue;
			}
			const config::child_map& abi = abil_cfg->all_children();
			for(config::child_map::const_iterator j = abi.begin(); j != abi.end(); ++j) {
				for(config::child_list::const_iterator k = j->second.begin(); k != j->second.end(); ++k) {
					if((**k)["name"] != "") {
						adv_abilities_.push_back((**k)["name"]);
						adv_ability_tooltips_.push_back((**k)["description"]);
					}
				}
			}
		}
	}

	hide_help_= utils::string_bool(cfg["hide_help"],false);

	build_status_ = HELP_INDEX;

	std::map< std::string, std::set< std::string > >::const_iterator adv_froms = future_advancefroms.find(id_);
	if (adv_froms != future_advancefroms.end()) {
		std::set< std::string >::const_iterator adv_it,
			adv_end = adv_froms->second.end();
		for(adv_it = adv_froms->second.begin(); adv_it != adv_end; ++adv_it) {
			add_advancesfrom(*adv_it);
		}
		future_advancefroms.erase(id_);
	}
}

void unit_type::build_created(const config& cfg, const movement_type_map& mv_types,
                     const race_map& races, const std::vector<config*>& traits)
{
	gender_types_[0] = NULL;
	gender_types_[1] = NULL;

	const config* const male_cfg = cfg.child("male");
	if(male_cfg != NULL) {
		config m_cfg;
		if(!utils::string_bool((*male_cfg)["inherit"], true)) {
			m_cfg = *male_cfg;
		} else {
			m_cfg = cfg;
			m_cfg.merge_with(*male_cfg);
		}
		m_cfg.clear_children("male");
		m_cfg.clear_children("female");
		gender_types_[unit_race::MALE] = new unit_type(m_cfg,mv_types,races,traits);
	}

	const config* const female_cfg = cfg.child("female");
	if(female_cfg != NULL) {
		config f_cfg;
		if(!utils::string_bool((*female_cfg)["inherit"], true)) {
			f_cfg = *female_cfg;
		} else {
			f_cfg = cfg;
			f_cfg.merge_with(*female_cfg);
		}
		f_cfg.clear_children("male");
		f_cfg.clear_children("female");
		gender_types_[unit_race::FEMALE] = new unit_type(f_cfg,mv_types,races,traits);
	}

    const std::string& advances_to_val = cfg["advances_to"];
    if(advances_to_val != "null" && advances_to_val != "")
        advances_to_ = utils::split_shared(advances_to_val);
    DBG_UT << "unit_type '" << id_ << "' advances to : " << advances_to_val << "\n";

 	experience_needed_=lexical_cast_default<int>(cfg["experience"],500);

	build_status_ = CREATED;
}

const unit_type& unit_type::get_gender_unit_type(unit_race::GENDER gender) const
{
	const size_t i = gender;
	if(i < sizeof(gender_types_)/sizeof(*gender_types_)
	&& gender_types_[i] != NULL) {
		return *gender_types_[i];
	}

	return *this;
}

const unit_type& unit_type::get_variation(const std::string& name) const
{
	const variations_map::const_iterator i = variations_.find(name);
	if(i != variations_.end()) {
		return *i->second;
	} else {
		return *this;
	}
}

const shared_string& unit_type::image_profile() const
{
	if(image_profile_.size() == 0)
		return image_;
	else
		return image_profile_;
}

const t_string unit_type::unit_description() const
{
	if(description_.empty()) {
		return (_(""));
	} else {
		return description_;
	}
}

const std::vector<unit_animation>& unit_type::animations() const {
	if (animations_.empty()) {
		unit_animation::fill_initial_animations(animations_,cfg_);
	}

	return animations_;
}

std::vector<attack_type> unit_type::attacks() const
{
	std::vector<attack_type> res;
	for(config::const_child_itors range = cfg_.child_range("attack");
	    range.first != range.second; ++range.first) {
		res.push_back(attack_type(**range.first));
	}

	return res;
}


namespace {
	int experience_modifier = 100;
}

unit_type::experience_accelerator::experience_accelerator(int modifier) : old_value_(experience_modifier)
{
	experience_modifier = modifier;
}

unit_type::experience_accelerator::~experience_accelerator()
{
	experience_modifier = old_value_;
}

int unit_type::experience_accelerator::get_acceleration()
{
	return experience_modifier;
}

int unit_type::experience_needed(bool with_acceleration) const
{
	if(with_acceleration) {
		int exp = (experience_needed_ * experience_modifier + 50) /100;
		if(exp < 1) exp = 1;
		return exp;
	}
	return experience_needed_;
}



const char* unit_type::alignment_description(unit_type::ALIGNMENT align, unit_race::GENDER gender)
{
	static const char* aligns[] = { N_("lawful"), N_("neutral"), N_("chaotic") };
	static const char* aligns_female[] = { N_("female^lawful"), N_("female^neutral"), N_("female^chaotic") };
	const char** tlist = (gender == unit_race::MALE ? aligns : aligns_female);

	return (sgettext(tlist[align]));
}

const char* unit_type::alignment_id(unit_type::ALIGNMENT align)
{
	static const char* aligns[] = { "lawful", "neutral", "chaotic" };
	return (aligns[align]);
}

bool unit_type::has_ability(const std::string& ability) const
{
	const config* abil = cfg_.child("abilities");
	if(abil) {
		return (abil->get_children(ability).size() > 0);
	}
	return false;
}
bool unit_type::has_ability_by_id(const std::string& ability) const
{
	const config* abil = cfg_.child("abilities");
	if(abil) {
		for(config::child_map::const_iterator i = abil->all_children().begin(); i != abil->all_children().end(); ++i) {
			for(config::child_list::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
				if((**j)["id"] == ability) {
					return true;
				}
			}
		}
	}
	return false;
}

std::vector<shared_string> unit_type::get_ability_list() const
{
	std::vector<shared_string> res;

	const config* abilities = cfg_.child("abilities");
	if (!abilities) return res;

	for(config::child_map::const_iterator i = abilities->all_children().begin(); i != abilities->all_children().end(); ++i) {
		for(config::child_list::const_iterator j = i->second.begin(); j != i->second.end(); ++j) {
			std::string const &id = (**j)["id"];

			if (!id.empty())
				res.push_back(id);
		}
	}

	return res;
}


const shared_string& unit_type::race() const
{
	if(race_ == NULL) {
		static const shared_string empty_string;
		return empty_string;
	}

	return race_->id();
}

// Allow storing "advances from" info for convenience in Help.
void unit_type::add_advancesfrom(const std::string& unit_id)
{
	if (find(advances_from_.begin(), advances_from_.end(), unit_id) == advances_from_.end())
		advances_from_.push_back(unit_id);
}


void unit_type::add_advancement(const unit_type &to_unit,int xp)
{
	const std::string &to_id =  to_unit.cfg_["id"];
	const std::string &from_id =  cfg_["id"];

	// Add extra advancement path to this unit type
	LOG_CONFIG << "adding advancement from " << from_id << " to " << to_id << "\n";
	if(std::find(advances_to_.begin(), advances_to_.end(), to_id) == advances_to_.end()) {
		advances_to_.push_back(to_id);
	} else {
		LOG_CONFIG << "advancement from " << from_id
		           << " to " << to_id << " already known, ignoring.\n";
		return;
	}

	if( xp > 0 && experience_needed_ > xp){
        DBG_UT << "Lowering experience_needed from " << experience_needed_ << " to " << xp << " due to [advancefrom] of " << to_id << "\n";
		experience_needed_ = xp;
	}

	// Add advancements to gendered subtypes, if supported by to_unit
	for(int gender=0; gender<=1; ++gender) {
		if(gender_types_[gender] == NULL) continue;
		if(to_unit.gender_types_[gender] == NULL) {
			WRN_CONFIG << to_id << " does not support gender " << gender << "\n";
			continue;
		}
		LOG_CONFIG << "gendered advancement " << gender << ": ";
		gender_types_[gender]->add_advancement(*(to_unit.gender_types_[gender]),xp);
	}

	// Add advancements to variation subtypes.
	// Since these are still a rare and special-purpose feature,
	// we assume that the unit designer knows what they're doing,
	// and don't block advancements that would remove a variation.
	for(variations_map::iterator v=variations_.begin();
	    v!=variations_.end(); ++v) {
		LOG_CONFIG << "variation advancement: ";
		v->second->add_advancement(to_unit,xp);
	}
}

unit_type_data* unit_type_data::instance_ = NULL;

unit_type_data::unit_type_data() :
	unit_types_()
{
}

unit_type_data::unit_type_map_wrapper::unit_type_map_wrapper() :
	types_(),
	dummy_unit_map_(),
	movement_types_(),
	races_(),
	unit_traits_(),
	unit_cfg_(NULL)
{
    dummy_unit_map_.insert(std::pair<const std::string,unit_type>("dummy_unit", unit_type()));
}

void unit_type_data::unit_type_map_wrapper::set_config(const config& cfg)
{
    DBG_UT << "unit_type_data::set_config, name: " << cfg["name"] << "\n";

    clear();
    set_unit_config(cfg);
	set_unit_traits(cfg.get_children("trait"));

	config::const_child_itors i;
	for(i = cfg.child_range("movetype"); i.first != i.second; ++i.first)
	{
		const unit_movement_type move_type(**i.first);
		movement_types_.insert(
			std::pair<std::string,unit_movement_type>(move_type.name(), move_type));
		increment_set_config_progress();
	}

	for(i = cfg.child_range("race"); i.first != i.second; ++i.first)
	{
		const unit_race race(**i.first);
		races_.insert(std::pair<std::string,unit_race>(race.id(),race));
		increment_set_config_progress();
	}

	for(i = cfg.child_range("unit_type"); i.first != i.second; ++i.first)
	{
	    std::string id = (**i.first)["id"];
		if((**i.first).child("base_unit"))
		{
            // Derive a new unit type from an existing base unit id
			const std::string based_from = (*(**i.first).child("base_unit"))["id"];
			config from_cfg = find_config(based_from);

            config merge_cfg = from_cfg;
            merge_cfg.merge_with(**i.first);
            merge_cfg.clear_children("base_unit");
            std::string id = merge_cfg["id"];
            if(id.empty()) {
                id = from_cfg["name"];
            }

            (**i.first) = merge_cfg;
            (**i.first)["id"] = id;
            /*
            //merge the base_unit config into this one
            (**i.first).merge_and_keep(from_cfg);
            (**i.first).clear_children("base_unit");
            (**i.first)["id"] = id;
            */
		}
        // we insert an empty unit_type and build it after the copy (for performance)
        std::pair<unit_type_map::iterator,bool> insertion =
            insert(std::pair<const std::string,unit_type>(id,unit_type()));
        unit_type_map::iterator itor = types_.find(id);
        //	if (!insertion.second)
        // TODO: else { warning for multiple units with same id}
        LOG_CONFIG << "added " << id << " to unit_type list (unit_type_data.unit_types)\n";
	}

	build_all(unit_type::CREATED);
}

bool unit_type_data::unit_type_map_wrapper::unit_type_exists(const std::string& key) const
{
    if (key.empty() || (key == "random"))
        return false;

    unit_type_map::iterator itor = types_.find(key);

    return ((itor == types_.end()) ? false : true);
}

unit_type_data::unit_type_map::const_iterator unit_type_data::unit_type_map_wrapper::find_unit_type(const std::string& key, unit_type::BUILD_STATUS status) const
{
    if (key.empty() || (key == "random"))
        return types_.end();

    unit_type_map::iterator itor = types_.find(key);

    DBG_CONFIG << "trying to find " << key  << " in unit_type list (unit_type_data.unit_types)\n";

    //This might happen if units of another era are requested (for example for savegames)
    if (itor == types_.end()){
        LOG_CONFIG << "key not found, returning dummy_unit\n";
        /*
        for (unit_type_map::const_iterator ut = types_.begin(); ut != types_.end(); ut++)
            DBG_UT << "Known unit_types: key = '" << ut->first << "', id = '" << ut->second.id() << "'\n";
        */
        itor = dummy_unit_map_.find("dummy_unit");
        assert(itor != dummy_unit_map_.end());
        return itor;
    }

    //check if the unit_type is constructed and build it if necessary
    build_unit_type(key, status);

    return types_.find(key);
}

const config& unit_type_data::unit_type_map_wrapper::find_config(const std::string& key) const
{
    const config* cfg = unit_cfg_->find_child("unit_type", "id", key);

    if (cfg != NULL)
        return *cfg;

    ERR_CONFIG << "unit type not found: " << key << "\n";
    ERR_CONFIG << *unit_cfg_ << "\n";

    ERROR_LOG("unit type not found");
}

void unit_type_data::unit_type_map_wrapper::build_all(unit_type::BUILD_STATUS status) const
{
    assert(unit_cfg_ != NULL);

    for (unit_type_map::const_iterator u = types_.begin(); u != types_.end(); u++){
        build_unit_type(u->first, status);
    }
    for (unit_type_map::iterator u = types_.begin(); u != types_.end(); u++){
        add_advancement(u->second);
    }
}

unit_type& unit_type_data::unit_type_map_wrapper::build_unit_type(const std::string& key, unit_type::BUILD_STATUS status) const
{
    unit_type_map::iterator ut = types_.find(key);

    if (key == "dummy_unit")
        return ut->second;

    DBG_UT << "Building unit type " << ut->first << ", level " << status << "\n";

    switch (status){
        case unit_type::CREATED: {
			const config& unit_cfg = find_config(key);
            ut->second.set_config(unit_cfg);
            ut->second.build_created(unit_cfg, movement_types_, races_, unit_traits_);
            break;
        }
        case unit_type::HELP_INDEX: {
            //build the stuff that is needed to feed the help index
            const config& unit_cfg = find_config(key);
            if ( (ut->second.build_status() == unit_type::NOT_BUILT) || (ut->second.build_status() == unit_type::CREATED) ) {
				ut->second.build_help_index(unit_cfg, movement_types_, races_, unit_traits_);
            }
			add_advancefrom(unit_cfg);
            break;
        }
        case unit_type::WITHOUT_ANIMATIONS:
        case unit_type::FULL: {
            if ( (ut->second.build_status() == unit_type::NOT_BUILT) ||
                (ut->second.build_status() == unit_type::CREATED) ||
                (ut->second.build_status() == unit_type::HELP_INDEX) )
            {
				const config& unit_cfg = find_config(key);
                ut->second.build_full(unit_cfg, movement_types_, races_, unit_traits_);

                if ( (ut->second.build_status() == unit_type::NOT_BUILT) ||
                    (ut->second.build_status() == unit_type::CREATED) )
                    add_advancefrom(unit_cfg);
            }
            break;
        }
        default:
            break;
    }

    return ut->second;
}

void unit_type_data::unit_type_map_wrapper::add_advancefrom(const config& unit_cfg) const
{
    //find the units this one can advance into and add advancefrom information for them
    const std::vector<std::string> advances_to = utils::split(unit_cfg["advances_to"]);
    if ( (advances_to.size() > 0) && (advances_to[0] != "null") ){
        int count = 0;
        for (std::vector<std::string>::const_iterator i_adv = advances_to.begin(); i_adv != advances_to.end(); i_adv++){
            count++;
            DBG_UT << "Unit: " << unit_cfg["id"] << ", AdvanceTo " << count << ": " << *i_adv << "\n";
            unit_type_map::iterator itor_advances_to = types_.find(*i_adv);
            if(itor_advances_to == types_.end()) {
            	// if we can't add the advancefrom information yet, we should
            	// just remember it for later (to prevent infinite recursion)
            	future_advancefroms[*i_adv].insert(unit_cfg["id"]);
            } else {
				itor_advances_to->second.add_advancesfrom(unit_cfg["id"]);
            }
        }
    }
}

void unit_type_data::unit_type_map_wrapper::add_advancement(unit_type& to_unit) const
{
    config::const_child_itors af;
    const config& cfg = to_unit.get_cfg();

    for(af = cfg.child_range("advancefrom"); af.first != af.second; ++af.first)
    {
        const std::string &from = (**af.first)["unit"];
        const int xp = lexical_cast_default<int>((**af.first)["experience"],0);

        unit_type_data::unit_type_map::iterator from_unit = types_.find(from);

        // Fix up advance_from references
        from_unit->second.add_advancement(to_unit, xp);

        DBG_UT << "Added advancement ([advancefrom]) from " << from << " to " << to_unit.id() << "\n";

        // Store what unit this type advances from
		to_unit.add_advancesfrom(from);
    }
}

// This function is only meant to return the likely state of not_living
// for a new recruit of this type. It should not be used to check if
// a particular unit is living or not, use get_state("not_living") for that.
bool unit_type::not_living() const
{
		// If a unit hasn't been modified it starts out as living.
		bool not_living = false;

		// Look at all of the "musthave" traits to see if the not_living
		// status gets changed. In the unlikely event it gets changed
		// multiple times, we want to try to do it in the same order
		// that unit::apply_modifications does things.
		config::child_list const &mods = possible_traits();
		config::child_list::const_iterator j, j_end = mods.end();
		for(j = mods.begin(); j != j_end; ++j) {
			const string_map *vals = &((**j).values);
			string_map::const_iterator temp = vals->find("availability");
			if (temp == vals->end() || (*temp).second != "musthave") {
				continue;
			}
			config::const_child_itors i;
			for(i = (**j).child_range("effect"); i.first != i.second; ++i.first) {

				// See if the effect only applies to
				// certain unit types But don't worry
				// about gender checks, since we don't
				// know what the gender of the
				// hypothetical recruit is.
				vals = &((**i.first).values);
				temp = vals->find("unit_type");
				if(temp != vals->end()) {
					const std::vector<std::string>& types = utils::split((*temp).second);
					if(std::find(types.begin(),types.end(),id()) == types.end()) {
							continue;
					}
				}

				// We're only interested in status changes.
				temp = vals->find("apply_to");
				if (temp == vals->end() || (*temp).second != "status") {
					continue;
				}
				temp = vals->find("add");
				if (temp != vals->end() && (*temp).second == "not_living") {
					not_living = true;
				}
				temp = vals->find("remove");
				if (temp != vals->end() && (*temp).second == "not_living") {
					not_living = false;
				}
			}
		}

		return not_living;
}
