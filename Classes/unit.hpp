/* $Id: unit.hpp 36125 2009-06-10 18:31:24Z soliton $ */
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

/** @file unit.hpp */

#ifndef UNIT_H_INCLUDED
#define UNIT_H_INCLUDED

#include "config.hpp"
#include "formula_callable.hpp"
#include "map_location.hpp"
#include "portrait.hpp"
#include "race.hpp"
#include "team.hpp"
#include "unit_types.hpp"
#include "unit_map.hpp"
#include "variable.hpp"

class game_display;
class gamestatus;
class game_state;
class config_writer;

#include <set>
#include <string>
#include <vector>

class unit_ability_list
{
public:
	unit_ability_list() :
		cfgs()
	{
	}

	bool empty() const;

	std::pair<int,map_location> highest(const std::string& key, int def=0) const;
	std::pair<int,map_location> lowest(const std::string& key, int def=100) const;

	std::vector<std::pair<config*,map_location> > cfgs;
};


class unit
{
public:
	/**
	 * Clear the unit status cache for all units. Currently only the hidden
	 * status of units is cached this way.
	 */
	static void clear_status_caches();

	friend struct unit_movement_resetter;
	// Copy constructor
	unit(const unit& u);
	/** Initilizes a unit from a config */
	unit(const config& cfg, bool use_traits=false);
	unit(unit_map* unitmap, const gamemap* map,
		const gamestatus* game_status, const std::vector<team>* teams,
		const config& cfg, bool use_traits=false, game_state* state = 0);
	/** Initializes a unit from a unit type */
	unit(const unit_type* t, int side, bool use_traits=false, bool dummy_unit=false, unit_race::GENDER gender=unit_race::MALE, std::string variation="");
	unit(unit_map* unitmap, const gamemap* map, const gamestatus* game_status, const std::vector<team>* teams, const unit_type* t, int side, bool use_traits=false, bool dummy_unit=false, unit_race::GENDER gender=unit_race::MALE, std::string variation="", bool force_gender=false);
	virtual ~unit();
	unit& operator=(const unit&);

	void set_game_context(unit_map* unitmap, const gamemap* map, const gamestatus* game_status, const std::vector<team>* teams);

	/** Advances this unit to another type */
	void advance_to(const unit_type* t, bool use_traits=false, game_state* state = 0);
	const std::vector<shared_string> advances_to() const { return advances_to_; }

	/** The type id of the unit */
	const std::string& type_id() const { return type_; }
	const unit_type* type() const;

	/** id assigned by wml */
	const std::string& id() const { if (id_.empty()) return type_name(); else return id_; }
	/** The unique internal ID of the unit */
	size_t underlying_id() const { return underlying_id_; }

	/** The unit type name */
	const t_string& type_name() const {return type_name_;}
	const std::string& undead_variation() const {return undead_variation_;}

	/** The unit name for display */
	const t_string &name() const { return name_; }
	void rename(const std::string& name) {if (!unrenamable_) name_= name;}

	/** The unit's profile */
	//const std::string& profile() const;
	const shared_string& profile() const;
	/** Information about the unit -- a detailed description of it */
	//const std::string& unit_description() const { return cfg_["description"]; }
	const std::string unit_description() const { return cfg_["description"]; }
	//const shared_string& unit_description() const { return cfg_["description"]; }

	int hitpoints() const { return hit_points_; }
	int max_hitpoints() const { return max_hit_points_; }
	int experience() const { return experience_; }
	int max_experience() const { return max_experience_; }
	int level() const { return level_; }
	/**
	 * Adds 'xp' points to the units experience; returns true if advancement
	 * should occur
	 */
	bool get_experience(int xp) { experience_ += xp; return advances(); }
	/** Colors for the unit's hitpoints. */
	SDL_Colour hp_color() const;
	/** Colors for the unit's XP. */
	SDL_Colour xp_color() const;
	/** Set to true for some scenario-specific units which should not be renamed */
	bool unrenamable() const { return unrenamable_; }
	unsigned int side() const { return side_; }
	std::string side_id() const {return teams_manager::get_teams()[side()-1].save_id(); }
	Uint32 team_rgb() const { return(team::get_side_rgb(side())); }
	const std::string& team_color() const { return flag_rgb_; }
	unit_race::GENDER gender() const { return gender_; }
	void set_side(unsigned int new_side) { side_ = new_side; }
	fixed_t alpha() const { return alpha_; }

	bool can_recruit() const { return utils::string_bool(cfg_["canrecruit"]); }
	bool incapacitated() const { return utils::string_bool(get_state("stoned"),false); }
	const std::vector<shared_string>& recruits() const { return recruits_; }
	int total_movement() const { return max_movement_; }
	int movement_left() const { return (movement_ == 0 || incapacitated()) ? 0 : movement_; }
	void set_hold_position(bool value) { hold_position_ = value; }
	bool hold_position() const { return hold_position_; }
	void set_user_end_turn(bool value=true) { end_turn_ = value; }
	bool user_end_turn() const { return end_turn_; }
	int attacks_left() const { return (attacks_left_ == 0 || incapacitated()) ? 0 : attacks_left_; }
	void set_movement(int moves);
	void set_attacks(int left) { attacks_left_ = std::max<int>(0,std::min<int>(left,max_attacks_)); }
	void unit_hold_position() { hold_position_ = end_turn_ = true; }
	void new_turn();
	void end_turn();
	void new_scenario();
	/** Called on every draw */
	void refresh(const game_display& disp,const map_location& loc) {
		if (state_ == STATE_FORGET  && anim_ && anim_->animation_finished_potential()) {
			set_standing( loc);
			return;
		}
		if (state_ != STATE_STANDING || (get_current_animation_tick() < next_idling_) || incapacitated()) return;
		if (get_current_animation_tick() > next_idling_ + 1000) { // prevent all units animating at the same
			set_standing(loc);
		} else {
			set_idling(disp, loc);
		}
	}

	bool take_hit(int damage) { hit_points_ -= damage; return hit_points_ <= 0; }
	void heal(int amount);
	void heal_all() { hit_points_ = max_hitpoints(); }
	bool resting() const { return resting_; }
	void set_resting(bool rest) { resting_ = rest; }

	const std::map<shared_string,shared_string>& get_states() const;
	std::string get_state(const std::string& state) const;
	void set_state(const std::string& state, const std::string& value);

	bool has_moved() const { return movement_left() != total_movement(); }
	bool has_goto() const { return get_goto().valid(); }
	bool emits_zoc() const { return emit_zoc_ && !incapacitated();}
	/* cfg: standard unit filter */
	bool matches_filter(const vconfig& cfg,const map_location& loc,bool use_flat_tod=false) const;
	void add_overlay(const std::string& overlay) { overlays_.push_back(overlay); }
	void remove_overlay(const std::string& overlay) { overlays_.erase(std::remove(overlays_.begin(),overlays_.end(),overlay),overlays_.end()); }
	const std::vector<shared_string>& overlays() const { return overlays_; }

	/**
	 * Initialize this unit from a cfg object.
	 *
	 * @param cfg                 Configuration object from which to read the unit.
	 * @param use_traits          ??
	 * */
	void read(const config& cfg, bool use_traits=true, game_state* state = 0);
	void write(config& cfg) const;
//	void write(config_writer& out) const;

	void assign_role(const std::string& role) { role_ = role; }
	void assign_ai_special(const std::string& s) { ai_special_ = s;}
            std::string get_ai_special() const { return(ai_special_); }
	const std::vector<attack_type>& attacks() const { return attacks_; }
	std::vector<attack_type>& attacks() { return attacks_; }

	int damage_from(const attack_type& attack,bool attacker,const map_location& loc) const { return resistance_against(attack,attacker,loc); }

	/** A SDL surface, ready for display for place where we need a still-image of the unit. */
	const surface still_image(bool scaled = false) const;

	/** draw a unit.  */
	void redraw_unit(game_display& disp, const map_location& loc);
	/** Clear unit_halo_  */
	void clear_haloes();


	void set_standing(const map_location& loc, bool with_bars = true);
	void set_idling(const game_display& disp,const map_location& loc);
	void set_selecting(const game_display& disp,const map_location& loc);
	unit_animation* get_animation() {  return anim_;};
	const unit_animation* get_animation() const {  return anim_;};
	void set_facing(map_location::DIRECTION dir);
	map_location::DIRECTION facing() const { return facing_; }

	bool invalidate(const map_location &loc);
	const t_string& traits_description() const { return traits_description_; }
	std::vector<std::string> get_traits_list() const;

	int cost () const { return unit_value_; }

	const map_location& get_goto() const { return goto_; }
	void set_goto(const map_location& new_goto) { goto_ = new_goto; }

	int upkeep() const;
	bool loyal() const {return cfg_["upkeep"]=="loyal"; }

	void set_hidden(bool state);
	bool get_hidden() const { return hidden_; }
	bool is_flying() const { return flying_; }
	bool is_fearless() const { return is_fearless_; }
	bool is_healthy() const { return is_healthy_; }
	int movement_cost(const t_translation::t_terrain terrain) const;
	int defense_modifier(t_translation::t_terrain terrain, int recurse_count=0) const;
	int resistance_against(const std::string& damage_name,bool attacker,const map_location& loc) const;
	int resistance_against(const attack_type& damage_type,bool attacker,const map_location& loc) const
		{return resistance_against(damage_type.type(), attacker, loc);};

	//return resistances without any abililities applied
	string_map get_base_resistances() const;
//		std::map<terrain_type::TERRAIN,int> movement_type() const;

	bool can_advance() const { return advances_to_.empty()==false || get_modification_advances().empty() == false; }
	bool advances() const { return experience_ >= max_experience() && can_advance(); }

    std::map<std::string,std::string> advancement_icons() const;
    std::vector<std::pair<std::string,std::string> > amla_icons() const;

	config::child_list get_modification_advances() const;
	const config::child_list& modification_advancements() const { return cfg_.get_children("advancement"); }

	size_t modification_count(const std::string& type, const std::string& id) const;

	void add_modification(const std::string& type, const config& modification,
	                  bool no_add=false);

	//const t_string& modification_description(const std::string& type) const;
	const shared_string& modification_description(const std::string& type) const;

	bool move_interrupted() const { return movement_left() > 0 && interrupted_move_.x >= 0 && interrupted_move_.y >= 0; }
	const map_location& get_interrupted_move() const { return interrupted_move_; }
	void set_interrupted_move(const map_location& interrupted_move) { interrupted_move_ = interrupted_move; }

	/** States for animation. */
	enum STATE {
		STATE_STANDING,   /** anim must fit in a hex */
		STATE_FORGET,     /** animation will be automaticaly replaced by a standing anim when finished */
		STATE_ANIM};      /** normal anims */
	void start_animation(const int start_time , const map_location &loc,const unit_animation* animation, bool with_bars,bool cycles=false,const std::string text = "", const Uint32 text_color =0,STATE state = STATE_ANIM);

	/** The name of the file to game_display (used in menus). */
	//const std::string& absolute_image() const { return cfg_["image"]; }
	const shared_string& absolute_image() const { return cfg_["image"]; }
	//const std::string& image_halo() const { return cfg_["halo"]; }
	const shared_string& image_halo() const { return cfg_["halo"]; }

	//const std::string& get_hit_sound() const { return cfg_["get_hit_sound"]; }
	const shared_string& get_hit_sound() const { return cfg_["get_hit_sound"]; }
	//const std::string& image_ellipse() const { return cfg_["ellipse"]; }
	const shared_string& image_ellipse() const { return cfg_["ellipse"]; }

	//const std::string& usage() const { return cfg_["usage"]; }
	const shared_string& usage() const { return cfg_["usage"]; }
	unit_type::ALIGNMENT alignment() const { return alignment_; }
	const unit_race* race() const { return race_; }

	const unit_animation* choose_animation(const game_display& disp, const map_location& loc,const std::string& event,const int damage=0,const unit_animation::hit_type hit_type = unit_animation::INVALID,const attack_type* attack=NULL,const attack_type* second_attack = NULL, int swing_num =0) const;

	bool get_ability_bool(const std::string& ability, const map_location& loc) const;
	unit_ability_list get_abilities(const std::string& ability, const map_location& loc) const;
	std::vector<std::string> ability_tooltips(const map_location& loc) const;
	std::vector<std::string> unit_ability_tooltips() const;
	std::vector<std::string> get_ability_list() const;
	bool has_ability_type(const std::string& ability) const;
	bool abilities_affects_adjacent() const;

	const game_logic::map_formula_callable_ptr& formula_vars() const { return formula_vars_; }
	void add_formula_var(std::string str, variant var);
	bool has_formula() const { return !unit_formula_.empty(); }
	bool has_loop_formula() const { return !unit_loop_formula_.empty(); }
	bool has_priority_formula() const { return !unit_priority_formula_.empty(); }
	bool has_on_fail_formula() const { return !unit_on_fail_formula_.empty(); }
	const std::string& get_formula() const { return unit_formula_; }
	const std::string& get_loop_formula() const { return unit_loop_formula_; }
	const std::string& get_priority_formula() const { return unit_priority_formula_; }
	const std::string& get_on_fail_formula() const { return unit_on_fail_formula_; }

	void reset_modifications();
	void backup_state();
	void apply_modifications();
	void remove_temporary_modifications();
	void generate_traits(bool musthaveonly=false, game_state* state = 0);
	void generate_traits_description();
	std::string generate_name( rand_rng::simple_rng* rng = 0) const
		{ return race_->generate_name(string_gender(cfg_["gender"]), rng); }

	// Only see_all=true use caching
	bool invisible(const map_location& loc,
		const unit_map& units,const std::vector<team>& teams, bool see_all=true) const;

	/** Mark this unit as clone so it can be insterted to unit_map
	 * @returns                   self (for convenience)
	 **/
	unit& clone(bool is_temporary=true);

	/**
	 * Make sure that invisibility cache is revalidate
	 * after ambush;
	 **/
	void ambush() const;

	unit_race::GENDER generate_gender(const unit_type& type, bool gen, game_state* state = 0);
	shared_string image_mods() const;

	/**
	 * Gets the portrait for a unit.
	 *
	 * @param size                The size of the portrait.
	 * @param side                The side the portrait is shown on.
	 *
	 * @returns                   The portrait with the wanted size.
	 * @retval NULL               The wanted portrait doesn't exist.
	 */
	const tportrait* portrait(
		const unsigned size, const tportrait::tside side) const;

private:

	bool internal_matches_filter(const vconfig& cfg,const map_location& loc,
		bool use_flat_tod) const;
	/*
	 * cfg: an ability WML structure
	 */
	bool ability_active(const std::string& ability,const config& cfg,const map_location& loc) const;
	bool ability_affects_adjacent(const std::string& ability,const config& cfg,int dir,const map_location& loc) const;
	bool ability_affects_self(const std::string& ability,const config& cfg,const map_location& loc) const;
	bool resistance_filter_matches(const config& cfg,bool attacker,const std::string& damage_name) const;
	bool resistance_filter_matches(const config& cfg,bool attacker,const attack_type& damage_type) const
	{return resistance_filter_matches(cfg, attacker, damage_type.type()); };

	int movement_cost_internal(t_translation::t_terrain terrain, int recurse_count=0) const;
	bool has_ability_by_id(const std::string& ability) const;
	void remove_ability_by_id(const std::string& ability);

	void set_underlying_id();

	// KP: why does this have it's own config? can't it point to the base unit data cfg??
	config cfg_;

	std::vector<shared_string> advances_to_;
	shared_string type_;
	const unit_race* race_;
	shared_string id_;
	shared_string name_;
	size_t underlying_id_;
	shared_string type_name_;
	shared_string undead_variation_;
	shared_string variation_;

	// KP: all int -> short, saved 
	short hit_points_;
	short max_hit_points_;
	short experience_;
	short max_experience_;
	short level_;
	unit_type::ALIGNMENT alignment_;
	shared_string flag_rgb_;
	shared_string image_mods_;

	bool unrenamable_;
	unsigned short side_;
	unit_race::GENDER gender_;

	fixed_t alpha_;

	shared_string unit_formula_;
	shared_string unit_loop_formula_;
	shared_string unit_priority_formula_;
	shared_string unit_on_fail_formula_;
	game_logic::map_formula_callable_ptr formula_vars_;

	std::vector<shared_string> recruits_;

	short movement_;
	short max_movement_;
	mutable std::map<t_translation::t_terrain, int> movement_costs_; // movement cost cache
	mutable std::map<t_translation::t_terrain, int> defense_mods_; // defense modifiers cache
	bool hold_position_;
	bool end_turn_;
	bool resting_;
	short attacks_left_;
	short max_attacks_;

	std::map<shared_string,shared_string> states_;
	bool slowed_;	// KP: added for optimization
	config variables_;
	short emit_zoc_;
	STATE state_;

	std::vector<shared_string> overlays_;

	shared_string role_;
	shared_string ai_special_;
	std::vector<attack_type> attacks_;
	map_location::DIRECTION facing_;

	shared_string traits_description_;
	short unit_value_;
	map_location goto_, interrupted_move_;
	short flying_, is_fearless_, is_healthy_;

	string_map modification_descriptions_;
	// Animations:
	std::vector<unit_animation> animations_;

	unit_animation *anim_;
	int next_idling_;
	int frame_begin_time_;


	short unit_halo_;
	bool getsHit_;
	bool refreshing_; // avoid infinite recursion
	bool hidden_;
	bool draw_bars_;

	config modifications_;

	friend void attack_type::set_specials_context(const map_location& loc, const map_location&, const unit& un, bool) const;
	const unit_map* units_;
	const gamemap* map_;
	const gamestatus* gamestatus_;

	/** Hold the visibility status cache for a unit, mutable since it's a cache. */
	mutable std::map<map_location, bool> invisibility_cache_;

	/**
	 * Clears the cache.
	 *
	 * Since we don't change the state of the object we're marked const (also
	 * required since the objects in the cache need to be marked const).
	 */
	void clear_visibility_cache() const { invisibility_cache_.clear(); }
};

/** Object which temporarily resets a unit's movement */
struct unit_movement_resetter
{
	unit_movement_resetter(unit& u, bool operate=true) : u_(u), moves_(u.movement_)
	{
		if(operate) {
			u.movement_ = u.total_movement();
		}
	}

	~unit_movement_resetter()
	{
		u_.movement_ = moves_;
	}

private:
	unit& u_;
	int moves_;
};

void sort_units(std::vector< unit > &);

/** Returns the number of units of the given side (team). */
int team_units(const unit_map& units, unsigned int team_num);
int team_upkeep(const unit_map& units, unsigned int team_num);
unit_map::const_iterator team_leader(unsigned int side, const unit_map& units);
unit_map::iterator find_visible_unit(unit_map& units,
		const map_location loc,
		const gamemap& map,
		const std::vector<team>& teams, const team& current_team,
		bool see_all=false);
unit_map::const_iterator find_visible_unit(const unit_map& units,
		const map_location loc,
		const gamemap& map,
		const std::vector<team>& teams, const team& current_team,
		bool see_all=false);

struct team_data
{
	team_data() :
		units(0),
		upkeep(0),
		villages(0),
		expenses(0),
		net_income(0),
		gold(0),
		teamname()
	{
	}

	int units, upkeep, villages, expenses, net_income, gold;
	std::string teamname;
};

team_data calculate_team_data(const class team& tm, int side, const unit_map& units);

/**
 * This object is used to temporary place a unit in the unit map, swapping out
 * any unit that is already there.  On destruction, it restores the unit map to
 * its original.
 */
struct temporary_unit_placer
{
	temporary_unit_placer(unit_map& m, const map_location& loc, unit& u);
	~temporary_unit_placer();

private:
	unit_map& m_;
	const map_location& loc_;
	std::pair<map_location,unit> *temp_;
};

/**
 * Gets a checksum for a unit.
 *
 * In MP games the descriptions are locally generated and might differ, so it
 * should be possible to discard them.  Not sure whether replays suffer the
 * same problem.
 *
 *  @param u                    the unit
 *
 *  @returns                    the checksum for a unit
 */
std::string get_checksum(const unit& u);

#endif
