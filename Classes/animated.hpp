/* $Id: animated.hpp 31859 2009-01-01 10:28:26Z mordante $ */
/*
   Copyright (C) 2004 - 2009 by Philippe Plantier <ayin@anathas.org>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file animated.hpp
 * Animate units.
 */

#ifndef ANIMATED_IMAGE_H_INCLUDED
#define ANIMATED_IMAGE_H_INCLUDED

#include "image.hpp"

#include <string>
#include <map>
#include <vector>

void new_animation_frame();
int get_current_animation_tick();


template<typename T>
class void_value
{
	public:
		const T operator()() { return T(); }
};

template<typename T, typename T_void_value=void_value<T> >
class animated
{
public:

	animated(int start_time=0);
	virtual ~animated(){};


	typedef  std::pair<int,T> frame_description;
	typedef  std::vector<frame_description> anim_description;
	animated(const std::vector<frame_description> &cfg, int start_time = 0,bool force_change =false);


	/** Adds a frame to an animation. */
	void add_frame(int duration, const T& value,bool force_change =false);

	/**
	 * Starts an animation cycle.
	 *
	 * The first frame of the animation to start may be set to any value by
	 * using a start_time different to 0.
	 */
	void start_animation(int start_time, bool cycles=false);
	void pause_animation(){ started_ =false;};
	void restart_animation(){if(start_tick_) started_ = true;};

	int get_begin_time() const;
	int get_end_time() const;
        void set_begin_time(int new_begin_time);

	int time_to_tick(int animation_time) const;
	int tick_to_time(int animation_tick) const;

	void update_last_draw_time(double acceleration = 0);
	bool need_update() const;

	bool cycles() const {return cycles_;};

	/** Returns true if the current animation was finished. */
	bool animation_finished() const;
	bool animation_finished_potential() const;
	int get_animation_time() const;
	int get_animation_time_potential() const;

	int get_animation_duration() const;
	const T& get_current_frame() const;
	int get_current_frame_begin_time() const;
	int get_current_frame_end_time() const;
	int get_current_frame_duration() const;
	int get_current_frame_time() const;
	const T& get_first_frame() const;
	const T& get_last_frame() const;
	int get_frames_count() const;
	void force_change() {does_not_change_ = false ; }
	bool does_not_change() const {return does_not_change_;}

	static const T void_value_; //MSVC: the frame constructor below requires this to be public

protected:
friend class unit_animation;
	int starting_frame_time_;
        // backward compatibility for teleport anims
        void remove_frames_until(int starting_time);
        void remove_frames_after(int ending_time);

private:
	struct frame
	{

		frame(int duration , const T& value,int start_time) :
			duration_(duration),value_(value),start_time_(start_time)
		{};
		frame():
			duration_(0),value_(void_value_),start_time_(0)
		{};

		// Represents the timestamp of the frame start
		int duration_;
		T value_;
		int start_time_;

	};

	bool does_not_change_;	// Optimization for 1-frame permanent animations
	bool started_;
	bool need_first_update_;
	std::vector<frame> frames_;

	// These are only valid when anim is started
	int start_tick_; // time at which we started
	bool cycles_;
	double acceleration_;
	int last_update_tick_;
	int current_frame_key_;

};

// KP: specialization for image::locator to add required cache functions
template<>
class animated <image::locator>
{
	public:

	animated(int start_time=0);
	virtual ~animated(){};


	typedef  std::pair<int,image::locator> frame_description;
	typedef  std::vector<frame_description> anim_description;
	animated(const std::vector<frame_description> &cfg, int start_time = 0,bool force_change =false);


	/** Adds a frame to an animation. */
	void add_frame(int duration, const image::locator& value,bool force_change =false);

	/**
	 * Starts an animation cycle.
	 *
	 * The first frame of the animation to start may be set to any value by
	 * using a start_time different to 0.
	 */
	void start_animation(int start_time, bool cycles=false);
	void pause_animation(){ started_ =false;};
	void restart_animation(){if(start_tick_) started_ = true;};

	int get_begin_time() const;
	int get_end_time() const;
	void set_begin_time(int new_begin_time);

	int time_to_tick(int animation_time) const;
	int tick_to_time(int animation_tick) const;

	void update_last_draw_time(double acceleration = 0);
	bool need_update() const;

	bool cycles() const {return cycles_;};

	/** Returns true if the current animation was finished. */
	bool animation_finished() const;
	bool animation_finished_potential() const;
	int get_animation_time() const;
	int get_animation_time_potential() const;

	int get_animation_duration() const;
	const image::locator& get_current_frame() const;
	int get_current_frame_begin_time() const;
	int get_current_frame_end_time() const;
	int get_current_frame_duration() const;
	int get_current_frame_time() const;
	const image::locator& get_first_frame() const;
	const image::locator& get_last_frame() const;
	int get_frames_count() const;
	void force_change() {does_not_change_ = false ; }
	bool does_not_change() const {return does_not_change_;}

	static const image::locator void_value_; //MSVC: the frame constructor below requires this to be public

	protected:
	friend class unit_animation;
	int starting_frame_time_;
	// backward compatibility for teleport anims
	void remove_frames_until(int starting_time);
	void remove_frames_after(int ending_time);

	private:
	struct frame
	{
		
		frame(int duration , const image::locator& value,int start_time) :
		duration_(duration),value_(value),start_time_(start_time)
		{};
		frame():
		duration_(0),value_(void_value_),start_time_(0)
		{};
		
		// Represents the timestamp of the frame start
		int duration_;
		image::locator value_;
		int start_time_;
		
		// KP: added cache functions
		void saveCache(FILE *file) const
		{
			fwrite(&duration_, sizeof(duration_), 1, file);
			value_.saveCache(file);
			fwrite(&start_time_, sizeof(start_time_), 1, file);
		}
		
		void loadCache(MEMFILE *file, char *loadBuffer)
		{
			mread(&duration_, sizeof(duration_), 1, file);
			value_.loadCache(file, loadBuffer);
			mread(&start_time_, sizeof(start_time_), 1, file);
		}
	};

	bool does_not_change_;	// Optimization for 1-frame permanent animations
	bool started_;
	bool need_first_update_;
	std::vector<frame> frames_;

	// These are only valid when anim is started
	int start_tick_; // time at which we started
	bool cycles_;
	double acceleration_;
	int last_update_tick_;
	int current_frame_key_;


	// KP: added cache functions
	public:
	void saveCache(FILE *file) const
	{
		fwrite(&starting_frame_time_, sizeof(starting_frame_time_), 1, file);
		char bChar;
		bChar = does_not_change_;
		fwrite(&bChar, sizeof(bChar), 1, file);
		bChar = started_;
		fwrite(&bChar, sizeof(bChar), 1, file);
		bChar = need_first_update_;
		fwrite(&bChar, sizeof(bChar), 1, file);
		short size = frames_.size();
		fwrite(&size, sizeof(size), 1, file);
		for (int i=0; i < size; i++)
		{
			frames_[i].saveCache(file);
		}
		fwrite(&start_tick_, sizeof(start_tick_), 1, file);
		bChar = cycles_;
		fwrite(&bChar, sizeof(bChar), 1, file);
		fwrite(&acceleration_, sizeof(acceleration_), 1, file);
		fwrite(&last_update_tick_, sizeof(last_update_tick_), 1, file);
		fwrite(&current_frame_key_, sizeof(current_frame_key_), 1, file);
	}

	void loadCache(MEMFILE *file, char *loadBuffer)
	{
		mread(&starting_frame_time_, sizeof(starting_frame_time_), 1, file);
		char bChar;
		mread(&bChar, sizeof(bChar), 1, file);
		does_not_change_ = bChar != 0;
		mread(&bChar, sizeof(bChar), 1, file);
		started_ = bChar != 0;
		mread(&bChar, sizeof(bChar), 1, file);
		need_first_update_ = bChar != 0;
		short size;
		mread(&size, sizeof(size), 1, file);
		frames_.reserve(size);
		for (int i=0; i < size; i++)
		{
			frame f;
			f.loadCache(file, loadBuffer);
			frames_.push_back(f);
		}
		mread(&start_tick_, sizeof(start_tick_), 1, file);
		mread(&bChar, sizeof(bChar), 1, file);
		cycles_ = bChar != 0;
		mread(&acceleration_, sizeof(acceleration_), 1, file);
		mread(&last_update_tick_, sizeof(last_update_tick_), 1, file);
		mread(&current_frame_key_, sizeof(current_frame_key_), 1, file);
	}

};

#endif

