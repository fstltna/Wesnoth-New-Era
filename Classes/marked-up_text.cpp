/* $Id: marked-up_text.cpp 36223 2009-06-15 12:27:49Z soliton $ */
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
 * @file marked-up_text.cpp
 * Support for simple markup in text (fonts, colors, images).
 * E.g. "@Victory" will be shown in green.
 */

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "global.hpp"

#include "font.hpp"
#include "gettext.hpp"
#include "marked-up_text.hpp"
#include "video.hpp"
#include "wml_exception.hpp"
#include <list>

namespace font {

const char /*LARGE_TEXT='*', */
			EXTRA_LARGE_TEXT=']',
		   LARGE_TEXT='*', SMALL_TEXT='`',
		   BOLD_TEXT='~',  NORMAL_TEXT='{',
		   NULL_MARKUP='^',
		   BLACK_TEXT='}', GRAY_TEXT='|',
           GOOD_TEXT='@',  BAD_TEXT='#',
           GREEN_TEXT='@', RED_TEXT='#',
           COLOR_TEXT='<', IMAGE='&';

std::string::const_iterator parse_markup(std::string::const_iterator i1,
												std::string::const_iterator i2,
												int* font_size,
												SDL_Color* colour, int* style)
{
	if (font_size == NULL && colour == NULL && style == NULL) {
		return i1;
	}

//	std::string::const_iterator i_start=i1;
	while(i1 != i2) {
		switch(*i1) {
		case '\\':
			// This must either be a quoted special character or a
			// quoted backslash - either way, remove leading backslash
			break;
		case BAD_TEXT:
			if (colour) *colour = BAD_COLOUR;
			break;
		case GOOD_TEXT:
			if (colour) *colour = GOOD_COLOUR;
			break;
		case NORMAL_TEXT:
			if (colour) *colour = NORMAL_COLOUR;
			break;
		case BLACK_TEXT:
			if (colour) *colour = BLACK_COLOUR;
			break;
		case GRAY_TEXT:
			if (colour) *colour = GRAY_COLOUR;
			break;
		case EXTRA_LARGE_TEXT:
			if (font_size) *font_size += 4;
			break;
		case LARGE_TEXT:
			if (font_size) *font_size += 2;
			break;
		case SMALL_TEXT:
			if (font_size) *font_size -= 2;
			break;
		case BOLD_TEXT:
			if (style) *style |= TTF_STYLE_BOLD;
			break;
		case NULL_MARKUP:
			return i1+1;
		case COLOR_TEXT:
			{
				std::string::const_iterator start = i1;
				// Very primitive parsing for rgb value
				// should look like <213,14,151>
				++i1;
				Uint8 red=0, green=0, blue=0, temp=0;
				while (i1 != i2 && *i1 >= '0' && *i1<='9') {
					temp*=10;
					temp += lexical_cast<int, char>(*i1);
					++i1;
				}
				red=temp;
				temp=0;
				if (i1 != i2 && ',' == (*i1)) {
					++i1;
					while(i1 != i2 && *i1 >= '0' && *i1<='9'){
						temp*=10;
						temp += lexical_cast<int, char>(*i1);
						++i1;
					}
					green=temp;
					temp=0;
				}
				if (i1 != i2 && ',' == (*i1)) {
					++i1;
					while(i1 != i2 && *i1 >= '0' && *i1<='9'){
						temp*=10;
						temp += lexical_cast<int, char>(*i1);
						++i1;
					}
				}
				blue=temp;
				if (i1 != i2 && '>' == (*i1)) {
					SDL_Color temp_color = {red, green, blue, 0};
					if (colour) *colour = temp_color;
				} else {
					// stop parsing and do not consume any chars
					return start;
				}
				if (i1 == i2) return i1;
				break;
			}
		default:
			return i1;
		}
		++i1;
	}
	return i1;
}

std::string del_tags(const std::string& text){
	int ignore_int;
	SDL_Color ignore_color;
	std::vector<std::string> lines = utils::split(text, '\n', 0);
	std::vector<std::string>::iterator line;
	for(line = lines.begin(); line != lines.end(); ++line) {
		std::string::const_iterator i1 = line->begin(),
			i2 = line->end();
		*line = std::string(parse_markup(i1,i2,&ignore_int,&ignore_color,&ignore_int),i2);
	}
	return utils::join(lines, '\n');
}

std::string nullify_markup(const std::string& text) {
	std::vector<std::string> lines = utils::split(text, '\n', 0);
	std::vector<std::string>::iterator line;
	for(line = lines.begin(); line != lines.end(); ++line) {
		*line = std::string() + NULL_MARKUP + *line;
	}
	return utils::join(lines, '\n');
}

std::string color2markup(const SDL_Color color) {
	std::stringstream markup;
	// The RGB of SDL_Color are Uint8, we need to cast them to int.
	// Without cast, it gives their char equivalent.
	markup << "<"
		   << static_cast<int>(color.r) << ","
		   << static_cast<int>(color.g) << ","
		   << static_cast<int>(color.b) << ">";
	return markup.str();
}

SDL_Rect text_area(const std::string& text, int size, int style)
{
	const SDL_Rect area = {0,0,10000,10000};
	return draw_text(NULL, area, size, font::NORMAL_COLOUR, text, 0, 0, false, style);
}

SDL_Rect draw_text_s(surface dst, const SDL_Rect& area, int size,
                   const SDL_Color& colour, const std::string& txt,
                   int x, int y, bool use_tooltips, int style)
{
	// Make sure there's always at least a space,
	// so we can ensure that we can return a rectangle for height
	static const std::string blank_text(" ");
	const std::string& text = txt.empty() ? blank_text : txt;

	SDL_Rect res;
	res.x = x;
	res.y = y;
	res.w = 0;
	res.h = 0;

	std::string::const_iterator i1 = text.begin();
	std::string::const_iterator i2 = std::find(i1,text.end(),'\n');
	for(;;) {
		SDL_Color col = colour;
		int sz = size;
		int text_style = style;

		i1 = parse_markup(i1,i2,&sz,&col,&text_style);

		if(i1 != i2) {
			std::string new_string(i1,i2);

			utils::unescape(new_string);

			const SDL_Rect rect = draw_text_line(dst, area, sz, col, new_string, x, y, use_tooltips, text_style);
			if(rect.w > res.w) {
				res.w = rect.w;
			}

			res.h += rect.h;
			y += rect.h;
		}

		if(i2 == text.end()) {
			break;
		}

		i1 = i2+1;
		i2 = std::find(i1,text.end(),'\n');
	}

	return res;
}

	
//std::map<const std::string, textCache> gTextCache;
std::list<textCache> gTextCache;

void free_cached_text(void)
{
	std::list<textCache>::iterator it;
	for (it=gTextCache.begin(); it != gTextCache.end(); it++)
	{
		//textCache tc = (*it);
		SDL_DestroyTexture(it->texture);
	}
	gTextCache.clear();
}
	
SDL_Rect draw_text(CVideo* gui, const SDL_Rect& area, int size,
                   const SDL_Color& colour, const std::string& txt,
                   int x, int y, bool use_tooltips, int style)
{
	// create a temporary surface to render to, then convert to texture and draw
	if (gui == NULL || txt.empty() || area.w <= 0 || area.h <= 0)
	{
		return draw_text_s(NULL, area, size, colour, txt, x, y, use_tooltips, style);
	}
	
	SDL_TextureID tex = 0;
	int w;
	int h;
	
	// LRU cache
	std::list<textCache>::iterator it;
	bool found = false;
	for (it = gTextCache.begin(); it != gTextCache.end(); it++)
	{
		if (it->text == txt && it->size == size && it->colour == colour && it->style == style)
		{
			found = true;
			break;
		}
	}
	
	if (found == true)
	{
		// move to back of the list
		textCache tc = (*it);
		gTextCache.erase(it);
		gTextCache.push_back(tc);
		tex = tc.texture;
		w = tc.w;
		h = tc.h;
	}
	else
	{
		surface surf = create_neutral_surface(area.w, area.h);
		SDL_Rect result = draw_text_s(surf, area, size, colour, txt, 0, 0, use_tooltips, style);
		surf = get_surface_portion(surf, result);
		result.x += x;
		result.y += y;
		tex = SDL_CreateTextureFromSurface(SDL_PIXELFORMAT_ABGR8888, surf);
		w = surf->w;
		h = surf->h;
		
		// take the place of the least used one (list head)
		textCache tc;
		if (gTextCache.size() >= MAX_TEXT_CACHE)
		{
			tc = gTextCache.front();
			SDL_DestroyTexture(tc.texture);
			gTextCache.pop_front();
		}
		tc.text = txt;
		tc.size = size;
		tc.colour = colour;
		tc.style = style;
		tc.texture = tex;
		tc.w = w;
		tc.h = h;
		gTextCache.push_back(tc);
	}
	
	
	SDL_Rect dst = {x, y, w, h};
	//SDL_RenderCopy(tex, NULL, &dst, DRAW);
	
	// KP: 20100228: obey clipping rectangle
	SDL_Rect src = {0, 0, w, h};
	
	SDL_Rect clip;
	
	getClipRect(&clip);
	
	// perform clipping
	SDL_Rect clippedDst;
	SDL_Rect clippedSrc;
	bool result = SDL_IntersectRect(&dst, &clip, &clippedDst);
	if (!result)
		return clippedDst;	// outside clip area
	
	clippedSrc = src;
	
	SDL_Rect clipAmount;
	clipAmount.x = clippedDst.x - dst.x;
	clipAmount.y = clippedDst.y - dst.y;
	clipAmount.w = dst.w - clippedDst.w;
	clipAmount.h = dst.h - clippedDst.h;
	
	textureRenderFlags flags = DRAW;	// KP: maybe this will change later for text...
	
	if ((flags & FLOP) != 0)
	{
		clippedSrc.x += (clipAmount.w-clipAmount.x);
		clippedSrc.w -= clipAmount.x;
	}
	else
	{
		clippedSrc.x += clipAmount.x;
		clippedSrc.w -= clipAmount.w;
	}
	if ((flags & FLIP) != 0)
	{
		clippedSrc.y += (clipAmount.h-clipAmount.y);
		clippedSrc.h -= clipAmount.y;
	}
	else
	{
		clippedSrc.y += clipAmount.y;
		clippedSrc.h -= clipAmount.h;
	}
	
	static unsigned long renderCount = 0;
	renderCount++;
	
	SDL_RenderCopy(tex, &clippedSrc, &clippedDst, flags);
	
	
	//SDL_DestroyTexture(tex);
	
	//return dst;
	return clippedDst;
}

bool is_format_char(char c)
{
	switch(c) {
	case EXTRA_LARGE_TEXT:
	case LARGE_TEXT:
	case SMALL_TEXT:
	case GOOD_TEXT:
	case BAD_TEXT:
	case NORMAL_TEXT:
	case BLACK_TEXT:
	case GRAY_TEXT:
	case BOLD_TEXT:
	case NULL_MARKUP:
		return true;
	default:
		return false;
	}
}

static void cut_word(std::string& line, std::string& word, int font_size, int style, int max_width)
{
	std::string tmp = line;
	utils::utf8_iterator tc(word);
	bool first = true;

	for(;tc != utils::utf8_iterator::end(word); ++tc) {
		tmp.append(tc.substr().first, tc.substr().second);
		SDL_Rect tsize = line_size(tmp, font_size, style);
		if(tsize.w > max_width) {
			const std::string& w = word;
			if(line.empty() && first) {
				line += std::string(w.begin(), tc.substr().second);
				word = std::string(tc.substr().second, w.end());
			} else {
				line += std::string(w.begin(), tc.substr().first);
				word = std::string(tc.substr().first, w.end());
			}
			break;
		}
		first = false;
	}
}

namespace {

/*
 * According to Kinsoku-Shori, Japanese rules about line-breaking:
 *
 * * the following characters cannot begin a line (so we will never break before them):
 * 、。，．）〕］｝〉》」』】’”ゝゞヽヾ々？！：；ぁぃぅぇぉゃゅょゎァィゥェォャュョヮっヵッヶ・…ー
 *
 * * the following characters cannot end a line (so we will never break after them):
 * （〔［｛〈《「『【‘“
 */
inline bool no_break_after(wchar_t ch)
{
	return
		ch == 0x2018 || ch == 0x201c || ch == 0x3008 || ch == 0x300a || ch == 0x300c ||
		ch == 0x300e || ch == 0x3010 || ch == 0x3014 || ch == 0xff08 || ch == 0xff3b ||
		ch == 0xff5b;

}

inline bool no_break_before(wchar_t ch)
{
	return
		ch == 0x2019 || ch == 0x201d || ch == 0x2026 || ch == 0x3001 || ch == 0x3002 ||
		ch == 0x3005 || ch == 0x3009 || ch == 0x300b || ch == 0x300d || ch == 0x300f ||
		ch == 0x3011 || ch == 0x3015 || ch == 0x3041 || ch == 0x3043 || ch == 0x3045 ||
		ch == 0x3047 || ch == 0x3049 || ch == 0x3063 || ch == 0x3083 || ch == 0x3085 ||
		ch == 0x3087 || ch == 0x308e || ch == 0x309d || ch == 0x309e || ch == 0x30a1 ||
		ch == 0x30a3 || ch == 0x30a5 || ch == 0x30a7 || ch == 0x30a9 || ch == 0x30c3 ||
		ch == 0x30e3 || ch == 0x30e5 || ch == 0x30e7 || ch == 0x30ee || ch == 0x30f5 ||
		ch == 0x30f6 || ch == 0x30fb || ch == 0x30fc || ch == 0x30fd || ch == 0x30fe ||
		ch == 0xff01 || ch == 0xff09 || ch == 0xff0c || ch == 0xff0e || ch == 0xff1a ||
		ch == 0xff1b || ch == 0xff1f || ch == 0xff3d || ch == 0xff5d;
}

inline bool break_before(wchar_t ch)
{
	if(no_break_before(ch))
		return false;

	return ch == ' ' ||
		// CKJ characters
		(ch >= 0x3000 && ch < 0xa000) ||
		(ch >= 0xf900 && ch < 0xfb00) ||
		(ch >= 0xff00 && ch < 0xfff0);
}

inline bool break_after(wchar_t ch)
{
	if(no_break_after(ch))
		return false;

	return ch == ' ' ||
		// CKJ characters
		(ch >= 0x3000 && ch < 0xa000) ||
		(ch >= 0xf900 && ch < 0xfb00) ||
		(ch >= 0xff00 && ch < 0xfff0);
}

} // end of anon namespace

std::string word_wrap_text(const std::string& unwrapped_text, int font_size, int max_width, int max_height, int max_lines)
{
	VALIDATE(max_width > 0, _("The maximum text width is less than 1."));

	utils::utf8_iterator ch(unwrapped_text);
	std::string current_word;
	std::string current_line;
	size_t line_width = 0;
	size_t current_height = 0;
	bool line_break = false;
	bool first = true;
	bool start_of_line = true;
	std::string wrapped_text;
	std::string format_string;
	SDL_Color color;
	int font_sz = font_size;
	int style = TTF_STYLE_NORMAL;
	utils::utf8_iterator end = utils::utf8_iterator::end(unwrapped_text);

	while(1) {
		if(start_of_line) {
			line_width = 0;
			format_string.clear();
			while(ch != end && *ch < static_cast<wchar_t>(0x100)
					&& is_format_char(*ch) && !ch.next_is_end()) {

				format_string.append(ch.substr().first, ch.substr().second);
				++ch;
			}
			// We need to parse the special format characters
			// to give the proper font_size and style to line_size()
			font_sz = font_size;
			style = TTF_STYLE_NORMAL;
			parse_markup(format_string.begin(),format_string.end(),&font_sz,&color,&style);
			current_line.clear();
			start_of_line = false;
		}

		// If there is no current word, get one
		if(current_word.empty() && ch == end) {
			break;
		} else if(current_word.empty()) {
			if(*ch == ' ' || *ch == '\n') {
				current_word = *ch;
				++ch;
			} else {
				wchar_t previous = 0;
				for(;ch != utils::utf8_iterator::end(unwrapped_text) &&
						*ch != ' ' && *ch != '\n'; ++ch) {

					if(!current_word.empty() &&
							break_before(*ch) &&
							!no_break_after(previous))
						break;

					if(!current_word.empty() &&
							break_after(previous) &&
							!no_break_before(*ch))
						break;

					current_word.append(ch.substr().first, ch.substr().second);

					previous = *ch;
				}
			}
		}

		if(current_word == "\n") {
			line_break = true;
			current_word.clear();
			start_of_line = true;
		} else {

			const size_t word_width = line_size(current_word, font_sz, style).w;

			line_width += word_width;

			if(static_cast<long>(line_width) > max_width) {
				if(static_cast<long>(word_width) > max_width) {
					cut_word(current_line,
						current_word, font_sz, style, max_width);
				}
				if(current_word == " ")
					current_word = "";
				line_break = true;
			} else {
				current_line += current_word;
				current_word = "";
			}
		}

		if(line_break || (current_word.empty() && ch == end)) {
			SDL_Rect size = line_size(current_line, font_sz, style);
			if(max_height > 0 && current_height + size.h >= size_t(max_height)) {
				return wrapped_text;
			}

			if(!first) {
				wrapped_text += '\n';
			}

			wrapped_text += format_string + current_line;
			current_line.clear();
			line_width = 0;
			current_height += size.h;
			line_break = false;
			first = false;

			if(--max_lines == 0) {
				return wrapped_text;
			}
		}
	}
	return wrapped_text;
}

SDL_Rect draw_wrapped_text(CVideo* gui, const SDL_Rect& area, int font_size,
		     const SDL_Color& colour, const std::string& text,
		     int x, int y, int max_width)
{
	std::string wrapped_text = word_wrap_text(text, font_size, max_width);
	return font::draw_text(gui, area, font_size, colour, wrapped_text, x, y, false);
}

size_t text_to_lines(std::string& message, size_t max_length)
{
	std::string starting_markup;
	bool at_start = true;

	size_t cur_line = 0, longest_line = 0;
	for(std::string::iterator i = message.begin(); i != message.end(); ++i) {
		if(at_start) {
			if(font::is_format_char(*i)) {
				starting_markup.push_back(*i);
			} else {
				at_start = false;
			}
		}

		if(*i == '\n') {
			at_start = true;
			starting_markup = "";
		}

		if(*i == ' ' && cur_line > max_length) {
			*i = '\n';
			const size_t index = i - message.begin();
			message.insert(index+1,starting_markup);
			i = message.begin() + index + starting_markup.size();

			if(cur_line > longest_line)
				longest_line = cur_line;

			cur_line = 0;
		}

		if(*i == '\n' || i+1 == message.end()) {
			if(cur_line > longest_line)
				longest_line = cur_line;

			cur_line = 0;

		} else {
			++cur_line;
		}
	}

	return longest_line;
}


} // end namespace font

