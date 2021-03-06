/* $Id: preprocessor.cpp 35988 2009-06-01 11:41:05Z silene $ */
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
 * @file serialization/preprocessor.cpp
 * WML preprocessor.
 */

#include "global.hpp"

#include "config.hpp"
#include "filesystem.hpp"
#include "foreach.hpp"
#include "log.hpp"
#include "wesconfig.h"
#include "serialization/binary_or_text.hpp"

#define ERR_CF LOG_STREAM(err, config)
#define LOG_CF LOG_STREAM(info, config)
#define DBG_CF LOG_STREAM(debug, config)

using std::streambuf;

bool preproc_define::operator==(preproc_define const &v) const {
	return value == v.value && arguments == v.arguments;
}

bool preproc_define::operator<(preproc_define const &v) const {
	if (location < v.location)
		return true;
	if (v.location < location)
		return false;
	return linenum < v.linenum;
}

void preproc_define::write_argument(config_writer& writer, const std::string& arg) const
{

	const std::string key = "argument";

	writer.open_child(key);

	writer.write_key_val("name", arg);
	writer.close_child(key);
}

void preproc_define::write(config_writer& writer, const std::string& name) const
{

	const std::string key = "preproc_define";
	writer.open_child(key);

	writer.write_key_val("name", name);
	writer.write_key_val("value", value);
	writer.write_key_val("textdomain", textdomain);
	writer.write_key_val("linenum", lexical_cast<std::string>(linenum));
	writer.write_key_val("location", location);

	foreach (const std::string &arg, arguments)
		write_argument(writer, arg);

	writer.close_child(key);
}

void preproc_define::read_argument(const config &cfg)
{
	arguments.push_back(cfg["name"]);
}

void preproc_define::read(const config& cfg)
{
	value = cfg["value"];
	textdomain = cfg["textdomain"];
	linenum = lexical_cast<int>(cfg["linenum"]);
	location = cfg["location"];

	foreach (const config *arg, cfg.get_children("argument"))
		read_argument(*arg);
}

preproc_map::value_type preproc_define::read_pair(const config* cfg)
{
	std::string first = (*cfg)["name"];

	preproc_define second;
	second.read(*cfg);
	return std::make_pair(first, second);
}

std::ostream& operator<<(std::ostream& stream, const preproc_define& def)
{
	return stream << std::string("value: ") << def.value << std::string(" arguments: ") << def.location;
}

std::ostream& operator<<(std::ostream& stream, const preproc_map::value_type& def)
{
	return stream << def.second;
}

class preprocessor;
class preprocessor_file;
class preprocessor_data;
class preprocessor_streambuf;
struct preprocessor_deleter;

/**
 * Base class for preprocessing an input.
 */
class preprocessor
{
	preprocessor *const old_preprocessor_;
	std::string old_textdomain_;
	std::string old_location_;
	int old_linenum_;
protected:
	preprocessor_streambuf &target_;
	std::vector<std::string> *called_macros_;
	preprocessor(preprocessor_streambuf &, std::vector<std::string> *);
public:
	/**
	 * Preprocesses and sends some text to the #target_ buffer.
	 * @return false when the input has no data left.
	 */
	virtual bool get_chunk() = 0;
	virtual ~preprocessor();
};

/**
 * Target for sending preprocessed output.
 * Objects of this class can be plugged into an STL stream.
 */
class preprocessor_streambuf: public streambuf
{
	std::string out_buffer_;      /**< Buffer read by the STL stream. */
	virtual int underflow();
	std::stringstream buffer_;    /**< Buffer filled by the #current_ preprocessor. */
	preprocessor *current_;       /**< Input preprocessor. */
	preproc_map *defines_;
	preproc_map default_defines_;
	std::string textdomain_;
	std::string location_;
	std::string *error_log;
	int linenum_;
	int depth_;
	/**
	 * Set to true if one preprocessor for this target started to read a string.
	 * Deeper-nested preprocessors are then forbidden to.
	 */
	bool quoted_;
	friend class preprocessor;
	friend class preprocessor_file;
	friend class preprocessor_data;
	friend struct preprocessor_deleter;
	preprocessor_streambuf(preprocessor_streambuf const &);
public:
	preprocessor_streambuf(preproc_map *, std::string *);
	void error(const std::string &, const std::string &);
};

preprocessor_streambuf::preprocessor_streambuf(preproc_map *def, std::string *err_log) :
	streambuf(),
	out_buffer_(""),
	buffer_(),
	current_(NULL),
	defines_(def),
	default_defines_(),
	textdomain_(PACKAGE),
	location_(""),
	error_log(err_log),
	linenum_(0),
	depth_(0),
	quoted_(false)
{
}

preprocessor_streambuf::preprocessor_streambuf(preprocessor_streambuf const &t) :
	streambuf(),
	out_buffer_(""),
	buffer_(),
	current_(NULL),
	defines_(t.defines_),
	default_defines_(),
	textdomain_(PACKAGE),
	location_(""),
	error_log(t.error_log),
	linenum_(0),
	depth_(t.depth_),
	quoted_(t.quoted_)
{
}

/**
 * Called by an STL stream whenever it has reached the end of #out_buffer_.
 * Fills #buffer_ by calling the #current_ preprocessor, then copies its
 * content into #out_buffer_.
 * @return the first character of #out_buffer_ if any, EOF otherwise.
 */
int preprocessor_streambuf::underflow()
{
	unsigned sz = 0;
	if (char *gp = gptr()) {
		if (gp < egptr()) {
			// Sanity check: the internal buffer has not been totally consumed,
			// should we force the caller to use what remains first?
			return *gp;
		}
		// The buffer has been completely read; fill it again.
		// Keep part of the previous buffer, to ensure putback capabilities.
		sz = out_buffer_.size();
		buffer_.str(std::string());
		if (sz > 3) {
			buffer_ << out_buffer_.substr(sz - 3);
			sz = 3;
		}
		else
		{
			buffer_ << out_buffer_;
		}
	} else {
		// The internal get-data pointer is null
	}
	const int desired_fill_amount = 2000;
	while (current_ && buffer_.rdbuf()->in_avail() < desired_fill_amount)
	{
		// Process files and data chunks until the desired buffer size is reached
		if (!current_->get_chunk()) {
			 // Delete the current preprocessor item to restore its predecessor
			delete current_;
		}
	}
	// Update the internal state and data pointers
	out_buffer_ = buffer_.str();
	char *begin = &*out_buffer_.begin();
	unsigned bs = out_buffer_.size();
	setg(begin, begin + sz, begin + bs);
	if (sz >= bs)
		return EOF;
	return static_cast<unsigned char>(*(begin + sz));
}

std::string lineno_string(const std::string &lineno)
{
	std::vector< std::string > pos = utils::quoted_split(lineno, ' ');
	std::vector< std::string >::const_iterator i = pos.begin(), end = pos.end();
	std::string included_from = " included from ";
	std::string res;
	while (i != end) {
		std::string const &line = *(i++);
		if (!res.empty())
			res += included_from;
		if (i != end)
			res += *(i++);
		else
			res += "<unknown>";
		res += ':' + line;
	}
	if (res.empty()) res = "???";
	return res;
}

void preprocessor_streambuf::error(const std::string& error_type, const std::string &pos)
{
	utils::string_map i18n_symbols;
	std::string position, error;
	position = lineno_string(pos);
	error = error_type + " at " + position;
	ERR_CF << error << '\n';
	if(error_log!=NULL)
		*error_log += error + '\n';

	throw preproc_config::error(error);
}


/**
 * Sets up a new preprocessor for stream buffer \a t.
 * Saves the current preprocessing context of #target_. It will be
 * automatically restored on destruction.
 */
preprocessor::preprocessor(preprocessor_streambuf &t, std::vector<std::string> *callstack) :
	old_preprocessor_(t.current_),
	old_textdomain_(t.textdomain_),
	old_location_(t.location_),
	old_linenum_(t.linenum_),
	target_(t),
	called_macros_(callstack)
{
	++target_.depth_;
	target_.current_ = this;
}

/**
 * Restores the old preprocessing context of #target_.
 * Appends location and domain directives to the buffer, so that the parser
 * notices these changes.
 */
preprocessor::~preprocessor()
{
	assert(target_.current_ == this);
	target_.current_  = old_preprocessor_;
	target_.location_ = old_location_;
	target_.linenum_  = old_linenum_;
	target_.textdomain_ = old_textdomain_;
	if (!old_location_.empty()) {
		target_.buffer_ << "\376line " << old_linenum_ << ' ' << old_location_ << '\n';
    }
	if (!old_textdomain_.empty()) {
		target_.buffer_ << "\376textdomain " << old_textdomain_ << '\n';
    }
	--target_.depth_;
}

/**
 * Specialized preprocessor for handling a file or a set of files.
 * A preprocessor_file object is created when a preprocessor encounters an
 * inclusion directive that resolves to a file or directory, e.g. '{themes/}'.
 */
class preprocessor_file: preprocessor
{
	std::vector< shared_string > files_;
	std::vector< shared_string >::const_iterator pos_, end_;
public:
	preprocessor_file(preprocessor_streambuf &, std::vector<std::string> *, std::string const &);
	virtual bool get_chunk();
};

/**
 * Specialized preprocessor for handling any kind of input stream.
 * This is the core of the preprocessor.
 */
class preprocessor_data: preprocessor
{
	/** Description of a preprocessing chunk. */
	struct token_desc
	{
		/** @todo FIXME: add enum for token type. */
		/**
		 * Preprocessor state.
		 * - 'i': processing the "if" branch of a ifdef/ifndef (the "else" branch will be skipped)
		 * - 'j': processing the "else" branch of a ifdef/ifndef
		 * - 'I': skipping the "if" branch of a ifdef/ifndef (the "else" branch, if any, will be processed)
		 * - 'J': skipping the "else" branch of a ifdef/ifndef
		 * - '"': processing a string
		 * - '{': processing between chunks of a macro call (skip spaces)
		 * - '[': processing inside a chunk of a macro call (stop on space or '(')
		 * - '(': processing a parenthesized macro argument
		 */
		char type;
		/** Starting position in #strings_ of the delayed text for this chunk. */
		int stack_pos;
		int linenum;
	};
	scoped_istream in_; /**< Input stream. */
	std::string directory_;
	/** Buffer for delayed input processing. */
	std::vector< std::string > strings_;
	/** Stack of nested preprocessing chunks. */
	std::vector< token_desc > tokens_;
	bool is_macro;
	/**
	 * Set to true whenever input tokens cannot be directly sent to the target
	 * buffer. For instance, this happens with macro arguments. In that case,
	 * the output is redirected toward #strings_ until it can be processed.
	 */
	int slowpath_;
	/**
	 * Non-zero when the preprocessor has to skip some input text.
	 * Increased whenever entering a conditional branch that is not useful,
	 * e.g. a ifdef that evaluates to false.
	 */
	int skipping_;
	int linenum_;

	std::string read_word();
	std::string read_line();
	void skip_spaces();
	void skip_eol();
	void push_token(char);
	void pop_token();
	void put(char);
	void put(std::string const & /*, int change_line
	= 0 */);
public:
	preprocessor_data(preprocessor_streambuf &,
                      std::vector<std::string> *,
                      std::istream *,
	                  std::string const &history,
	                  std::string const &name, int line,
	                  std::string const &dir, std::string const &domain,
                      std::string* = NULL);
    ~preprocessor_data();
	virtual bool get_chunk();
};

preprocessor_file::preprocessor_file(preprocessor_streambuf &t, std::vector<std::string> *callstack,
		std::string const &name) :
	preprocessor(t,callstack),
	files_(),
	pos_(),
	end_()
{
	if (is_directory(name))
		get_files_in_dir(name, &files_, NULL, ENTIRE_FILE_PATH, SKIP_MEDIA_DIR, DO_REORDER);
	else {
		std::istream * file_stream = istream_file(name);
		if (!file_stream->good()) {
			ERR_CF << "Could not open file " << name << "\n";
			delete file_stream;
		}
		else
			new preprocessor_data(t, called_macros_, file_stream, "", name, 1, directory_name(name), t.textdomain_);
	}
	pos_ = files_.begin();
	end_ = files_.end();
}

/**
 * preprocessor_file::get_chunk()
 *
 * Inserts and processes the next file in the list of included files.
 * @return	false if there is no next file.
 */
bool preprocessor_file::get_chunk()
{
	while (pos_ != end_) {
		const std::string &name = *(pos_++);
		unsigned sz = name.size();
		// Use reverse iterator to optimize testing
		//if (sz < 5 || !std::equal(name.rbegin(), name.rbegin() + 4, "gfc."))
		if (sz < 5 || !memcmp(name.c_str()+name.size()-1-4, ".cfg", 4))
			continue;
		new preprocessor_file(target_, called_macros_, name);
		return true;
	}
	return false;
}

preprocessor_data::preprocessor_data(preprocessor_streambuf &t, std::vector<std::string> *callstack,
        std::istream *i, std::string const &history, std::string const &name, int linenum,
		std::string const &directory, std::string const &domain, std::string *symbol) :
	preprocessor(t,callstack),
	in_(i),
	directory_(directory),
	strings_(),
	tokens_(),
	is_macro(symbol != NULL),
	slowpath_(0),
	skipping_(0),
	linenum_(linenum)
{
    if(is_macro) {
        called_macros_->push_back(*symbol);
    }

	std::ostringstream s;

	s << history;
	if (!name.empty()) {
		std::string ename(name);
		if (!history.empty())
			s << ' ';
		s << utils::escape(ename, " \\");
	}
	if (!t.location_.empty())
		s << ' ' << t.linenum_ << ' ' << t.location_;
	t.location_ = s.str();
	t.linenum_ = linenum;
	t.textdomain_ = domain;

	t.buffer_ << "\376line " << linenum
		<< ' ' << t.location_ << "\n\376textdomain " << domain << '\n';

	push_token('*');
}

preprocessor_data::~preprocessor_data()
{
    if(is_macro)
    {
        called_macros_->pop_back();
    }
}

void preprocessor_data::push_token(char t)
{
	token_desc token = { t, strings_.size(), linenum_ };
	tokens_.push_back(token);
	std::ostringstream s;
	if (!skipping_ && slowpath_) {
		s << "\376line " << linenum_ << ' ' << target_.location_
		  << "\n\376textdomain " << target_.textdomain_ << '\n';
	}
	strings_.push_back(s.str());
}

void preprocessor_data::pop_token()
{
	strings_.erase(strings_.begin() + tokens_.back().stack_pos, strings_.end());
	tokens_.pop_back();
}

void preprocessor_data::skip_spaces()
{
	for(;;) {
		int c = in_->peek();
		if (!in_->good() || (c != ' ' && c != '\t'))
			return;
		in_->get();
	}
}

void preprocessor_data::skip_eol()
{
	for(;;) {
		int c = in_->get();
		if (c == '\n') {
			++linenum_;
			return;
		}
		if (!in_->good())
			return;
	}
}

std::string preprocessor_data::read_word()
{
	std::string res;
	for(;;) {
		int c = in_->peek();
		if (c == preprocessor_streambuf::traits_type::eof() || utils::portable_isspace(c)) {
			// DBG_CF << "(" << res << ")\n";
			return res;
		}
		in_->get();
		res += static_cast<char>(c);
	}
}

std::string preprocessor_data::read_line()
{
	std::string res;
	for(;;) {
		int c = in_->get();
		if (c == '\n') {
			++linenum_;
			return res;
		}
		if (!in_->good())
			return res;
		if (c != '\r')
			res += static_cast<char>(c);
	}
}

void preprocessor_data::put(char c)
{
	if (skipping_)
		return;
	if (slowpath_) {
		strings_.back() += c;
		return;
	}
	if ((linenum_ != target_.linenum_ && c != '\n') ||
	    (linenum_ != target_.linenum_ + 1 && c == '\n')) {
		target_.linenum_ = linenum_;
		if (c == '\n')
			--target_.linenum_;

		target_.buffer_ << "\376line " << target_.linenum_
			<< ' ' << target_.location_ << '\n';
	}
	if (c == '\n')
		++target_.linenum_;
	target_.buffer_ << c;
}

void preprocessor_data::put(std::string const &s /*, int line_change*/)
{
	if (skipping_)
		return;
	if (slowpath_) {
		strings_.back() += s;
		return;
	}
	target_.buffer_ << s;
//	target_.linenum_ += line_change;
}

bool preprocessor_data::get_chunk()
{
	char c = in_->get();
	token_desc &token = tokens_.back();
	if (!in_->good()) {
		// The end of file was reached.
		// Make sure we don't have any incomplete tokens.
		char const *s;
		switch (token.type) {
		case '*': return false; // everything is fine
		case 'i':
		case 'I':
		case 'j':
		case 'J': s = "#ifdef or #ifndef"; break;
		case '"': s = "Quoted string"; break;
		case '[':
		case '{': s = "Macro substitution"; break;
		case '(': s = "Macro argument"; break;
		default: s = "???";
		}
		std::ostringstream error;
		error<<s<<" not terminated";
        std::ostringstream location;
		location<<linenum_<<' '<<target_.location_;
		pop_token();
		target_.error(error.str(), location.str());
	}
	if (c == '\n')
		++linenum_;
	if (c == '\376') {
		std::string buffer(1, c);
		for(;;) {
			char d = in_->get();
			if (!in_->good() || d == '\n')
				break;
			buffer += d;
		}
		buffer += '\n';
		// line_change = 1-1 = 0
		put(buffer);
	} else if (c == '"') {
		if (token.type == '"') {
			target_.quoted_ = false;
			put(c);
			if (!skipping_ && slowpath_) {
				std::string tmp = strings_.back();
				pop_token();
				strings_.back() += tmp;
			} else
				pop_token();
		} else if (!target_.quoted_) {
			target_.quoted_ = true;
			if (token.type == '{')
				token.type = '[';
			push_token('"');
			put(c);
		} else {
			std::string error="Nested quoted string";
	        std::ostringstream location;
			location<<linenum_<<' '<<target_.location_;
			target_.error(error, location.str());
		}
	} else if (c == '{') {
		if (token.type == '{')
			token.type = '[';
		push_token('{');
		++slowpath_;
	} else if (c == ')' && token.type == '(') {
		tokens_.pop_back();
		strings_.push_back(std::string());
	} else if (c == '#' && !target_.quoted_) {
		std::string command = read_word();
		bool comment = false;
		if (command == "define") {
			skip_spaces();
			int linenum = linenum_;
			std::vector< std::string > items = utils::split(read_line(), ' ');
			if (items.empty()) {
				std::string error="No macro name found after #define directive";
				std::ostringstream location;
				location<<linenum_<<' '<<target_.location_;
				target_.error(error, location.str());
			}
			std::string symbol = items.front();
			items.erase(items.begin());
			int found_enddef = 0;
			std::string buffer;
			for(;;) {
				if (!in_->good())
					break;
				char d = in_->get();
				if (d == '\n')
					++linenum_;
				buffer += d;
				if (d == '#')
					found_enddef = 1;
				else if (found_enddef > 0)
					if (++found_enddef == 7) {
						if (std::equal(buffer.end() - 6, buffer.end(), "enddef"))
							break;
						else
							found_enddef = 0;
					}
			}
			if (found_enddef != 7) {
				std::string error="Unterminated preprocessor definition at";
		        std::ostringstream location;
				location<<linenum_<<' '<<target_.location_;
				target_.error(error, location.str());
			}
			if (!skipping_) {
				buffer.erase(buffer.end() - 7, buffer.end());
				target_.defines_->insert(std::make_pair(
					symbol, preproc_define(buffer, items, target_.textdomain_,
					                       linenum + 1, target_.location_)));
				LOG_CF << "defining macro " << symbol << " (location " << target_.location_ << ")\n";
			}
		} else if (command == "ifdef") {
			skip_spaces();
			std::string const &symbol = read_word();
			bool skip = target_.defines_->count(symbol) == 0;
			DBG_CF << "testing for macro " << symbol << ": " << (skip ? "not defined" : "defined") << '\n';
			if (skip)
				++skipping_;
			push_token(skip ? 'J' : 'i');
		} else if (command == "ifndef") {
			skip_spaces();
			std::string const &symbol = read_word();
			bool skip = target_.defines_->count(symbol) != 0;
			DBG_CF << "testing for macro " << symbol << ": " << (skip ? "not defined" : "defined") << '\n';
			if (skip)
				++skipping_;
			push_token(skip ? 'J' : 'i');
		} else if (command == "else") {
			if (token.type == 'J') {
				pop_token();
				--skipping_;
				push_token('j');
			} else if (token.type == 'i') {
				pop_token();
				++skipping_;
				push_token('I');
			} else {
				std::string error="Unexpected #else at";
		        std::ostringstream location;
				location<<linenum_<<' '<<target_.location_;
				target_.error(error, location.str());
			}
		} else if (command == "endif") {
			switch (token.type) {
			case 'I':
			case 'J': --skipping_;
			case 'i':
			case 'j': break;
			default:
				std::string error="Unexpected #endif at";
		        std::ostringstream location;
				location<<linenum_<<' '<<target_.location_;
				target_.error(error, location.str());
			}
			pop_token();
		} else if (command == "textdomain") {
			skip_spaces();
			std::string const &s = read_word();
			put("#textdomain ");
			put(s);
			target_.textdomain_ = s;
			comment = true;
		} else if (command == "enddef") {
			std::string error="Unexpected #enddef at";
			std::ostringstream location;
			location<<linenum_<<' '<<target_.location_;
			target_.error(error, location.str());
		} else if (command == "undef") {
			skip_spaces();
			std::string const &symbol = read_word();
			target_.defines_->erase(symbol);
			LOG_CF << "undefine macro " << symbol << " (location " << target_.location_ << ")\n";
		} else
			comment = true;
		skip_eol();
		if (comment)
			put('\n');
	} else if (token.type == '{' || token.type == '[') {
		if (c == '(') {
			if (token.type == '[')
				token.type = '{';
			else
				strings_.pop_back();
			push_token('(');
		} else if (utils::portable_isspace(c)) {
			if (token.type == '[') {
				strings_.push_back(std::string());
				token.type = '{';
			}
		} else if (c == '}') {
			--slowpath_;
			if (skipping_) {
				pop_token();
				return true;
			}
			if (token.type == '{') {
				if (!strings_.back().empty()) {
					std::ostringstream error;
	        			std::ostringstream location;
					error << "Can't parse new macro parameter with a macro call scope open";
					location<<linenum_<<' '<<target_.location_;
					target_.error(error.str(), location.str());
				}
				strings_.pop_back();
			}

			std::string symbol = strings_[token.stack_pos];
			std::string::size_type pos;
			while ((pos = symbol.find('\376')) != std::string::npos) {
				std::string::iterator b = symbol.begin(); // invalidated at each iteration
				symbol.erase(b + pos, b + symbol.find('\n', pos + 1) + 1);
			}
			// If this is a known pre-processing symbol, then we insert it,
			// otherwise we assume it's a file name to load.
			preproc_map::const_iterator macro = target_.defines_->find(symbol);
			if(macro != target_.defines_->end()) {

/**
 * @todo it seems some addons use other names instead of include so disable the
 * code for now. Once 1.4 has been released we can try to fix it again or make
 * it mandatory to use INCLUDE for this purpose.
 *
 * ESR says: No, it was a fundamentally mistaken idea to do this here; wmllint
 * could do the same direct-recursion check without imposing runtime overhead
 * or causing the problems this code did.  Leaving this code in for
 * documentation purposes only, don't re-enable it.
 */
#if 0
				// INCLUDE is special and is allowed to be used recusively.
				if(symbol != "INCLUDE") {
				    for(std::vector<std::string>::iterator
							iter=called_macros_->begin();
							iter!=called_macros_->end(); ++iter) {
						if(*iter==symbol) {
							std::ostringstream error;
							error << "symbol '" << symbol << "' will cause a recursive macro call";
							std::ostringstream location;
							location<<linenum_<<' '<<target_.location_;
							target_.error(error.str(), location.str());
						}
					}
				}
#endif // 0
				preproc_define const &val = macro->second;
				size_t nb_arg = strings_.size() - token.stack_pos - 1;
				if (nb_arg != val.arguments.size()) {
                    std::ostringstream error;
					error << "preprocessor symbol '" << symbol << "' expects "
					      << val.arguments.size() << " arguments, but has "
					      << nb_arg << " arguments";
			        std::ostringstream location;
					location<<linenum_<<' '<<target_.location_;
					target_.error(error.str(), location.str());
				}

				std::stringstream *buffer = new std::stringstream;
				std::string::const_iterator i_bra = val.value.end();
				int macro_num = val.linenum;
				std::string macro_textdomain = val.textdomain;
				bool quoting = false;
				for(std::string::const_iterator i = val.value.begin(),
				    i_end = val.value.end(); i != i_end; ++i) {
					char c = *i;
					if (c == '\n') {
						++macro_num;
					} else if (c == '"') {
						quoting = !quoting;
					}

					if (c == '{') {
						if (i_bra != i_end)
							buffer->write(&*i_bra - 1, i - i_bra + 1);
						i_bra = i + 1;
					} else if (i_bra == i_end) {
						if (c == '#' && !quoting) {
							// Keep track of textdomain changes in the body of the
							// macro, so they can be restored after each substitution
							// of a macro argument.
							std::string::const_iterator i_beg = i + 1;
							if (i_end - i_beg >= 13 &&
							    std::equal(i_beg, i_beg + 10, "textdomain")) {
								i_beg += 10;
								i = std::find(i_beg, i_end, '\n');
								if (i_beg != i)
									++i_beg;
								macro_textdomain = std::string(i_beg, i);
								*buffer << "#textdomain " << macro_textdomain;
								++macro_num;
								c = '\n';
							} else if((i_end - i_beg < 6 || (!std::equal(i_beg, i_beg + 6, "define")
							&& !std::equal(i_beg, i_beg + 6, "ifndef")))
							&& (i_end - i_beg < 5 || (!std::equal(i_beg, i_beg + 5, "ifdef")
							&& !std::equal(i_beg, i_beg + 5, "endif") && !std::equal(i_beg, i_beg + 5, "undef")))
							&& (i_end - i_beg < 4 || !std::equal(i_beg, i_beg + 4, "else"))) {
								// Check for define, ifdef, ifndef, endif, undef, else.
								// Otherwise, this is a comment and should be skipped.
								i = std::find(i_beg, i_end, '\n');
								++macro_num;
								c = '\n';
							}
						}
						buffer->put(c);
					} else if (c == '}') {
						size_t sz = i - i_bra;
						for(size_t n = 0; n < nb_arg; ++n) {
							std::string const &arg = val.arguments[n];
							if (arg.size() != sz ||
							    !std::equal(i_bra, i, arg.begin()))
								continue;
							*buffer << strings_[token.stack_pos + n + 1]
							        << "\376line " << macro_num
							        << ' ' << val.location << "\n\376textdomain "
							        << macro_textdomain << '\n';
							i_bra = i_end;
							break;
						}
						if (i_bra != i_end) {
							// The bracketed text was no macro argument
							buffer->write(&*i_bra - 1, sz + 2);
							i_bra = i_end;
						}
					}
				}

				pop_token();
				std::string const &dir = directory_name(val.location.substr(0, val.location.find(' ')));
				if (!slowpath_) {
					DBG_CF << "substituting macro " << symbol << '\n';
					new preprocessor_data(target_, called_macros_, buffer, val.location, "",
					                      val.linenum, dir, val.textdomain, &symbol);
				} else {
					DBG_CF << "substituting (slow) macro " << symbol << '\n';
					std::ostringstream res;
					preprocessor_streambuf *buf =
						new preprocessor_streambuf(target_);
					{	std::istream in(buf);
						new preprocessor_data(*buf, called_macros_, buffer, val.location, "",
						                      val.linenum, dir, val.textdomain, &symbol);
						res << in.rdbuf(); }
					delete buf;
					strings_.back() += res.str();
				}
			} else if (target_.depth_ < 40) {
				LOG_CF << "Macro definition not found for " << symbol << " , attempting to open as file.\n";
				pop_token();
				std::string prefix;
				std::string nfname = get_wml_location(symbol, directory_);
				if (!nfname.empty())
				{
					if (!slowpath_)
						new preprocessor_file(target_, called_macros_, nfname);
					else {
						std::ostringstream res;
						preprocessor_streambuf *buf =
							new preprocessor_streambuf(target_);
						{	std::istream in(buf);
							new preprocessor_file(*buf, called_macros_, nfname);
							res << in.rdbuf(); }
						delete buf;
						strings_.back() += res.str();
					}
				}
				else
				{
					if (!slowpath_)
						put("dummy=" + symbol);
					else
						strings_.back() += "dummy=" + symbol;
					ERR_CF << "File not found '" << symbol << "'\n";
				}
			} else {
				ERR_CF << "Too much nested preprocessing inclusions at "
				       << linenum_ << ' ' << target_.location_
				       << ". Aborting.\n";
				pop_token();
			}
		} else {
			strings_.back() += c;
			token.type = '[';
		}
	} else
		put(c);
	return true;
}

struct preprocessor_deleter: std::basic_istream<char>
{
	preprocessor_streambuf *buf_;
	preproc_map *defines_;
	std::vector<std::string> *callstack_;
	std::string *error_log;
	preprocessor_deleter(preprocessor_streambuf *buf, preproc_map *defines, std::vector<std::string>*);
	~preprocessor_deleter();
};

preprocessor_deleter::preprocessor_deleter(preprocessor_streambuf *buf,
		preproc_map *defines,
		std::vector<std::string> *callstack)
	: std::basic_istream<char>(buf), buf_(buf), defines_(defines), callstack_(callstack), error_log(buf->error_log)
{
}

preprocessor_deleter::~preprocessor_deleter()
{
	rdbuf(NULL);
	delete buf_;
	delete defines_;
	delete callstack_;
}


std::istream *preprocess_file(std::string const &fname,
                              preproc_map *defines, std::string *error_log)
{
	log_scope("preprocessing file...");
	preproc_map *owned_defines = NULL;
	if (!defines) {
		// If no preproc_map has been given, create a new one,
		// and ensure it is destroyed when the stream is
// ??
		// by giving it to the deleter.
		owned_defines = new preproc_map;
		defines = owned_defines;
	}
	preprocessor_streambuf *buf = new preprocessor_streambuf(defines, error_log);
	std::vector<std::string> *callstack=new std::vector<std::string>;
	new preprocessor_file(*buf, callstack, fname);
	if(error_log!=NULL&&error_log->empty()==false)
		throw preproc_config::error("Error preprocessing files.");
	return new preprocessor_deleter(buf, owned_defines,callstack);
}

