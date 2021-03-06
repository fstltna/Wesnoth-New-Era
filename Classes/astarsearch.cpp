/* $Id: astarsearch.cpp 35548 2009-05-10 17:16:36Z alink $ */
/*
   Copyright (C) 2003 by David White <dave@whitevine.net>
                 2005 - 2009 by Guillaume Melquiond <guillaume.melquiond@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "global.hpp"

#include "astarnode.hpp"
#include "log.hpp"
#include "map.hpp"
#include "pathfind.hpp"


#define LOG_PF LOG_STREAM(info, engine)
#define DBG_PF LOG_STREAM(debug, engine)

typedef std::vector<map_location> vector_location;
typedef std::vector<a_star_node*> vector_a_star_node;
typedef std::set<map_location> set_location;

// heaps give the biggest element for free, so we want the biggest element to
// have the smallest cost
static bool compare_lt_a_star_node(const a_star_node* node1, const a_star_node* node2) {
	return node1->g + node1->h > node2->g + node2->h;
}

static void a_star_init(map_location const &src, map_location const &dst,
                        vector_a_star_node &openList, a_star_world &aStarGameWorld,
                        const size_t parWidth, const size_t parHeight,
                        vector_location &vectLocation, std::set<map_location> const *teleports,
                        size_t &parNbTeleport)
{
	bool locIsCreated;

	aStarGameWorld.resize_IFN(parWidth, parHeight);
	a_star_node *locStartNode = aStarGameWorld.getNodeFromLocation(src, locIsCreated);
	assert(locIsCreated);
	locStartNode->initNode(src, dst, 0.0, NULL, teleports);

	const size_t locValueH = size_t(locStartNode->h);
	size_t locAllocSize;

	if (locValueH < 16)
		locAllocSize = 16;
	else if (locValueH > 128)
		locAllocSize = 128;
	else
		locAllocSize = locValueH;
	openList.reserve(locAllocSize);
	openList.push_back(locStartNode);

	if (teleports != NULL)
		parNbTeleport = teleports->size();
	else
		parNbTeleport = 0;

	vectLocation.reserve(parNbTeleport + 6);
	vectLocation.resize(parNbTeleport + 6);

	if (parNbTeleport > 0)
		std::copy(teleports->begin(), teleports->end(), &vectLocation[6]);
}

static void a_star_explore_neighbours(map_location const &dst, const double stop_at,
                                      cost_calculator const *costCalculator,
                                      const size_t parWidth, const size_t parHeight,
                                      std::set<map_location> const *teleports,
                                      vector_location &vectLocation, vector_a_star_node &openList,
                                      a_star_world &aStarGameWorld,
                                      a_star_node *parCurNode, const size_t parNbTeleport)
{
	typedef std::pair<vector_a_star_node::iterator, vector_a_star_node::iterator> pair_node_iter;

	a_star_node *locNextNode;
	double locCost;
	pair_node_iter locPlace;
	size_t locSize;
	bool locIsCreated;
	const double locCostFather = parCurNode->g;

	get_adjacent_tiles(parCurNode->loc, &vectLocation[0]);

	if (parNbTeleport > 0 && teleports->count(parCurNode->loc) > 0)
		locSize = parNbTeleport + 6;
	else
		locSize = 6;

	bool broken_heap = false;
	int locNbAdded = 0;

	for (size_t i = 0; i != locSize; ++i)
	{
		const map_location&  locLocation = vectLocation[i];

		if (locLocation.valid(int(parWidth), int(parHeight)) == false)
			continue;
		locNextNode = aStarGameWorld.getNodeFromLocation(locLocation, locIsCreated);
		if (locIsCreated) {
			locCost = locCostFather + costCalculator->cost(parCurNode->loc,locLocation, locCostFather);

			locNextNode->initNode(locLocation, dst, locCost, parCurNode, teleports);
			if (locNextNode->g + locNextNode->h < stop_at) {
				openList.push_back(locNextNode);
				++locNbAdded;
			} else
				locNextNode->isInCloseList = true;

		} else if (locCostFather + 1 < locNextNode->g) { // cost() always >= 1 (see below)
			locCost = locCostFather + costCalculator->cost(parCurNode->loc,locLocation, locCostFather);

			if (locCost < locNextNode->g) {
				if (locNextNode->isInCloseList) {
					locNextNode->isInCloseList = false;
					openList.push_back(locNextNode);
					++locNbAdded;
				} else
					broken_heap = true;

				locNextNode->g = locCost;
				locNextNode->nodeParent = parCurNode;
			}
		}
	}

	vector_a_star_node::iterator openList_begin = openList.begin(),
	                             openList_end = openList.end();
	if (broken_heap)
		std::make_heap(openList_begin, openList_end, compare_lt_a_star_node);
	else
		for(; locNbAdded > 0; --locNbAdded)
			std::push_heap(openList_begin, openList_end - (locNbAdded - 1), compare_lt_a_star_node);
}

paths::route a_star_search(map_location const &src, map_location const &dst,
                           double stop_at, cost_calculator const *costCalculator, const size_t parWidth,
                           const size_t parHeight, std::set<map_location> const *teleports)
{
	//----------------- PRE_CONDITIONS ------------------
	assert(src.valid(parWidth, parHeight));
	assert(dst.valid(parWidth, parHeight));
	assert(costCalculator != NULL);
	assert(stop_at <= costCalculator->getNoPathValue());
	//---------------------------------------------------
	static a_star_world aStarGameWorld;

	vector_a_star_node openList;
	vector_location vectLocation;
	paths::route locRoute;
	size_t locNbTeleport;
	a_star_node *locDestNode = NULL;
	a_star_node *locCurNode = NULL;

	DBG_PF << "A* search: " << src << " -> " << dst << '\n';

	if (costCalculator->cost(src,dst, 0) >= stop_at) {
		LOG_PF << "aborted A* search because Start or Dest is invalid\n";
		locRoute.move_left = int(costCalculator->getNoPathValue());
		return locRoute;
	}

	a_star_init(src, dst, openList, aStarGameWorld, parWidth, parHeight, vectLocation, teleports, locNbTeleport);

	bool routeSolved = false;
	while (!routeSolved && !openList.empty())
	{
		locCurNode = openList.front();
		assert(locCurNode != NULL);

		//if we have found a solution
		if (locCurNode->loc == dst)
		{
			routeSolved = true;
		} else {
			std::pop_heap(openList.begin(), openList.end(), compare_lt_a_star_node);
			openList.pop_back();

			assert(locCurNode->isInCloseList == false);
			locCurNode->isInCloseList = true;

			a_star_explore_neighbours(dst, stop_at, costCalculator, parWidth, parHeight,
				teleports, vectLocation, openList, aStarGameWorld, locCurNode, locNbTeleport);
		}
	}
	if(routeSolved) {
		locDestNode = locCurNode;

		DBG_PF << "found solution; calculating it...\n";
		while (locCurNode != NULL)
		{
			locRoute.steps.push_back(locCurNode->loc);
			locCurNode = locCurNode->nodeParent;
		}
		std::reverse(locRoute.steps.begin(), locRoute.steps.end());
		int move_left = costCalculator->get_max_cost() - int(locDestNode->g);

		if (move_left > 0)
			locRoute.move_left = move_left;

		assert(locRoute.steps.front() == src);
		assert(locRoute.steps.back() == dst);

		DBG_PF << "exiting a* search (solved)\n";
	} else {
		//route not solved
		LOG_PF << "aborted a* search\n";
		locRoute.move_left = int(costCalculator->getNoPathValue());
	}
	openList.clear();
	aStarGameWorld.clear();
	return locRoute;
}

static void get_tiles_radius_internal(const map_location& a, size_t radius,
	std::set<map_location>& res, std::map<map_location,int>& visited)
{
	visited[a] = radius;
	res.insert(a);

	if(radius == 0) {
		return;
	}

	map_location adj[6];
	get_adjacent_tiles(a,adj);
	for(size_t i = 0; i != 6; ++i) {
		if(visited.count(adj[i]) == 0 || visited[adj[i]] < int(radius)-1) {
			get_tiles_radius_internal(adj[i],radius-1,res,visited);
		}
	}
}

void get_tiles_radius(const map_location& a, size_t radius,
					  std::set<map_location>& res)
{
	std::map<map_location,int> visited;
	get_tiles_radius_internal(a,radius,res,visited);
}

void get_tiles_radius(gamemap const &map, std::vector<map_location> const &locs,
                      size_t radius, std::set<map_location> &res, xy_pred *pred)
{
	typedef std::set<map_location> location_set;
	location_set not_visited(locs.begin(), locs.end()), must_visit, filtered_out;
	++radius;

	for(;;) {
		location_set::const_iterator it = not_visited.begin(), it_end = not_visited.end();
		std::copy(it,it_end,std::inserter(res,res.end()));
		for(; it != it_end; ++it) {
			map_location adj[6];
			get_adjacent_tiles(*it, adj);
			for(size_t i = 0; i != 6; ++i) {
				map_location const &loc = adj[i];
				if(map.on_board(loc) && !res.count(loc) && !filtered_out.count(loc)) {
					if(!pred || (*pred)(loc)) {
						must_visit.insert(loc);
					} else {
						filtered_out.insert(loc);
					}
				}
			}
		}

		if(--radius == 0 || must_visit.empty()) {
			break;
		}

		not_visited.swap(must_visit);
		must_visit.clear();
	}
}

