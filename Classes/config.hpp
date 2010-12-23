/* $Id: config.hpp 31858 2009-01-01 10:27:41Z mordante $ */
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
 * @file config.hpp
 * Definitions for the interface to Wesnoth Markup Language (WML).
 *
 * This module defines the interface to Wesnoth Markup Language (WML).  WML is
 * a simple hierarchical text-based file format.  The format is defined in
 * Wiki, under BuildingScenariosWML
 *
 * All configuration files are stored in this format, and data is sent across
 * the network in this format.  It is thus used extensively throughout the
 * game.
 */

#ifndef CONFIG_HPP_INCLUDED
#define CONFIG_HPP_INCLUDED


#include "global.hpp"

#include <boost/shared_ptr.hpp>

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "game_errors.hpp"
#include "tstring.hpp"
#include "serialization/string_utils.hpp"

#include <boost/unordered_map.hpp>

#include "MemFile.h"
//#include "std::stringPool.h"


//extern std::stringPool gstd::stringPool;		// lives in config.cpp

#include "shared_string.hpp"
#include "skiplist_map.hpp"
#include "AssocVector.h"

//typedef std::map<std::string,t_string> string_map;
//typedef std::map<shared_string,shared_string> string_map;
//typedef skiplist_map<shared_string,shared_string> string_map;
typedef AssocVector<shared_string,shared_string> string_map;

class config;

typedef boost::shared_ptr<config> config_ptr;

/** A config object defines a single node in a WML file, with access to child nodes. */
class config
{
public:
	// Create an empty node.
	config();

	config(const config& cfg);

	// Create a config with an empty child of name 'child'.
	config(const shared_string& child);
	~config();

	config& operator=(const config& cfg);

	typedef std::vector<config*> child_list;
	//typedef std::map<std::string,child_list> child_map;
	//typedef std::map<shared_string,child_list> child_map;
	typedef skiplist_map<shared_string, child_list> child_map;
	//typedef AssocVector<shared_string, child_list> child_map;
	
	typedef std::vector<config*>::iterator child_iterator;
	typedef std::vector<config*>::const_iterator const_child_iterator;

	typedef std::pair<child_iterator,child_iterator> child_itors;
	typedef std::pair<const_child_iterator,const_child_iterator> const_child_itors;

	child_itors child_range(const shared_string& key);
	const_child_itors child_range(const shared_string& key) const;
	size_t child_count(const shared_string& key) const;

	const child_list& get_children(const shared_string& key) const;
	const child_map& all_children() const;

	config* child(const shared_string& key);
	const config* child(const shared_string& key) const;
	config& add_child(const shared_string& key);
	config& add_child(const shared_string& key, const config& val);
	config& add_child_at(const shared_string& key, const config& val, size_t index);
	//std::string& operator[](const std::string& key);
	shared_string& operator[](const shared_string& key);
	//const std::string& operator[](const std::string& key) const;
	const shared_string& operator[](const shared_string& key) const;
	

	//const std::string& get_attribute(const std::string& key) const;
	const shared_string& get_attribute(const shared_string& key) const;
	bool has_attribute(const shared_string& key) const 
	{ 
		return values.find(key) != values.end();
	}
	void remove_attribute(const shared_string& key) 
	{ 
		values.erase(key);
	}

	/**
	 * This should only be used if there is no mapping of the key already,
	 * e.g. when creating a new config object. It does not replace an existing value.
	 * @returns true when it added the key-value pair, false if it already existed
	 *          (and you probably wanted to use config[key] = value)
	 */
	bool init_attribute(const shared_string& key, const shared_string& value) 
	{
		//return values.insert(std::pair<std::string, std::string>(key, value)).second;
		return values.insert(std::pair<shared_string, shared_string>(key, value)).second;
	}

	config* find_child(const shared_string& key, const shared_string& name,
	                   const shared_string& value);
	const config* find_child(const shared_string& key, const shared_string& name,
	                         const shared_string& value) const;
	
	void clear_children(const shared_string& key);
	void remove_child(const shared_string& key, size_t index);
	void recursive_clear_value(const shared_string& key);

	void clear();
	bool empty() const;

	std::string debug() const;
	std::string hash() const;

	struct error : public game::error {
		error(const std::string& message) : game::error(message) {}
	};

	struct child_pos {
		child_pos(child_map::const_iterator p, size_t i) : pos(p), index(i) {}
		child_map::const_iterator pos;
		size_t index;

		bool operator==(const child_pos& o) const { return pos == o.pos && index == o.index; }
		bool operator!=(const child_pos& o) const { return !operator==(o); }
	};

	struct all_children_iterator {
		//typedef std::pair<std::string,const config*> value_type;
		typedef std::pair<shared_string,const config*> value_type;
		typedef std::forward_iterator_tag iterator_category;
		typedef int difference_type;
		typedef std::auto_ptr<value_type> pointer;
		typedef value_type& reference;
		typedef std::vector<child_pos>::const_iterator Itor;
		explicit all_children_iterator(Itor i=Itor());

		all_children_iterator& operator++();
		all_children_iterator  operator++(int);

		value_type operator*() const;
		pointer operator->() const;

		//std::string get_key() const;
		shared_string get_key() const;
		size_t get_index() const;
		const config& get_child() const;

		bool operator==(all_children_iterator i) const;
		bool operator!=(all_children_iterator i) const;

	private:
		Itor i_;
	};

	
	/** In-order iteration over all children. */
	all_children_iterator ordered_begin() const;
	all_children_iterator ordered_end() const;
	all_children_iterator erase(const all_children_iterator& i);

	/**
	 * A function to get the differences between this object,
	 * and 'c', as another config object.
	 * I.e. calling cfg2.apply_diff(cfg1.get_diff(cfg2))
	 * will make cfg1 identical to cfg2.
	 */
	config get_diff(const config& c) const;
	void get_diff(const config& c, config& res) const;

	void apply_diff(const config& diff); //throw error

	/**
	 * Merge config 'c' into this config.
	 * Overwrites this config's values.
	 */
	void merge_with(const config& c);

	/**
	 * Merge config 'c' into this config.
	 * Keeps this config's values and does not add existing elements.
	 * NOTICE: other nonstandard behavior includes no merge recursion into child
	 * and has limited merging for child lists of differing lengths
	 */
    void merge_and_keep(const config& c);

	bool matches(const config &filter) const;

	/** Removes keys with empty values. */
	void prune();

	/**
	 * Append data from another config object to this one.
	 * Attributes in the latter config object will clobber attributes in this one.
	 */
	void append(const config& cfg);

	/**
	 * All children with the given key will be merged
	 * into the first element with that key.
	 */
	void merge_children(const shared_string& key);

	/** Resets the translated values of all strings contained in this object */
	void reset_translation() const;

	//this is a cheap O(1) operation
	void swap(config& cfg);

	/** All the attributes of this node. */
	string_map values;

	// KP: new cache functions
	void saveCache(std::string filename) const;
	void saveCache(FILE *file) const;
	void loadCache(std::string filename);
	void loadCache(MEMFILE *file, char *loadBuffer);

private:
	/** A list of all children of this node. */
	child_map children;
	std::vector<child_pos> ordered_children;
};

bool operator==(const config& a, const config& b);
bool operator!=(const config& a, const config& b);
std::ostream& operator << (std::ostream& os, const config& cfg);

#endif
