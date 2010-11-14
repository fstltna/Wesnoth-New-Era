/* $Id: config_cache.cpp 33687 2009-03-15 16:50:42Z mordante $ */
/*
   Copyright (C) 2008 - 2009 by Pauli Nieminen <paniemin@cc.hut.fi>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "config_cache.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "gettext.hpp"
#include "game_config.hpp"
#include "game_display.hpp"
#include "log.hpp"
#include "marked-up_text.hpp"
#include "show_dialog.hpp"
#include "sha1.hpp"
#include "serialization/binary_or_text.hpp"
#include "serialization/parser.hpp"

#include <boost/bind.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <iostream>
#include <fstream>
#include "zlib.h"

#define ERR_CACHE std::cerr //LOG_STREAM(err, cache)
#define LOG_CACHE std::cerr //LOG_STREAM(info, cache)
#define DBG_CACHE std::cerr //LOG_STREAM(debug, cache)


extern "C" {
	void* dlmalloc(size_t size);
	void* dlcalloc(size_t count, size_t size);
	void* dlvalloc(size_t size);
	void* dlmemalign(size_t alignment, size_t size);
	void* dlrealloc(void* ptr, size_t size);
	void dlfree(void* ptr);
};

namespace game_config {

	config_cache& config_cache::instance()
	{
		static config_cache cache;
		return cache;
	}

	config_cache::config_cache() :
#ifdef __IPHONEOS__	
		force_valid_cache_(true),
#else	
		force_valid_cache_(false),
#endif	
		use_cache_(true),
		fake_invalid_cache_(false),
		defines_map_(),
		path_defines_()
	{
		// To set-up initial defines map correctly
		clear_defines();
	}

	struct output {
		void operator()(const preproc_map::value_type& def)
		{
			DBG_CACHE << "key: " << def.first << " " << def.second << "\n";
		}
	};
	const preproc_map& config_cache::get_preproc_map() const
	{
		return defines_map_;
	}

	void config_cache::clear_defines()
	{
		LOG_CACHE << "Clearing defines map!\n";
		defines_map_.clear();
		// set-up default defines map

#ifdef USE_TINY_GUI
		defines_map_["TINY"] = preproc_define();
#endif

		if (game_config::small_gui)
			defines_map_["SMALL_GUI"] = preproc_define();

#if defined(__APPLE__)
		defines_map_["APPLE"] = preproc_define();
#endif

	}

	void config_cache::get_config(const std::string& path, config& cfg)
	{
		load_configs(path, cfg);
	}

	config_ptr config_cache::get_config(const std::string& path)
	{
		config_ptr ret(new config());
		load_configs(path, *ret);

		return ret;
	}


	void config_cache::write_file(std::string path, const config& cfg)
	{
		scoped_ostream stream = ostream_file(path);
		const bool gzip = true;
		config_writer writer(*stream, gzip, game_config::cache_compression_level);
		writer.write(cfg);
	}
	void config_cache::write_file(std::string path, const preproc_map& defines_map)
	{
		if (defines_map.empty())
		{
			if (file_exists(path))
			{
				delete_directory(path);
			}
			return;
		}
		scoped_ostream stream = ostream_file(path);
		const bool gzip = true;
		config_writer writer(*stream, gzip, game_config::cache_compression_level);

		// write all defines to stream;
		// call foreach define is: second.write(config_writer,first);
		std::for_each(defines_map.begin(), defines_map.end(),
			   	boost::bind(&preproc_define::write,
					boost::bind(&preproc_map::value_type::second,_1),
					boost::ref(writer),
					boost::bind(&preproc_map::value_type::first,_1)));

	}

	void config_cache::read_file(const std::string& path, config& cfg)
	{
		unsigned long timestart = SDL_GetTicks();
		std::cerr << "\nStarting config_cache::read_file() at " << timestart << " ticks\n";
		
		// KP: try to use a memory stream on iPhone first, to buffer the data in memory
		scoped_istream stream = istream_file(path);
		read_gz(cfg, *stream);
/*		
		std::ifstream file(path.c_str());
		file.seekg(0, std::ios::end);
		std::streampos length = file.tellg();
		file.seekg(0, std::ios::beg);
		
		std::vector<char> buffer(length);
		file.read(&buffer[0], length);							// ensures the data is read all at once, not char-by-char
		
		std::stringstream localStream;
		localStream.rdbuf()->pubsetbuf(&buffer[0], length);		// data is pointed to, NOT copied
		
		read_gz(cfg, localStream);
 */
		
		unsigned long timeend = SDL_GetTicks();
		std::cerr << "Ended config_cache::read_file() at " << timeend << " ticks (" << timeend-timestart << "ms)\n";
	}

	preproc_map& config_cache::make_copy_map()
	{
		return config_cache_transaction::instance().get_active_map(defines_map_);
	}


	static bool compare_define(const preproc_map::value_type& a, const preproc_map::value_type& b)
	{
		if (a.first < b.first)
			return true;
		if (b.first < a.first)
			return false;
		if (a.second < b.second)
			return true;
		return false;
	}

	void config_cache::add_defines_map_diff(preproc_map& defines_map)
	{
		return config_cache_transaction::instance().add_defines_map_diff(defines_map);
	}


	void config_cache::read_configs(const std::string& path, config& cfg, preproc_map& defines_map)
	{
		std::string error_log;
		//read the file and then write to the cache
		scoped_istream stream = preprocess_file(path, &defines_map, &error_log);

		read(cfg, *stream, &error_log);
		if (!error_log.empty())
		{
			throw config::error(error_log);
		}
	}

	void config_cache::read_cache(const std::string& path, config& cfg)
	{
		const std::string extension = ".dat"; //".gz";
		bool is_valid = true;
		std::stringstream defines_string;

		//		defines_string << path;
		
		// KP: for iPhone, do not use the full path, remove the exe_dir
		// this is to support pre-caching, when the app path changes
		int pos = path.find(game_config::path);
		std::string newPath;
		if (pos != std::string::npos)
		{
			newPath = path.substr(game_config::path.size());
		}
		else
		{
			newPath = path;
		}
		defines_string << newPath;
		for(preproc_map::const_iterator i = defines_map_.begin(); i != defines_map_.end(); ++i) {
			if(i->second.value != "" || i->second.arguments.empty() == false) {
				is_valid = false;
				ERR_CACHE << "Preprocessor define not valid\n";
				break;
			}

			defines_string << " " << i->first;
		}

		// Do cache check only if  define map is valid and
		// caching is allowed
		if(is_valid) {
			const std::string& cache = get_cache_dir();
			if(cache != "") {
				sha1_hash sha(defines_string.str()); // use a hash for a shorter display of the defines
				const std::string fname = cache + "/cache-v" +
					boost::algorithm::replace_all_copy(game_config::revision, ":", "_") +
					"-" + sha.display();
				const std::string fname_checksum = fname + ".checksum" + extension;

				file_tree_checksum dir_checksum;

				if(!force_valid_cache_ && !fake_invalid_cache_) {
					try {
						if(file_exists(fname_checksum)) {
							DBG_CACHE << "Reading checksum: " << fname_checksum << "\n";
							config checksum_cfg;
							read_file(fname_checksum, checksum_cfg);
							dir_checksum = file_tree_checksum(checksum_cfg);
						}
					} catch(config::error&) {
						ERR_CACHE << "cache checksum is corrupt\n";
					} catch(io_exception&) {
						ERR_CACHE << "error reading cache checksum\n";
					}
				}

				if(force_valid_cache_) {
					LOG_CACHE << "skipping cache validation (forced)\n";
				}

				if(file_exists(fname + ".cache.dat") && (force_valid_cache_ || (dir_checksum == data_tree_checksum()))) {
					LOG_CACHE << "found valid cache at '" << fname << extension << "' with defines_map " << defines_string.str() << "\n";
					log_scope("read cache");
					try 
					{
						/*
						read_file(fname + extension,cfg);
						 */
						cfg.loadCache(fname);
						
						// TODO: check for a diff, if the define string is not "/data APPLE TINY"
						const std::string define_file = fname + ".define" + extension;
						if (file_exists(define_file))
						{
							config_cache_transaction::instance().add_define_file(define_file);
						}
						return;
					} catch(config::error& e) {
						ERR_CACHE << "cache " << fname << extension << " is corrupt. Loading from files: "<< e.message<<"\n";
					} catch(io_exception&) {
						ERR_CACHE << "error reading cache " << fname << extension << ". Loading from files\n";
					}
				}

				LOG_CACHE << "no valid cache found. Writing cache to '" << fname << extension << " with defines_map "<< defines_string.str() << "'\n";
				// Now we need queued defines so read them to memory
				read_defines_queue();

				preproc_map copy_map(make_copy_map());

				read_configs(path, cfg, copy_map);

				add_defines_map_diff(copy_map);

				try {
					//write_file(fname + extension, cfg);
					
					// TODO: only save a diff, if the define string is not "/data APPLE TINY"
					cfg.saveCache(fname);
					
					
					write_file(fname + ".define" + extension, copy_map);
					/*
					config checksum_cfg;
					data_tree_checksum().write(checksum_cfg);
					write_file(fname_checksum, checksum_cfg);
					 */
				} catch(io_exception&) {
					ERR_CACHE << "could not write to cache '" << fname << "'\n";
				}
				return;
			}
		}
		LOG_CACHE << "Loading plain config instead of cache\n";
		preproc_map copy_map(make_copy_map());
		read_configs(path, cfg, copy_map);
		add_defines_map_diff(copy_map);
	}


	void config_cache::read_defines_file(const std::string& path)
	{
		config cfg;
		read_file(path, cfg);

		DBG_CACHE << "Reading cached defines from: " << path << "\n";

		// use static preproc_define::read_pair(config*) to make a object
		// and pass that object config_cache_transaction::insert_to_active method
	   	std::for_each(cfg.ordered_begin(), cfg.ordered_end(),
				add_define_from_file());
	}

	void config_cache::read_defines_queue()
	{
		const config_cache_transaction::filenames& files = config_cache_transaction::instance().get_define_files();
		std::for_each(files.begin(), files.end(),
				boost::bind(&config_cache::read_defines_file,
					this,
					_1));
	}

	scoped_preproc_define_ptr config_cache::create_path_preproc_define(const path_define_map::value_type& define)
	{
		scoped_preproc_define_ptr ret(new scoped_preproc_define(define.second));
		return ret;
	}

	void config_cache::load_configs(const std::string& path, config& cfg)
	{
		// Make sure that we have fake transaction if no real one is going on
		fake_transaction fake;

		{
			// activate path defines
			scoped_preproc_define_list defines;
			std::for_each(path_defines_.lower_bound(path),path_defines_.upper_bound(path),
					boost::bind(&scoped_preproc_define_list::push_back,
						boost::ref(defines),
						boost::bind(&config_cache::create_path_preproc_define,
							this,
							_1)
						)
					);
			if (use_cache_)
			{
				read_cache(path, cfg);
			} else {
				preproc_map copy_map(make_copy_map());
				read_configs(path, cfg, copy_map);
				add_defines_map_diff(copy_map);
			}
		}

		return;
	}

	void config_cache::set_force_invalid_cache(bool force)
	{
		fake_invalid_cache_ = force;
	}

	void config_cache::set_use_cache(bool use)
	{
		use_cache_ = use;
	}

	void config_cache::set_force_valid_cache(bool force)
	{
		force_valid_cache_ = force;
	}

	void config_cache::recheck_filetree_checksum()
	{
		data_tree_checksum(true);
	}

	void config_cache::add_path_define(const std::string& path, const std::string& define)
	{
		path_defines_.insert(std::make_pair(path,define));
	}

	class define_finder {
		const std::string& define_;
		public:
		define_finder(const std::string& define) : define_(define)
		{}
		bool operator()(const config_cache::path_define_map::value_type& value)
		{
			return value.second == define_;
		}
	};

	void config_cache::remove_path_define(const std::string& path, const std::string& define)
	{
		path_define_map::iterator begin = path_defines_.lower_bound(path);
		path_define_map::iterator end = path_defines_.upper_bound(path);
		path_define_map::iterator match = std::find_if(begin,end, define_finder(define));
		path_defines_.erase(match);
	}

	void config_cache::add_define(const std::string& define)
	{
		DBG_CACHE << "adding define: " << define << "\n";
		defines_map_[define] = preproc_define();
		if (config_cache_transaction::is_active())
		{
			// we have to add this to active map too
			config_cache_transaction::instance().get_active_map(defines_map_).insert(
					std::make_pair(define, preproc_define()));
		}

	}

	void config_cache::remove_define(const std::string& define)
	{
		DBG_CACHE << "removing define: " << define << "\n";
		defines_map_.erase(define);
		if (config_cache_transaction::is_active())
		{
			// we have to remove this from active map too
			config_cache_transaction::instance().get_active_map(defines_map_).erase(define);
		}
	}

	config_cache_transaction::state config_cache_transaction::state_ = FREE;
	config_cache_transaction* config_cache_transaction::active_ = 0;

	config_cache_transaction::config_cache_transaction()
		: define_filenames_()
		, active_map_()
	{
		assert(state_ == FREE);
		state_ = NEW;
		active_ = this;
	}

	config_cache_transaction::~config_cache_transaction()
	{
		state_ = FREE;
		active_ = 0;
	}

	void config_cache_transaction::lock()
	{
		state_ = LOCKED;
	}

	const config_cache_transaction::filenames& config_cache_transaction::get_define_files() const
	{
		return define_filenames_;
	}

	void config_cache_transaction::add_define_file(const std::string& file)
	{
		define_filenames_.push_back(file);
	}

	preproc_map& config_cache_transaction::get_active_map(const preproc_map& defines_map)
	{
		if(active_map_.empty())
		{
			std::copy(defines_map.begin(),
					defines_map.end(),
					std::insert_iterator<preproc_map>(active_map_, active_map_.begin()));
			if ( get_state() == NEW)
				state_ = ACTIVE;
		 }

		return active_map_;
	}

	void config_cache_transaction::add_defines_map_diff(preproc_map& new_map)
	{

		if (get_state() == ACTIVE)
		{
			preproc_map temp;
			std::set_difference(new_map.begin(),
					new_map.end(),
					active_map_.begin(),
					active_map_.end(),
					std::insert_iterator<preproc_map>(temp,temp.begin()),
					&compare_define);
			std::for_each(temp.begin(), temp.end(),boost::bind(&config_cache_transaction::insert_to_active,this, _1));

			temp.swap(new_map);
		} else if (get_state() == LOCKED) {
			new_map.clear();
		}
	}

	void config_cache_transaction::insert_to_active(const preproc_map::value_type& def)
	{
		active_map_[def.first] = def.second;
	}
}

// KP: cache functions
std::vector<std::string> stringTable;
std::vector<unsigned long> stringOffsets;
char *loadStringTable = NULL;
unsigned long totalStringSize=0;
std::map<std::string, int> stringTableHelper;

void cacheFreeStringTable(void)
{
	stringTable.clear();
	stringTableHelper.clear();
	stringOffsets.clear();
	if (loadStringTable)
		free(loadStringTable);
	loadStringTable = NULL;
	totalStringSize = 0;
}


#include <algorithm>

void cacheSaveString(FILE *file, const std::string &str)
{
	unsigned short pos;
	
	// check if vector contains element
	//std::vector<std::string>::iterator it;
	//it = std::find(stringTable.begin(), stringTable.end(), str);
	std::map<std::string, int>::iterator fnd;
	fnd = stringTableHelper.lower_bound(str);
	
	if (fnd != stringTableHelper.end() && !(stringTableHelper.key_comp()(str, fnd->first)))
	{
		//pos = it - stringTable.begin();
		pos = fnd->second;
	}
	else
	{
		stringTable.push_back(str);
		stringOffsets.push_back(totalStringSize);
		pos = stringTable.size() - 1;
		totalStringSize += str.size();
		stringTableHelper.insert(fnd, std::make_pair<std::string, int>(str, pos));
	}
	
	fwrite(&pos, sizeof(pos), 1, file);
}

void cacheSaveStringTable(FILE *file)
{
	unsigned long strSize;
	const char *string;
	
	unsigned short numStrings = stringTable.size();
	fwrite(&numStrings, sizeof(numStrings), 1, file);
	fwrite(&totalStringSize, sizeof(totalStringSize), 1, file);
	for (unsigned short i=0; i < numStrings; i++)
	{
		unsigned long strOffset = stringOffsets[i];
		fwrite(&strOffset, sizeof(strOffset), 1, file);
	}
	for (unsigned short i=0; i < numStrings; i++)
	{
		strSize = stringTable[i].size();
		string = stringTable[i].c_str();
		fwrite(string, strSize, 1, file);
	}
}

void cacheLoadStringTable(MEMFILE *file, char *loadBuffer)
{
	unsigned short numStrings;
	mread(&numStrings, sizeof(numStrings), 1, file);
	mread(&totalStringSize, sizeof(totalStringSize), 1, file);
	stringOffsets.reserve(numStrings+1);
	for (unsigned short i=0; i < numStrings; i++)
	{
		unsigned long strOffset;
		mread(&strOffset, sizeof(strOffset), 1, file);
		stringOffsets.push_back(strOffset);
	}
	stringOffsets.push_back(totalStringSize);
	loadStringTable = (char *) malloc(totalStringSize);
	mread(loadStringTable, totalStringSize, 1, file);	
}

std::string gTempStr, gTempStr2;

void cacheLoadString(MEMFILE *file, char **charPtr, unsigned long *strSize)
{
	unsigned short index;
	mread(&index, sizeof(index), 1, file);
	
	*charPtr = loadStringTable + stringOffsets[index];
	*strSize = stringOffsets[index+1] - stringOffsets[index];
}

/**
 *	uncompress a file to memory with minimal overhead
 *
 *	@param filename - the name of the compressed file to load
 *  @return unsigned char * - pointer to uncompressed data
 *
 *  NOTE: it uses dlmalloc to get memory, avoiding increasing the tcmalloc pool, which is never released to the OS
 *  NOTE: make sure to dlfree() the returned pointer!
 */
unsigned char *cacheUncompress(std::string &filename)
{
	FILE *infile = fopen(filename.c_str(), "rb");

	if (!infile)
		return NULL;
	
	// read zlib stats
	unsigned long originalSize;
	unsigned long compressedSize;
	fread(&originalSize, sizeof(originalSize), 1, infile);
	fread(&compressedSize, sizeof(compressedSize), 1, infile);
	
	// uncompress the data
	unsigned char *compressedData = (unsigned char *) dlmalloc(compressedSize);
	unsigned char *originalData = (unsigned char *) dlmalloc(originalSize);
	assert(compressedData);
	assert(originalData);
	fread(compressedData, compressedSize, 1, infile);
	fclose(infile);
	
	int z_result = uncompress(originalData, &originalSize, compressedData, compressedSize);
	assert(z_result == Z_OK);
	
	dlfree(compressedData);
	return originalData;
}

/**
 *	Compresses a file
 *
 *	@param filename - name of an existing file to compress
 *
 *  NOTE: the original file is overwritten with the compressed version!
 */
void cacheCompress(std::string &filename)
{
	FILE *infile = fopen(filename.c_str(), "rb");
	
	if (!infile)
		return;
	
	unsigned long originalSize;
	
	fseek(infile, 0, SEEK_END);
	originalSize = ftell(infile);
	fseek(infile, 0, SEEK_SET);
	
	unsigned char *originalData = (unsigned char *) malloc(originalSize);
	fread(originalData, originalSize, 1, infile);
	fclose(infile);
	
	unsigned long compressedSize = ((float)originalSize * 1.10) + 100;	// just a maximum guess...
	unsigned char *compressedData = (unsigned char *) malloc(compressedSize);
	int z_result = compress(compressedData, &compressedSize, originalData, originalSize);
	assert(z_result == Z_OK);
	
	FILE *outfile = fopen(filename.c_str(), "wb");
	fwrite(&originalSize, sizeof(originalSize), 1, outfile);
	fwrite(&compressedSize, sizeof(compressedSize), 1, outfile);
	fwrite(compressedData, compressedSize, 1, outfile);
	fclose(outfile);

	free(originalData);
	free(compressedData);
}
