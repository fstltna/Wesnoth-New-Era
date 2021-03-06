/* $Id: config.cpp 33687 2009-03-15 16:50:42Z mordante $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
   Copyright (C) 2005 - 2009 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file config.cpp
 * Routines related to configuration-files / WML.
 */

#include "global.hpp"

#include "config.hpp"
#include "log.hpp"

#include <cstring>
#include <deque>

extern "C" Uint32 SDL_GetTicks(void);

#include "config_cache.hpp"
#include "loadscreen.hpp"

#define ERR_CF LOG_STREAM(err, config)


extern "C" {
	void* dlmalloc(size_t size);
	void* dlcalloc(size_t count, size_t size);
	void* dlvalloc(size_t size);
	void* dlmemalign(size_t alignment, size_t size);
	void* dlrealloc(void* ptr, size_t size);
	void dlfree(void* ptr);
};

config::config() : values(), children()//, ordered_children()
{
}

config::config(const config& cfg) : values(), children()//, ordered_children()
{
	append(cfg);
}

config::config(const shared_string& child) : values(), children()//, ordered_children()
{
	add_child(child);
}

config::~config()
{
	clear();
}

config& config::operator=(const config& cfg)
{
	if(this == &cfg) {
		return *this;
	}

	clear();

	append(cfg);

	return *this;
}

void config::append(const config& cfg)
{
	for(all_children_iterator i = cfg.ordered_begin(); i != cfg.ordered_end(); ++i) {
		//const all_children_iterator::value_type& value = *i;
		const std::pair<shared_string,const config*>& value = *i;
		add_child(value.first,*value.second);
	}

	for(string_map::const_iterator j = cfg.values.begin(); j != cfg.values.end(); ++j) {
		values[j->first] = j->second;
	}
}

void config::merge_children(const shared_string& key)
{
	config merged_children;
	const child_list& children = get_children(key);
	if(children.size() < 2) {
		return;
	}

	for(child_list::const_iterator i = children.begin(); i != children.end(); ++i) {
		merged_children.append(**i);
	}

	clear_children(key);
	add_child(key,merged_children);
}

config::child_itors config::child_range(const shared_string& key)
{
	child_map::iterator i = children.find(key);
	if(i != children.end()) {
		return child_itors(i->second.begin(),i->second.end());
	} else {
		static std::vector<config*> dummy;
		return child_itors(dummy.begin(),dummy.end());
	}
}

config::const_child_itors config::child_range(const shared_string& key) const
{
	child_map::const_iterator i = children.find(key);
	if(i != children.end()) {
		return const_child_itors(i->second.begin(),i->second.end());
	} else {
		static const std::vector<config*> dummy;
		return const_child_itors(dummy.begin(),dummy.end());
	}
}

size_t config::child_count(const shared_string& key) const
{
	child_map::const_iterator i = children.find(key);
	if(i != children.end()) {
		return i->second.size();
	}
	return 0;
}

const config::child_list& config::get_children(const shared_string& key) const
{
	const child_map::const_iterator i = children.find(key);
	if(i != children.end()) {
		return i->second;
	} else {
		static const child_list dummy;
		return dummy;
	}
}

const config::child_map& config::all_children() const { return children; }

config* config::child(const shared_string& key)
{
	const child_map::const_iterator i = children.find(key);
	if(i != children.end() && i->second.empty() == false) {
		return i->second.front();
	} else {
		return NULL;
	}
}

const config* config::child(const shared_string& key) const
{
	const child_map::const_iterator i = children.find(key);
	if(i != children.end() && i->second.empty() == false) {
		return i->second.front();
	} else {
		return NULL;
	}
}

config& config::add_child(const shared_string& key)
{
	child_list& v = children[key];
	v.push_back(new config());
	ordered_children.push_back(child_pos(children.find(key),v.size()-1));
	return *v.back();
}

config& config::add_child(const shared_string& key, const config& val)
{
	child_list& v = children[key];
	v.push_back(new config(val));
	ordered_children.push_back(child_pos(children.find(key),v.size()-1));
	return *v.back();
}

config& config::add_child_at(const shared_string& key, const config& val, size_t index)
{
	child_list& v = children[key];
	if(index > v.size()) {
		throw error("illegal index to add child at");
	}

	v.insert(v.begin()+index,new config(val));

	bool inserted = false;

	const child_pos value(children.find(key),index);

	std::vector<child_pos>::iterator ord = ordered_children.begin();
	for(; ord != ordered_children.end(); ++ord) {
		if(!inserted && ord->index == index && ord->pos->first == key) {
			ord = ordered_children.insert(ord,value);
			inserted = true;
		} else if(ord->index >= index && ord->pos->first == key) {
			ord->index++;
		}
	}

	if(!inserted) {
		ordered_children.push_back(value);
	}

	return *v[index];
}

namespace {

struct remove_ordered {
	remove_ordered(const shared_string& key) : key_(key) {}

	bool operator()(const config::child_pos& pos) const { return pos.pos->first == key_; }
private:
	shared_string key_;
};

}

void config::clear_children(const shared_string& key)
{
	ordered_children.erase(std::remove_if(ordered_children.begin(),ordered_children.end(),remove_ordered(key)),ordered_children.end());
	
	child_map::iterator i = children.find(key);
	if (i != children.end()) {
		for (child_iterator c = i->second.begin(); c != i->second.end(); c++) {
			delete *c;
		}
		children.erase(i);
	}
}

void config::recursive_clear_value(const shared_string& key)
{
	values.erase(key);
	
	for (all_children_iterator it = ordered_begin(); it != ordered_end(); ++it)
	{
		const_cast<config*>(it->second)->recursive_clear_value(key);
	}
}

config::all_children_iterator config::erase(const config::all_children_iterator& i)
{
	config* found_config = NULL;
	std::vector<child_pos>::iterator erase_pos, j, j_end = ordered_children.end();
	for(j = ordered_children.begin(); j != j_end; ++j) {
		if(i.get_key() == j->pos->first) {
			if(i.get_index() == j->index) {
				erase_pos = j;
				found_config = *(j->pos->second.begin() + j->index);
			} else if(i.get_index() < j->index) {
				//decrement subsequent child indeces of the same key
				j->index--;
			}
		}
	}
	child_list& vec = children[i.get_key()];
	if(!found_config || erase_pos->index >= vec.size()) {
		ERR_CF << "Error: attempting to delete non-existing child: "
			<< i.get_key() << "[" << i.get_index() << "]\n";
		return ordered_end();
	}
	delete found_config;
	vec.erase(vec.begin()+i.get_index());
	return all_children_iterator(ordered_children.erase(erase_pos));
}

void config::remove_child(const shared_string& key, size_t index)
{
	// Remove from the ordering
	const child_pos pos(children.find(key),index);
	ordered_children.erase(std::remove(ordered_children.begin(),ordered_children.end(),pos),ordered_children.end());
	
	// Decrement all indices in the ordering that are above this index,
	// since everything is getting shifted back by 1.
	for(std::vector<child_pos>::iterator i = ordered_children.begin(); i != ordered_children.end(); ++i) {
		if(i->pos->first == key && i->index > index) {
			i->index--;
		}
	}
	
	// Remove from the child map
	child_list& v = children[key];
	if(index >= v.size()) {
		ERR_CF << "Error: attempting to delete non-existing child: "
		<< key << "[" << index << "]\n";
		return;
	}
	config* const res = v[index];
	v.erase(v.begin()+index);
	delete res;
}
/*
shared_string& config::operator[](const std::string& key)
{
	return values[key];
}

const shared_string& config::operator[](const std::string& key) const
{
	return get_attribute(key);
}
*/
//std::string& config::operator[](const std::string& key)
shared_string& config::operator[](const shared_string& key)
{
	return values[key];
}

//const std::string& config::operator[](const std::string& key) const
const shared_string& config::operator[](const shared_string& key) const 
{
	return get_attribute(key);
}


//const std::string& config::get_attribute(const std::string& key) const
const shared_string& config::get_attribute(const shared_string& key) const
{
	const string_map::const_iterator i = values.find(key);
	if(i != values.end()) {
		return i->second;
	} else {
		static const shared_string empty_string;
		//static const std::string empty_string;
		return empty_string;
	}
}


namespace {

struct config_has_value {
	config_has_value(const shared_string& name, const shared_string& value)
	              : name_(name), value_(value)
	{}

	bool operator()(const config* cfg) const { return (*cfg)[name_] == value_; }

private:
	const shared_string name_, value_;
};

} // end namespace

config* config::find_child(const shared_string& key,
                           const shared_string& name,
                           const shared_string& value)
{
	const child_map::iterator i = children.find(key);
	if(i == children.end())
		return NULL;

	const child_list::iterator j = std::find_if(i->second.begin(),
	                                            i->second.end(),
	                                            config_has_value(name,value));
	if(j != i->second.end())
		return *j;
	else
		return NULL;
}

const config* config::find_child(const shared_string& key,
                                 const shared_string& name,
                                 const shared_string& value) const
{
	const child_map::const_iterator i = children.find(key);
	if(i == children.end())
		return NULL;

	const child_list::const_iterator j = std::find_if(
	                                            i->second.begin(),
	                                            i->second.end(),
	                                            config_has_value(name,value));
	if(j != i->second.end())
		return *j;
	else
		return NULL;
}

namespace {
	/**
	 * Helper struct for iterative config clearing.
	 */
	struct config_clear_state
	{
		config_clear_state()
			: c(NULL)
			, mi()
			, vi(0)
		{
		}

		config* c; //the config being inspected
		config::child_map::iterator mi; //current child map entry
		size_t vi; //index into the child map item vector
	};
}

void config::clear()
{
	if (!children.empty()) {
		//start with this node, the first entry in the child map,
		//zeroeth element in that entry
		config_clear_state init;
		init.c = this;
		init.mi = children.begin();
		init.vi = 0;
		std::deque<config_clear_state> l;
		l.push_back(init);

		while (!l.empty()) {
			config_clear_state& state = l.back();
			if (state.mi != state.c->children.end()) {
				std::vector<config*>& v = state.mi->second;
				if (state.vi < v.size()) {
					config* c = v[state.vi];
					++state.vi;
					if (c->children.empty()) {
						delete c; //special case for a slight speed increase?
					} else {
						//descend to the next level
						config_clear_state next;
						next.c = c;
						next.mi = c->children.begin();
						next.vi = 0;
						l.push_back(next);
					}
				} else {
					state.vi = 0;
					++state.mi;
				}
			} else {
				//reached end of child map for this element - all child nodes
				//have beed deleted, so it's safe to clear the map, delete the
				//node and move up one level
				state.c->children.clear();
				if (state.c != this) delete state.c;
				l.pop_back();
			}
		}
	}

	values.clear();
	ordered_children.clear();
}

bool config::empty() const
{
	return children.empty() && values.empty();
}

config::all_children_iterator::all_children_iterator(config::all_children_iterator::Itor i) : i_(i)
{}

config::all_children_iterator& config::all_children_iterator::operator++()
{
	++i_;
	return *this;
}

config::all_children_iterator config::all_children_iterator::operator++(int)
{
	config::all_children_iterator i = *this;
	++i_;
	return i;
}

config::all_children_iterator::value_type config::all_children_iterator::operator*() const
{
	return config::all_children_iterator::value_type((i_->pos->first),i_->pos->second[i_->index]);
}

config::all_children_iterator::pointer config::all_children_iterator::operator->() const
{
	return pointer(new config::all_children_iterator::value_type((i_->pos->first),i_->pos->second[i_->index]));
}

shared_string config::all_children_iterator::get_key() const
//std::string config::all_children_iterator::get_key() const
{
	return i_->pos->first;
}

const config& config::all_children_iterator::get_child() const
{
	return *(i_->pos->second[i_->index]);
}

size_t config::all_children_iterator::get_index() const
{
	return i_->index;
}

bool config::all_children_iterator::operator==(all_children_iterator i) const
{
	return i_ == i.i_;
}

bool config::all_children_iterator::operator!=(all_children_iterator i) const
{
	return i_ != i.i_;
}

config::all_children_iterator config::ordered_begin() const
{
	return all_children_iterator(ordered_children.begin());
}

config::all_children_iterator config::ordered_end() const
{
	return all_children_iterator(ordered_children.end());
}

config config::get_diff(const config& c) const
{
	config res;
	get_diff(c, res);
	return res;
}

void config::get_diff(const config& c, config& res) const
{
	config* inserts = NULL;

	string_map::const_iterator i;
	for(i = values.begin(); i != values.end(); ++i) {
		const string_map::const_iterator j = c.values.find(i->first);
		if(j == c.values.end() || (i->second != j->second && i->second != "")) {
			if(inserts == NULL) {
				inserts = &res.add_child("insert");
			}

			(*inserts)[i->first] = i->second;
		}
	}

	config* deletes = NULL;

	for(i = c.values.begin(); i != c.values.end(); ++i) {
		const string_map::const_iterator itor = values.find(i->first);
		if(itor == values.end() || itor->second == "") {
			if(deletes == NULL) {
				deletes = &res.add_child("delete");
			}

			(*deletes)[i->first] = "x";
		}
	}

	//std::vector<std::string> entities;
	std::vector<shared_string> entities;

	child_map::const_iterator ci;
	for(ci = children.begin(); ci != children.end(); ++ci) {
		entities.push_back(ci->first);
	}

	for(ci = c.children.begin(); ci != c.children.end(); ++ci) {
		if(children.count(ci->first) == 0) {
			entities.push_back(ci->first);
		}
	}

	//for(std::vector<std::string>::const_iterator itor = entities.begin(); itor != entities.end(); ++itor) {
	for(std::vector<shared_string>::const_iterator itor = entities.begin(); itor != entities.end(); ++itor) {
		const child_map::const_iterator itor_a = children.find(*itor);
		const child_map::const_iterator itor_b = c.children.find(*itor);

		static const child_list dummy;

		// Get the two child lists. 'b' has to be modified to look like 'a'.
		const child_list& a = itor_a != children.end() ? itor_a->second : dummy;
		const child_list& b = itor_b != c.children.end() ? itor_b->second : dummy;

		size_t ndeletes = 0;
		size_t ai = 0, bi = 0;
		while(ai != a.size() || bi != b.size()) {
			// If the two elements are the same, nothing needs to be done.
			if(ai < a.size() && bi < b.size() && *a[ai] == *b[bi]) {
				++ai;
				++bi;
			} else {
				// We have to work out what the most appropriate operation --
				// delete, insert, or change is the best to get b[bi] looking like a[ai].
				std::stringstream buf;

				// If b has more elements than a, then we assume this element
				// is an element that needs deleting.
				if(b.size() - bi > a.size() - ai) {
					config& new_delete = res.add_child("delete_child");
					buf << bi - ndeletes;
					new_delete.values["index"] = buf.str();
					new_delete.add_child(*itor);

					++ndeletes;
					++bi;
				}

				// If b has less elements than a, then we assume this element
				// is an element that needs inserting.
				else if(b.size() - bi < a.size() - ai) {
					config& new_insert = res.add_child("insert_child");
					buf << ai;
					new_insert.values["index"] = buf.str();
					new_insert.add_child(*itor,*a[ai]);

					++ai;
				}

				// Otherwise, they have the same number of elements,
				// so try just changing this element to match.
				else {
					config& new_change = res.add_child("change_child");
					buf << bi;
					new_change.values["index"] = buf.str();
					new_change.add_child(*itor,a[ai]->get_diff(*b[bi]));

					++ai;
					++bi;
				}
			}
		}
	}
}

void config::apply_diff(const config& diff)
{
	const config* const inserts = diff.child("insert");
	if(inserts != NULL) {
		for(string_map::const_iterator i = inserts->values.begin(); i != inserts->values.end(); ++i) {
			values[i->first] = i->second;
		}
	}

	const config* const deletes = diff.child("delete");
	if(deletes != NULL) {
		for(string_map::const_iterator i = deletes->values.begin(); i != deletes->values.end(); ++i) {
			values.erase(i->first);
		}
	}

	const child_list& child_changes = diff.get_children("change_child");
	child_list::const_iterator i;
	for(i = child_changes.begin(); i != child_changes.end(); ++i) {
		const size_t index = lexical_cast<size_t>((**i)["index"]);
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const all_children_iterator::value_type item = *j;

			if(item.first == "") {
				continue;
			}

			const child_map::iterator itor = children.find(item.first);
			if(itor == children.end() || index >= itor->second.size()) {
				throw error("error in diff: could not find element '" + std::string(item.first) + "'");
			}

			itor->second[index]->apply_diff(*item.second);
		}
	}

	const child_list& child_inserts = diff.get_children("insert_child");
	for(i = child_inserts.begin(); i != child_inserts.end(); ++i) {
		const size_t index = lexical_cast<size_t>((**i)["index"]);
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const all_children_iterator::value_type item = *j;
			add_child_at(item.first,*item.second,index);
		}
	}

	const child_list& child_deletes = diff.get_children("delete_child");
	for(i = child_deletes.begin(); i != child_deletes.end(); ++i) {
		const size_t index = lexical_cast<size_t>((**i)["index"]);
		for(all_children_iterator j = (*i)->ordered_begin(); j != (*i)->ordered_end(); ++j) {
			const all_children_iterator::value_type item = *j;

			remove_child(item.first,index);
		}
	}
}

void config::merge_with(const config& c)
{
	//std::map<std::string, unsigned> visitations;
	std::map<shared_string, unsigned> visitations;

	// Merge attributes first
	string_map::const_iterator attrib_it, attrib_end = c.values.end();
	for(attrib_it = c.values.begin(); attrib_it != attrib_end; ++attrib_it) {
		values[attrib_it->first] = attrib_it->second;
	}

	// Now merge shared tags
	all_children_iterator::Itor i, i_end = ordered_children.end();
	for(i = ordered_children.begin(); i != i_end; ++i) {
		std::string tag(i->pos->first);
		child_map::const_iterator j = c.children.find(i->pos->first);
		if (j != c.children.end()) {
			unsigned &visits = visitations[tag];
			if(visits < j->second.size()) {
				(i->pos->second[i->index])->merge_with(*j->second[visits++]);
			}
		}
	}

	// Now add any unvisited tags
	for(child_map::const_iterator j = c.children.begin(); j != c.children.end(); ++j) {
		std::string tag(j->first);
		unsigned &visits = visitations[tag];
		while(visits < j->second.size()) {
			add_child(tag, *j->second[visits++]);
		}
	}
}

void config::merge_and_keep(const config& c)
{
	// Merge not existing attributes first
	string_map::const_iterator attrib_it, attrib_end = c.values.end();
	for(attrib_it = c.values.begin(); attrib_it != attrib_end; ++attrib_it) {
	    if (values[attrib_it->first] == "")
            values[attrib_it->first] = attrib_it->second;
	}

	// Now merge unshared tags
	for(child_map::const_iterator i = c.children.begin(); i != c.children.end(); ++i) {
		const shared_string tag = i->first;
		//const std::string tag = i->first;
		child_map::const_iterator j = children.find(tag);
		if (j == children.end()) {
		    for (size_t count=0; count < i->second.size(); count++)
                add_child(tag, *i->second[count]);
		}
	}
}

bool config::matches(const config &filter) const
{
	// First match values. all values should match.
	for(string_map::const_iterator j = filter.values.begin(); j != filter.values.end(); ++j) {
		if(!this->values.count(j->first)) return false;
		const t_string& test_val = this->values.find(j->first)->second;
		if(test_val != j->second) {
			//const std::string& boolcheck = j->second.base_str();
			const std::string& boolcheck = j->second;
			if(boolcheck == "yes" || boolcheck == "true" || boolcheck == "on") {
				if(!utils::string_bool(test_val, false)) {
					return false;
				}
			} else if(boolcheck == "no" || boolcheck == "false" || boolcheck == "off") {
				if(utils::string_bool(test_val, true)) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	// Now, match the kids
	for(all_children_iterator i2 = filter.ordered_begin(); i2 != filter.ordered_end(); ++i2) {
		if((*i2).first == "not") continue;
		child_list interesting_children = get_children((*i2).first);
		bool found = false;
		for(child_list::iterator j2 = interesting_children.begin(); j2 != interesting_children.end(); ++j2) {
			if((*j2)->matches(*(*i2).second)) {
				found = true;
			}
		}
		if(!found) return false;
	}
	child_list negative_children = filter.get_children("not");
	for(child_list::iterator j3 = negative_children.begin() ; j3 != negative_children.end() ; j3++) {
		if(matches(**j3)) return false;
	}
	return true;
}

void config::prune() {
	/*
	string_map::iterator val = values.begin();
	while(val != values.end()) {
		if(val->second.empty()) {
			values.erase(val++);
		} else {
			++val;
		}
	}
	 */
	// KP: with AssocVector, iterators are invalided on erase
	string_map::iterator val = values.begin();
	while(val != values.end()) {
		if(val->second.empty()) {
			values.erase(val++);
			val = values.begin();
		} else {
			++val;
		}
	}
	

	for(child_map::const_iterator list = children.begin(); list != children.end(); ++list) {
		for(child_list::const_iterator child = list->second.begin(); child != list->second.end(); ++child) {
			(*child)->prune();
		}
	}
}

void config::reset_translation() const
{
/*
	for(string_map::const_iterator val = values.begin(); val != values.end(); ++val) {
		val->second.reset_translation();
	}

	for(child_map::const_iterator list = children.begin(); list != children.end(); ++list) {
		for(child_list::const_iterator child = list->second.begin();
				child != list->second.end(); ++child) {
			(*child)->reset_translation();
		}
	}
*/ 
}

std::string config::debug() const
{
	std::ostringstream outstream;
	outstream << *this;
	return outstream.str();
}

std::ostream& operator << (std::ostream& outstream, const config& cfg) {
	static int i = 0;
	i++;
	for(string_map::const_iterator val = cfg.values.begin(); val != cfg.values.end(); ++val) {
		for (int j = 0; j < i-1; j++){ outstream << char(9); }
		outstream << val->first << " = ";
		outstream << "\"" << val->second << "\"\n";
	}
	for(config::all_children_iterator list = cfg.ordered_begin(); list != cfg.ordered_end(); ++list) {
		{ for (int j = 0; j < i-1; j++){ outstream << char(9); } }
		outstream << "[" << (*list).first << "]\n";
		outstream << *(*list).second;
		{ for (int j = 0; j < i-1; j++){ outstream << char(9); } }
		outstream << "[/" << (*list).first << "]\n";
	}
	i--;
    return outstream;
}

void config::saveCache(FILE *outfile) const
{
	short numValues = values.size();
	fwrite(&numValues, sizeof(short), 1, outfile);
	
	for(string_map::const_iterator val = values.begin(); val != values.end(); ++val) 
	{
		cacheSaveString(outfile, val->first);
		cacheSaveString(outfile, val->second);
	}
	

	short numChildren = 0;
	for(config::all_children_iterator list = ordered_begin(); list != ordered_end(); ++list)
	{
		numChildren++;
	}
	fwrite(&numChildren, sizeof(short), 1, outfile);
	for(config::all_children_iterator list = ordered_begin(); list != ordered_end(); ++list) 
	{
		cacheSaveString(outfile, (*list).first);

		// recursive call for the children
		(*list).second->saveCache(outfile);
	}
}

void config::loadCache(std::string filename)
{
	unsigned long startTicks = SDL_GetTicks();
	
	char *loadBuffer = (char *) malloc(80 * 1024);
	
	{
		std::string newFilename = filename + ".str.dat";
		unsigned char *data = cacheUncompress(newFilename);
		MEMFILE mf(data);
		cacheLoadStringTable(&mf, loadBuffer);
		dlfree(data);
	}
	
	{
		std::string newFilename = filename + ".cache.dat";
		unsigned char *data = cacheUncompress(newFilename);
		MEMFILE mf(data);	
		loadCache(&mf, loadBuffer);
		dlfree(data);
	}
	
	free(loadBuffer);
	cacheFreeStringTable();

	unsigned long endTicks = SDL_GetTicks();
	std::cerr << "\nTook " << endTicks - startTicks << " ms to read cached file " << filename << "\n";	
}

void config::saveCache(std::string filename) const
{
	unsigned long startTicks = SDL_GetTicks();
	
	{
		std::string newFilename = filename + ".cache.dat";
		FILE *outfile = fopen(newFilename.c_str(), "wb");
		saveCache(outfile);
		fclose(outfile);
		
		cacheCompress(newFilename);
	}
	
	{
		std::string newFilename = filename + ".str.dat";
		FILE *outfile = fopen(newFilename.c_str(), "wb");
		cacheSaveStringTable(outfile);
		fclose(outfile);
		cacheCompress(newFilename);
	}

	cacheFreeStringTable();
	
	unsigned long endTicks = SDL_GetTicks();
	std::cerr << "\nTook " << endTicks - startTicks << " ms to SAVE cached file " << filename << "\n";		
}

void config::loadCache(MEMFILE *infile, char *loadBuffer)
{
	increment_parser_progress();
	
	short numValues;
	mread(&numValues, sizeof(short), 1, infile);

	char *str;
	unsigned long strSize;
	values.reserve(numValues);
	//values.rehash(numValues);
//	std::string key;
//	std::string value;
//	std::map<shared_string, shared_string>::iterator it = values.begin();	// for fast insertion
	for(int i=0; i < numValues; i++)
	{
		cacheLoadString(infile, &str, &strSize);
		gTempStr.assign(str, strSize);

		cacheLoadString(infile, &str, &strSize);
		gTempStr2.assign(str, strSize);
		
		//values.insert(std::pair<std::string, std::string>(gTempStr, gTempStr2));
//		it = values.insert(it, std::pair<shared_string, shared_string>(shared_string(gTempStr), shared_string(gTempStr2)));
		values.insert(std::pair<shared_string, shared_string>(shared_string(gTempStr), shared_string(gTempStr2)));
	}
	
	short numChildren;
	mread(&numChildren, sizeof(short), 1, infile);
	ordered_children.reserve(numChildren);
//	std::string name;
	for(short i=0; i < numChildren; i++) 
	{		
		cacheLoadString(infile, &str, &strSize);
		gTempStr.assign(str, strSize);
	
		config *cfg = new config();
				
		child_list& v = children[gTempStr];
		v.push_back(cfg);
		//ordered_children.push_back(child_pos(children.find(key),v.size()-1));
		ordered_children.push_back(child_pos(children.find(gTempStr),v.size()-1));
		
		// recursive call for the children
		cfg->loadCache(infile, loadBuffer);

	}
}

std::string config::hash() const
{
	static const unsigned int hash_length = 128;
	static const char hash_string[] =
		"+-,.<>0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char hash_str[hash_length + 1];
	std::string::const_iterator c;

	unsigned int i;
	for(i = 0; i != hash_length; ++i) {
		hash_str[i] = 'a';
	}
	hash_str[hash_length] = 0;

	i = 0;
	for(string_map::const_iterator val = values.begin(); val != values.end(); ++val) {
		//if(val->first != 0 && val->second.size()) {
		if(val->first != "" && val->second.size()) {
			std::string firstStr(val->first);
			for(c = firstStr.begin(); c != firstStr.end(); ++c) {
				if(utils::portable_isspace(*c)) {
					continue;
				}
				hash_str[i] ^= *c;
				++i;
				if(i == hash_length) {
					i = 0;
				}
			}
			//const std::string &base_str = val->second.base_str();
			const std::string &base_str = val->second;
			for(c = base_str.begin(); c != base_str.end(); ++c) {
				if(utils::portable_isspace(*c)) {
					continue;
				}
				hash_str[i] ^= *c;
				++i;
				if(i == hash_length) {
					i = 0;
				}
			}
		}
	}
	for(child_map::const_iterator list = children.begin(); list != children.end(); ++list) {
		for(child_list::const_iterator child = list->second.begin(); child != list->second.end(); ++child) {
			std::string child_hash = (*child)->hash();
			for(c = child_hash.begin(); c != child_hash.end(); ++c) {
				hash_str[i] ^= *c;
				++i;
				if(i == hash_length) {
					i = 0;
				}
			}
		}
	}

	for(i = 0; i != hash_length; ++i) {
		hash_str[i] = hash_string[
			static_cast<unsigned>(hash_str[i]) % strlen(hash_string)];
	}

	return std::string(hash_str);
}

void config::swap(config& cfg)
{
	values.swap(cfg.values);
	children.swap(cfg.children);
	ordered_children.swap(cfg.ordered_children);
}

bool operator==(const config& a, const config& b)
{
	if (a.values != b.values)
		return false;

	config::all_children_iterator x = a.ordered_begin(), y = b.ordered_begin();
	while(x != a.ordered_end() && y != b.ordered_end()) {
		const config::all_children_iterator::value_type val1 = *x;
		const config::all_children_iterator::value_type val2 = *y;

		if(val1.first != val2.first || *val1.second != *val2.second) {
			return false;
		}

		++x;
		++y;
	}

	return x == a.ordered_end() && y == b.ordered_end();
}

bool operator!=(const config& a, const config& b)
{
	return !operator==(a,b);
}

//#define TEST_CONFIG

#ifdef TEST_CONFIG

int main()
{
	config cfg(read_file("testconfig"));
	std::cout << cfg.write() << std::endl;
}

#endif
