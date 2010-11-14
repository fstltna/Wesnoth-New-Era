/* $Id: canvas.cpp 33687 2009-03-15 16:50:42Z mordante $ */
/*
   Copyright (C) 2007 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file canvas.cpp
 * Implementation of canvas.hpp.
 */

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/widgets/canvas.hpp"

#include "config.hpp"
#include "../../image.hpp"
#include "gettext.hpp"
#include "gui/widgets/formula.hpp"
#include "gui/widgets/helper.hpp"
#include "../../text.hpp"
#include "wml_exception.hpp"

#include <stdio.h>

extern void blit_surface_scaled(int x, int y, int w, int h, surface surf, textureRenderFlags flags=DRAW);
extern void blit_surface(int x, int y, surface surf, SDL_Rect* srcrect=NULL, SDL_Rect* clip_rect=NULL, textureRenderFlags flags=DRAW);
extern void blit_texture_scaled(int x, int y, int w, int h, SDL_TextureID tex, textureRenderFlags flags=DRAW);
extern void blit_texture(int x, int y, SDL_TextureID tex, SDL_Rect* srcrect=NULL, SDL_Rect* clip_rect=NULL, textureRenderFlags flags=DRAW);
extern void draw_line(Uint32 color, int x1, int y1, int x2, int y2);
extern void fill_rect(Uint32 color, SDL_Rect *rect);

// KP: added timage caching
SDL_TextureID last_timage = 0;

std::string timage_cache_files[] = {
"buttons/downarrow-button.png",
"buttons/downarrow-button-disabled.png",
"buttons/downarrow-button-pressed.png",
"buttons/downarrow-button-active.png",
"buttons/uparrow-button.png",
"buttons/uparrow-button-disabled.png",
"buttons/uparrow-button-pressed.png",
"buttons/uparrow-button-active.png",
"buttons/left_arrow-button.png",
"buttons/left_arrow-button-disabled.png",
"buttons/left_arrow-button-pressed.png",
"buttons/left_arrow-button-active.png",
"buttons/right_arrow-button.png",
"buttons/right_arrow-button-disabled.png",
"buttons/right_arrow-button-pressed.png",
"buttons/right_arrow-button-active.png",
"buttons/button.png",
"buttons/button-disabled.png",
"buttons/button-pressed.png",
"buttons/button-active.png",
"buttons/scrollgroove-left.png",
"buttons/scrollgroove-horizontal.png",
"buttons/scrollgroove-right.png",
"buttons/scrollleft.png",
"buttons/scrollhorizontal.png",
"buttons/scrollright.png",
"buttons/scrollleft-disabled.png",
"buttons/scrollhorizontal-disabled.png",
"buttons/scrollright-disabled.png",
"buttons/scrollleft-pressed.png",
"buttons/scrollhorizontal-pressed.png",
"buttons/scrollright-pressed.png",
"buttons/scrollleft-active.png",
"buttons/scrollhorizontal-active.png",
"buttons/scrollright-active.png",
"dialogs/translucent65-border-top.png",
"dialogs/translucent65-border-bottom.png",
"dialogs/translucent65-background.png",
"buttons/slider.png",
"buttons/slider-disabled.png",
"buttons/slider-selected.png",
"buttons/slider-active.png",
"buttons/checkbox.png",
"buttons/checkbox.png",
"buttons/checkbox-active.png",
"buttons/checkbox-pressed.png",
"buttons/checkbox-pressed.png",
"buttons/checkbox-active-pressed.png",
"misc/selection2-border-topleft.png",
"misc/selection2-border-topright.png",
"misc/selection2-border-botleft.png",
"misc/selection2-border-botright.png",
"misc/selection2-border-top.png",
"misc/selection2-border-bottom.png",
"misc/selection2-border-left.png",
"misc/selection2-border-right.png",
"misc/selection2-background.png",
"misc/selection-border-topleft.png",
"misc/selection-border-topright.png",
"misc/selection-border-botleft.png",
"misc/selection-border-botright.png",
"misc/selection-border-top.png",
"misc/selection-border-bottom.png",
"misc/selection-border-left.png",
"misc/selection-border-right.png",
"misc/selection-background.png",
"buttons/scrollgroove-top.png",
"buttons/scrollgroove-mid.png",
"buttons/scrollgroove-bottom.png",
"buttons/scrolltop.png",
"buttons/scrollmid.png",
"buttons/scrollbottom.png",
"buttons/scrolltop-disabled.png",
"buttons/scrollmid-disabled.png",
"buttons/scrollbottom-disabled.png",
"buttons/scrolltop-pressed.png",
"buttons/scrollmid-pressed.png",
"buttons/scrollbottom-pressed.png",
"buttons/scrolltop-active.png",
"buttons/scrollmid-active.png",
"buttons/scrollbottom-active.png",
"dialogs/opaque-border-topleft.png",
"dialogs/opaque-border-top.png",
"dialogs/opaque-border-topright.png",
"dialogs/opaque-border-right.png",
"dialogs/opaque-border-botright.png",
"dialogs/opaque-border-bottom.png",
"dialogs/opaque-border-botleft.png",
"dialogs/opaque-border-left.png",
"dialogs/opaque-background.png",
"dialogs/translucent65-border-topleft.png",
"dialogs/translucent65-border-top.png",
"dialogs/translucent65-border-topright.png",
"dialogs/translucent65-border-right.png",
"dialogs/translucent65-border-botright.png",
"dialogs/translucent65-border-bottom.png",
"dialogs/translucent65-border-botleft.png",
"dialogs/translucent65-border-left.png",
"dialogs/translucent65-background.png",
"dialogs/translucent54-border-topleft.png",
"dialogs/translucent54-border-top.png",
"dialogs/translucent54-border-topright.png",
"dialogs/translucent54-border-right.png",
"dialogs/translucent54-border-botright.png",
"dialogs/translucent54-border-bottom.png",
"dialogs/translucent54-border-botleft.png",
"dialogs/translucent54-border-left.png",
"dialogs/translucent54-background.png",
""
};

std::map<std::string, SDL_TextureID> timage_cache;

namespace gui2 {

namespace {


/*WIKI
 * @page = GUICanvasWML
 *
 * THIS PAGE IS AUTOMATICALLY GENERATED, DO NOT MODIFY DIRECTLY !!!
 *
 * = Canvas =
 *
 * A canvas is a blank drawing area on which the user can add various items.
 *
 */

/***** ***** ***** ***** ***** LINE ***** ***** ***** ***** *****/

/** Definition of a line shape. */
class tline : public tcanvas::tshape
{
public:

	/**
	 * Constructor.
	 *
	 * @param cfg                 The config object to define the line see
	 *                            http://www.wesnoth.org/wiki/GUICanvasWML#Line
	 *                            for more info.
	 */
	tline(const config& cfg);

	/** Implement shape::draw(). */
	void draw(//surface& canvas,
			  int xOffset, int yOffset,
		const game_logic::map_formula_callable& variables);

private:
	tformula<unsigned>
		x1_, /**< The start x coordinate of the line. */
		y1_, /**< The start y coordinate of the line. */
		x2_, /**< The end x coordinate of the line. */
		y2_; /**< The end y coordinate of the line. */

	/** The colour of the line. */
	Uint32 colour_;

	/**
	 * The thickness of the line.
	 *
	 * if the value is odd the x and y are the middle of the line.
	 * if the value is even the x and y are the middle of a line
	 * with width - 1. (0 is special case, does nothing.)
	 */
	unsigned thickness_;
};

tline::tline(const config& cfg) :
	x1_(cfg["x1"]),
	y1_(cfg["y1"]),
	x2_(cfg["x2"]),
	y2_(cfg["y2"]),
	colour_(decode_colour(cfg["colour"])),
	thickness_(lexical_cast_default<unsigned>(cfg["thickness"]))
{
/*WIKI
 * @page = GUICanvasWML
 *
 * == Line ==
 * Definition of a line. When drawing a line it doesn't get blended on the
 * surface but replaces the pixels instead. A blitting flag might be added later
 * if needed.
 *
 * Keys:
 * @start_table = config
 *     x1 (f_unsigned = 0)             The x coordinate of the startpoint.
 *     y1 (f_unsigned = 0)             The y coordinate of the startpoint.
 *     x2 (f_unsigned = 0)             The x coordinate of the endpoint.
 *     y2 (f_unsigned = 0)             The y coordinate of the endpoint.
 *     colour (colour = "")            The colour of the line.
 *     thickness = (unsigned = 0)      The thickness of the line if 0 nothing
 *                                     is drawn.
 *     debug = (string = "")           Debug message to show upon creation
 *                                     this message is not stored.
 * @end_table
 *
 * <span id="general_variables">Variables:</span>.
 * @start_table = formula
 *     width unsigned                  The width of the canvas.
 *     height unsigned                 The height of the canvas.
 *     text tstring                    The text to render on the widget.
 *     text_maximum_width unsigned     The maximum width available for the text
 *                                     on the widget.
 *     text_maximum_height unsigned    The maximum height available for the text
 *                                     on the widget.
 *     text_wrap_mode int              When the text doesn't fit in the
 *                                     available width there are serveral ways
 *                                     to fix that. This variable holds the
 *                                     best method. (NOTE this is a 'hidden'
 *                                     variable meant to copy state from a
 *                                     widget to its canvas so there's no
 *                                     reason to use this variable and thus
 *                                     it's values are not listed and might
 *                                     change without further notice.)
 *@end_table
 *
 * Note when drawing the valid coordinates are:<br>
 * 0 -> width - 1 <br>
 * 0 -> height -1
 *
 * Drawing outside this area will result in unpredictable results including
 * crashing. (That should be fixed, when encountered.)
 *
 */

/*WIKI - unclassified
 * This code can be used by a parser to generate the wiki page
 * structure
 * [tag name]
 * param type_info description
 *
 * param                               Name of the parameter.
 *
 * type_info = ( type = default_value) The info about a optional parameter.
 * type_info = ( type )                The info about a mandatory parameter
 * type_info = [ type_info ]           The info about a conditional parameter
 *                                     description should explain the reason.
 *
 * description                         Description of the parameter.
 *
 *
 *
 *
 * Formulas are a function between brackets, that way the engine can see whether
 * there is standing a plain number or a formula eg:
 * 0     A value of zero
 * (0)   A formula returning zero
 *
 * When formulas are available the text should state the available variables
 * which are available in that function.
 */

/*WIKI
 * @page = GUIVariable
 *
 * THIS PAGE IS AUTOMATICALLY GENERATED, DO NOT MODIFY DIRECTLY !!!
 *
 * = Variables =
 *
 * In various parts of the GUI there are several variables types in use. This
 * page describes them.
 *
 * == Simple types ==
 *
 * The simple types are types which have one value or a short list of options.
 *
 * @start_table = variable_types
 *     unsigned                        Unsigned number (positive whole numbers
 *                                     and zero).
 *     f_unsigned                      Unsigned number or formula returning an
 *                                     unsigned number.
 *     int                             Signed number (whole numbers).
 *     f_int                           Signed number or formula returning an
 *                                     signed number.
 *     bool                            A boolean value accepts the normal
 *                                     values as the rest of the game.
 *     f_bool                          Boolean value or a formula returning a
 *                                     boolean value.
 *     string                          A text.
 *     tstring                         A translatable string.
 *     f_tstring                       Formula returning a translatable string.
 *
 *     colour                          A string which contains the colour, this
 *                                     a group of 4 numbers between 0 and 255
 *                                     separated by a comma. The numbers are red
 *                                     component, green component, blue
 *                                     component and alpha. A colour of 0 is not
 *                                     available. An alpha of 255 is fully
 *                                     transparent. Omitted values are set to 0.
 *
 *     font_style                      A string which contains the style of the
 *                                     font:
 *                                     @* normal    normal font
 *                                     @* bold      bold font
 *                                     @* italic    italic font
 *                                     @* underline underlined font
 *                                     @-Since SDL has problems combining these
 *                                     styles only one can be picked. Once SDL
 *                                     will allow multiple options, this type
 *                                     will be transformed to a comma separated
 *                                     list. If empty we default to the normal
 *                                     style.
 *
 *     v_align                         Vertical alignment; how an item is
 *                                     aligned vertically in the available
 *                                     space. Possible values:
 *                                     @* top    aligned at the top
 *                                     @* bottom aligned at the bottom
 *                                     @* center centered
 *                                     @-When nothing is set or an another
 *                                     value as in the list the item is
 *                                     centred.
 *
 *     h_align                         Horizontal alignment; how an item is
 *                                     aligned horizontal in the available
 *                                     space. Possible values:
 *                                     @* top    aligned at the top
 *                                     @* bottom aligned at the bottom
 *                                     @* center centered
 *
 *     border                          Comma separated list of borders to use.
 *                                     Possible values:
 *                                     @* left   border at the left side
 *                                     @* right  border at the right side
 *                                     @* top    border at the top
 *                                     @* bottom border at the bottom
 *                                     @* all    alias for "left, right, top,
 *                                     bottom"
 *
 *     scrollbar_mode                  How to show the scrollbar of a widget.
 *                                     Possible values:
 *                                     @* always The scrollbar is always shown,
 *                                     regardless whether it's required or not.
 *                                     @* never  The scrollbar is never shown,
 *                                     even not when needed. (Note when setting
 *                                     this mode dialogs might not properly fit
 *                                     anymore).
 *                                     @* auto   Shows the scrollbar when
 *                                     needed. The widget will reserve space for
 *                                     the scrollbar, but only show when needed.
 * @end_table
 *
 * == Section types ==
 *
 * For more complex parts, there are sections. Sections contain of several
 * lines of WML and can have sub sections. For example a grid has sub sections
 * which contain various widgets. Here's the list of sections.
 *
 * @start_table = variable_types
 *     section                         A generic section. The documentation
 *                                     about the section should describe the
 *                                     section in further detail.
 *
 *     grid                            A grid contains serveral widgets. (TODO
 *                                     add link to generic grid page.)
 * @end_table
 */

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Line: found debug message '" << debug << "'.\n";
	}
}

void tline::draw(//surface& canvas,
				 int xOffset, int yOffset,
	const game_logic::map_formula_callable& variables)
{
	/**
	 * @todo formulas are now recalculated every draw cycle which is a bit silly
	 * unless there has been a resize. So to optimize we should use an extra
	 * flag or do the calculation in a separate routine.
	 */

	const unsigned x1 = x1_(variables) + xOffset;
	const unsigned y1 = y1_(variables) + yOffset;
	const unsigned x2 = x2_(variables) + xOffset;
	const unsigned y2 = y2_(variables) + yOffset;

//	DBG_G_D << "Line: draw from "
//		<< x1 << ',' << y1 << " to " << x2 << ',' << y2
//		<< " canvas size " << canvas->w << ',' << canvas->h << ".\n";

//	VALIDATE(static_cast<int>(x1) < canvas->w &&
//		 static_cast<int>(x2) < canvas->w &&
//		 static_cast<int>(y1) < canvas->h &&
//		 static_cast<int>(y2) < canvas->h,
//		 _("Line doesn't fit on canvas."));

	// @todo FIXME respect the thickness.

	// now draw the line we use Bresenham's algorithm, which doesn't
	// support antialiasing. The advantage is that it's easy for testing.

	// lock the surface
//	surface_lock locker(canvas);
	
	if(x1 > x2) {
		// invert points
		//draw_line(canvas, colour_, x2, y2, x1, y1);
		::draw_line(colour_, x2, y2, x1, y1);
	} else {
		//draw_line(canvas, colour_, x1, y1, x2, y2);
		::draw_line(colour_, x1, y1, x2, y2);
	}
}

/***** ***** ***** ***** ***** Rectangle ***** ***** ***** ***** *****/

/** Definition of a rectangle shape. */
class trectangle : public tcanvas::tshape
{
public:

	/**
	 * Constructor.
	 *
	 * @param cfg                 The config object to define the rectangle see
	 *                            http://www.wesnoth.org/wiki/GUICanvasWML#Rectangle
	 *                            for more info.
	 */
	trectangle(const config& cfg);

	/** Implement shape::draw(). */
	void draw(//surface& canvas,
			  int xOffset, int yOffset,
		const game_logic::map_formula_callable& variables);

private:
	tformula<unsigned>
		x_, /**< The x coordinate of the rectangle. */
		y_, /**< The y coordinate of the rectangle. */
		w_, /**< The width of the rectangle. */
		h_; /**< The height of the rectangle. */

	/**
	 * Border thickness.
	 *
	 * If 0 the fill colour is used for the entire widget.
	 */
	unsigned border_thickness_;

	/**
	 * The border colour of the rectangle.
	 *
	 * If the colour is fully transparent the border isn't drawn.
	 */
	Uint32 border_colour_;

	/**
	 * The border colour of the rectangle.
	 *
	 * If the colour is fully transparent the rectangle won't be filled.
	 */
	Uint32 fill_colour_;
};

trectangle::trectangle(const config& cfg) :
	x_(cfg["x"]),
	y_(cfg["y"]),
	w_(cfg["w"]),
	h_(cfg["h"]),
	border_thickness_(lexical_cast_default<unsigned>(cfg["border_thickness"])),
	border_colour_(decode_colour(cfg["border_colour"])),
	fill_colour_(decode_colour(cfg["fill_colour"]))
{
/*WIKI
 * @page = GUICanvasWML
 *
 * == Rectangle ==
 *
 * Definition of a rectangle. When drawing a rectangle it doesn't get blended on
 * the surface but replaces the pixels instead. A blitting flag might be added
 * later if needed.
 *
 * Keys:
 * @start_table = config
 *     x (f_unsigned = 0)              The x coordinate of the top left corner.
 *     y (f_unsigned = 0)              The y coordinate of the top left corner.
 *     w (f_unsigned = 0)              The width of the rectangle.
 *     h (f_unsigned = 0)              The height of the rectangle.
 *     border_thickness (unsigned = 0) The thickness of the border if the
 *                                     thickness is zero it's not drawn.
 *     border_colour (colour = "")     The colour of the border if empty it's
 *                                     not drawn.
 *     fill_colour (colour = "")       The colour of the interior if omitted
 *                                     it's not drawn.
 *     debug = (string = "")           Debug message to show upon creation
 *                                     this message is not stored.
 * @end_table
 * Variables:
 * See [[#general_variables|Line]].
 *
 */
	if(border_colour_ == 0) {
		border_thickness_ = 0;
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Rectangle: found debug message '" << debug << "'.\n";
	}
}

Uint32 RGBA_to_ARGB(Uint32 rgba)
{
	Uint32 argb = (rgba >> 8) & 0xffffff;
	argb = argb | ((rgba & 0xff) << 24);
	return argb;
}
	
void trectangle::draw(//surface& canvas,
					  int xOffset, int yOffset,
	const game_logic::map_formula_callable& variables)
{

	/**
	 * @todo formulas are now recalculated every draw cycle which is a  bit
	 * silly unless there has been a resize. So to optimize we should use an
	 * extra flag or do the calculation in a separate routine.
	 */
	const unsigned x = x_(variables) + xOffset;
	const unsigned y = y_(variables) + yOffset;
	const unsigned w = w_(variables);
	const unsigned h = h_(variables);
	
	if (w==0 || h == 0)
		return;

//	DBG_G_D << "Rectangle: draw from " << x << ',' << y
//		<< " width " << w << " height " << h
//		<< " canvas size " << canvas->w << ',' << canvas->h << ".\n";

//	VALIDATE(static_cast<int>(x) < canvas->w &&
//		 static_cast<int>(x + w) <= canvas->w &&
//		 static_cast<int>(y) < canvas->h &&
//		 static_cast<int>(y + h) <= canvas->h,
//		 _("Rectangle doesn't fit on canvas."));


//	surface_lock locker(canvas);

	// convert RGBA to ARGB
	Uint32 bc = RGBA_to_ARGB(border_colour_);
	
	// draw the border
	for(unsigned i = 0; i < border_thickness_; ++i) {

		const unsigned left = x + i;
		const unsigned right = left + w - (i * 2) - 1;
		const unsigned top = y + i;
		const unsigned bottom = top + h - (i * 2) - 1;

		// top horizontal (left -> right)
		//draw_line(canvas, border_colour_, left, top, right, top);
		::draw_line(bc, left, top, right, top);

		// right vertical (top -> bottom)
		//draw_line(canvas, border_colour_, right, top, right, bottom);
		::draw_line(bc, right, top, right, bottom);

		// bottom horizontal (left -> right)
		//draw_line(canvas, border_colour_, left, bottom, right, bottom);
		::draw_line(bc, left, bottom, right, bottom);

		// left vertical (top -> bottom)
		//draw_line(canvas, border_colour_, left, top, left, bottom);
		::draw_line(bc, left, top, left, bottom);

	}

	// The fill_rect_alpha code below fails, can't remember the exact cause
	// so use the slow line drawing method to fill the rect.
	Uint32 fc = RGBA_to_ARGB(fill_colour_);
	if(fc) {

		const unsigned left = x + border_thickness_;
		const unsigned right = left + w - (2 * border_thickness_) - 1;
		const unsigned top = y + border_thickness_;
		const unsigned bottom = top + h - (2 * border_thickness_);

		
		//for(unsigned i = top; i < bottom; ++i) {
		//	draw_line(canvas, fill_colour_, left, i, right, i);
		//}
		SDL_Rect r = {left, top, right-left, bottom-top};
		::fill_rect(fc, &r);
	}
}

/***** ***** ***** ***** ***** IMAGE ***** ***** ***** ***** *****/

/** Definition of an image shape. */
class timage : public tcanvas::tshape
{
public:

	/**
	 * Constructor.
	 *
	 * @param cfg                 The config object to define the image see
	 *                            http://www.wesnoth.org/wiki/GUICanvasWML#Image
	 *                            for more info.
	 */
	timage(const config& cfg);

	/** Implement shape::draw(). */
	void draw(//surface& canvas,
			  int xOffset, int yOffset,
		const game_logic::map_formula_callable& variables);
	
	// KP: need to do this to free the texture
	/*
	virtual ~timage()
	{
		printf("~timage()\n");
		if (image_texture_ != 0)
		{
			SDL_DestroyTexture(image_texture_);
		}
	}
	 */
	// could never get the destructor to actually be called.... memory leaks somewhere??

private:
	tformula<unsigned>
		x_, /**< The x coordinate of the image. */
		y_, /**< The y coordinate of the image. */
		w_, /**< The width of the image. */
		h_; /**< The height of the image. */

	/** Contains the size of the image. */
	SDL_Rect src_clip_;

	/** The image is cached in this surface. */
	//surface image_;
	SDL_TextureID image_texture_;

	/**
	 * Name of the image.
	 *
	 * This value is only used when the image name is a formula. If it isn't a
	 * formula the image will be loaded at construction. If a formula it will
	 * be loaded every draw cycles. This allows 'changing' images.
	 */
	tformula<shared_string> image_name_;

	/**
	 * When an image needs to be scaled in one direction there are two options:
	 * - scale, which interpolates the image.
	 * - stretch, which used the first row/column and copies those pixels.
	 */
	bool stretch_;

	/** Mirror the image over the vertical axis. */
	tformula<bool> vertical_mirror_;
};

timage::timage(const config& cfg) :
	x_(cfg["x"]),
	y_(cfg["y"]),
	w_(cfg["w"]),
	h_(cfg["h"]),
	src_clip_(),
	image_texture_(0),
	image_name_(cfg["name"]),
	stretch_(utils::string_bool(cfg["stretch"])),
	vertical_mirror_(cfg["vertical_mirror"])
{
/*WIKI
 * @page = GUICanvasWML
 *
 * == Image ==
 * Definition of an image.
 *
 * Keys:
 * @start_table = config
 *     x (f_unsigned = 0)              The x coordinate of the top left corner.
 *     y (f_unsigned = 0)              The y coordinate of the top left corner.
 *     w (f_unsigned = 0)              The width of the image, if not zero the
 *                                     image will be scaled to the desired width.
 *     h (f_unsigned = 0)              The height of the image, if not zero the
 *                                     image will be scaled to the desired height.
 *     stretch (bool = false)          Border images often need to be either
 *                                     stretched in the width or the height. If
 *                                     that's the case use stretch. It only works
 *                                     if only the height or the width is not zero.
 *                                     It will copy the first pixel to the others.
 *     vertical_mirror (f_bool = false)
 *                                     Mirror the image over the vertical axis.
 *     name (f_string = "")            The name of the image.
 *     debug = (string = "")           Debug message to show upon creation
 *                                     this message is not stored.
 *
 * @end_table
 * Variables:
 * @start_table = formula
 *     image_width unsigned             The width of the image, either the
 *                                      requested width or the natural width of
 *                                      the image. This value can be used to set
 *                                      the x (or y) value of the image. (This
 *                                      means x and y are evaluated after the
 *                                      width and height.)
 *     image_height unsigned            The height of the image, either the
 *                                      requested height or the natural height of
 *                                      the image. This value can be used to set
 *                                      the y (or x) value of the image. (This
 *                                      means x and y are evaluated after the
 *                                      width and height.)
 *     image_original_width unsigned    The width of the image as stored on
 *                                      disk, can be used to set x or w
 *                                      (also y and h can be set).
 *     image_original_height unsigned   The height of the image as stored on
 *                                      disk, can be used to set y or h
 *                                      (also x and y can be set).
 * @end_table
 * Also the general variables are available, see [[#general_variables|Line]].
 *
 */
	if(!image_name_.has_formula()) {
		
		if(timage_cache.size() == 0)
		{
			// build the cache for the first time
			int i=0;
			while (timage_cache_files[i] != "")
			{
				// KP: do not cache this
				surface tmp(image::get_image(image::locator(timage_cache_files[i]), image::UNSCALED, false));				
				
				surface image_;
				image_.assign(make_neutral_surface(tmp));
				assert(image_);
				std::cerr << "Caching texture for timage " << timage_cache_files[i] << "\n";
				SDL_TextureID tid = SDL_CreateTextureFromSurface(SDL_PIXELFORMAT_ABGR8888, image_);
				timage_cache.insert(std::pair<std::string, SDL_TextureID>(timage_cache_files[i], tid));
				i++;
			}
		}
		
				
		std::string str = image_name_();
		
		std::map<std::string, SDL_TextureID>::iterator it;
		it = timage_cache.find(str);
		if (it != timage_cache.end())
		{
			image_texture_ = (*it).second;
			std::cerr << "Loaded from cache for timage " << str << "\n";
		}
		else
		{
			// KP: avoid texture leaks
			if (last_timage != 0)
			{
				SDL_DestroyTexture(last_timage);
				last_timage = 0;
			}
			
			
			// KP: do not cache this
			surface tmp(image::get_image(image::locator(str), image::UNSCALED, false));

			if(!tmp) {
				ERR_G_D << "Image: '" << str
					<< "'not found and won't be drawn.\n";
				return;
			}

			surface image_;
			image_.assign(make_neutral_surface(tmp));
			assert(image_);
			std::cerr << "Creating texture for timage " << str << "\n";
			image_texture_ = SDL_CreateTextureFromSurface(SDL_PIXELFORMAT_ABGR8888, image_);
			src_clip_ = ::create_rect(0, 0, image_->w, image_->h);
			last_timage = image_texture_;
		}
	}

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Image: found debug message '" << debug << "'.\n";
	}
}

	
void timage::draw(//surface& canvas,
				  int xOffset, int yOffset,
	const game_logic::map_formula_callable& variables)
{
	DBG_G_D << "Image: draw.\n";

	/**
	 * @todo formulas are now recalculated every draw cycle which is a  bit
	 * silly unless there has been a resize. So to optimize we should use an
	 * extra flag or do the calculation in a separate routine.
	 */
	if(image_name_.has_formula()) {
		const shared_string& name = image_name_(variables);

		if(name.empty()) {
			DBG_G_D << "Image: formula returned no value, will not be drawn.\n";
			return;
		}

		// KP: do not cache this
		surface tmp(image::get_image(image::locator(name), image::UNSCALED, false));

		if(!tmp) {
			ERR_G_D << "Image: formula returned name '"
				<< name << "'not found and won't be drawn.\n";
			return;
		}

		// KP: avoid texture leaks
		if (last_timage != 0)
		{
			SDL_DestroyTexture(last_timage);
			last_timage = 0;
		}
		
		surface image_;
		image_.assign(make_neutral_surface(tmp));
		assert(image_);
		std::cerr << "Creating texture for FORMULA timage " << name << "\n";
		image_texture_ = SDL_CreateTextureFromSurface(SDL_PIXELFORMAT_ABGR8888, image_);
		src_clip_ = ::create_rect(0, 0, image_->w, image_->h);
		last_timage = image_texture_;
		
	} else if(!image_texture_){
		// The warning about no image should already have taken place
		// so leave silently.
		return;
	}

	game_logic::map_formula_callable local_variables(variables);
	int imgw, imgh;
	SDL_QueryTexture(image_texture_, NULL, NULL, &imgw, &imgh);
	local_variables.add("image_original_width", variant(imgw));
	local_variables.add("image_original_height", variant(imgh));

	unsigned w = w_(local_variables);
	unsigned h = h_(local_variables);

	local_variables.add("image_width", variant(w ? w : imgw));
	local_variables.add("image_height", variant(h ? h : imgh));
	const unsigned x = x_(local_variables) + xOffset;
	const unsigned y = y_(local_variables) + yOffset;

	// Copy the data to local variables to avoid overwriting the originals.
	SDL_Rect src_clip = src_clip_;
	SDL_Rect dst_clip = {x, y, 0, 0};
//	surface surf;

	// Test whether we need to scale and do the scaling if needed.
	if(w || h) {
		bool done = false;
		// @TODO: KP: sometimes we do want to stretch, and not scale...
//		bool stretch = stretch_ && (!!w ^ !!h);
	/*	
		if(!w) {
			if(stretch) {
				DBG_G_D << "Image: vertical stretch from " << image_->w
					<< ',' << image_->h << " to a height of " << h << ".\n";

				surf = stretch_surface_vertical(image_, h, false);
				done = true;
			}
			w = image_->w;
		}

		if(!h) {
			if(stretch) {
				DBG_G_D << "Image: horizontal stretch from " << image_->w
					<< ',' << image_->h << " to a width of " << w << ".\n";

				surf = stretch_surface_horizontal(image_, w, false);
				done = true;
			}
			h = image_->h;
		}
*/
		if(!done) {

			DBG_G_D << "Image: scaling from " << imgw
				<< ',' << imgh << " to " << w << ',' << h << ".\n";

			//surf = scale_surface(image_, w, h, false);
			// KP: better openGL rendering support
			w = w ? w : imgw;
			h = h ? h : imgh;
			if (!vertical_mirror_(local_variables))
			{
				//::blit_surface_scaled(dst_clip.x, dst_clip.y, w, h, image_);
				::blit_texture_scaled(dst_clip.x, dst_clip.y, w, h, image_texture_);
				return;
			}
			else
			{
				::blit_texture_scaled(dst_clip.x, dst_clip.y, w, h, image_texture_, FLIP);
				return;
			}
//			surf = scale_surface(image_, w, h, false);
		}
		src_clip.w = w;
		src_clip.h = h;
	} else {
//		surf = image_;
	}

	if(vertical_mirror_(local_variables)) {
		//surf = flip_surface(surf, false);
		::blit_texture(dst_clip.x, dst_clip.y, image_texture_, &src_clip, NULL, FLIP);
		return;
	}

//	blit_surface(surf, &src_clip, canvas, &dst_clip);
//	::blit_surface(dst_clip.x, dst_clip.y, surf, &src_clip);
	::blit_texture(dst_clip.x, dst_clip.y, image_texture_, &src_clip);
}


/***** ***** ***** ***** ***** TEXT ***** ***** ***** ***** *****/

/** Definition of a text shape. */
class ttext : public tcanvas::tshape
{
public:

	/**
	 * Constructor.
	 *
	 * @param cfg                 The config object to define the text see
	 *                            http://www.wesnoth.org/wiki/GUICanvasWML#Text
	 *                            for more info.
	 */
	ttext(const config& cfg);

	/** Implement shape::draw(). */
	void draw(//surface& canvas,
			  int xOffset, int yOffset,
		const game_logic::map_formula_callable& variables);

private:
	tformula<unsigned>
		x_, /**< The x coordinate of the text. */
		y_, /**< The y coordinate of the text. */
		w_, /**< The width of the text. */
		h_; /**< The height of the text. */

	/** The font size of the text. */
	unsigned font_size_;

	/** The style of the text. */
	unsigned font_style_;

	/** The colour of the text. */
	Uint32 colour_;

	/** The text to draw. */
	tformula<t_string> text_;

	/** The text markup switch of the text. */
	tformula<bool> text_markup_;

	/** The maximum width for the text. */
	tformula<int> maximum_width_;

	/** The maximum height for the text. */
	tformula<int> maximum_height_;
};

ttext::ttext(const config& cfg) :
	x_(cfg["x"]),
	y_(cfg["y"]),
	w_(cfg["w"]),
	h_(cfg["h"]),
	font_size_(lexical_cast_default<unsigned>(cfg["font_size"])),
	font_style_(decode_font_style(cfg["font_style"])),
	colour_(decode_colour(cfg["colour"])),
	text_(cfg["text"]),
	text_markup_(cfg["text_markup"], false),
	maximum_width_(cfg["maximum_width"], -1),
	maximum_height_(cfg["maximum_height"], -1)
{

/*WIKI
 * @page = GUICanvasWML
 *
 * == Text ==
 * Definition of text.
 *
 * Keys:
 * @start_table = config
 *     x (f_unsigned = 0)              The x coordinate of the top left corner.
 *     y (f_unsigned = 0)              The y coordinate of the top left corner.
 *     w (f_unsigned = 0)              The width of the rectangle.
 *     h (f_unsigned = 0)              The height of the rectangle.
 *     font_size (unsigned)            The size of the font to draw in.
 *     font_style (font_style = "")    The style of the text.
 *     colour (colour = "")            The colour of the text.
 *     text (f_tstring = "")           The text to draw (translatable).
 *     test_markup (f_bool = false)    Can the text have markup?
 *     maximum_width (f_int = -1)      The maximum width the text is allowed to be.
 *     maximum_height (f_int = -1)     The maximum height the text is allowed to be.
 *     debug = (string = "")           Debug message to show upon creation
 *                                     this message is not stored.
 * @end_table
 * NOTE alignment can be done with the formulas.
 *
 * Variables:
 * @start_table = formula
 *     text_width unsigned             The width of the rendered text.
 *     text_height unsigned            The height of the rendered text.
 * @end_table
 * Also the general variables are available, see [[#general_variables|Line]].
 *
 */

	VALIDATE(font_size_, _("Text has a font size of 0."));

	const std::string& debug = (cfg["debug"]);
	if(!debug.empty()) {
		DBG_G_P << "Text: found debug message '" << debug << "'.\n";
	}
}

void ttext::draw(//surface& canvas,
				 int xOffset, int yOffset,
	const game_logic::map_formula_callable& variables)
{

	assert(variables.has_key("text"));

	// We first need to determine the size of the text which need the rendered
	// text. So resolve and render the text first and then start to resolve
	// the other formulas.
	const t_string text = text_(variables);

	if(text.empty()) {
		DBG_G_D << "Text: no text to render, leave.\n";
		return;
	}

	static font::ttext text_renderer;

	text_renderer.set_text(text, text_markup_(variables)).
		set_font_size(font_size_).
		set_font_style(font_style_).
		set_foreground_colour(colour_).
		set_maximum_width(maximum_width_(variables)).
		set_maximum_height(maximum_height_(variables)).
		set_ellipse_mode(variables.has_key("text_wrap_mode")
			? static_cast<PangoEllipsizeMode>
				(variables.query_value("text_wrap_mode").as_int())
			: PANGO_ELLIPSIZE_END);

	surface surf = text_renderer.render();

	game_logic::map_formula_callable local_variables(variables);
	local_variables.add("text_width", variant(surf->w));
	local_variables.add("text_height", variant(surf->h));
/*
	std::cerr << "Text: drawing text '" << text
		<< " maximum width " << maximum_width_(variables)
		<< " maximum height " << maximum_height_(variables)
		<< " text width " << surf->w
		<< " text height " << surf->h;
*/
	//@todo formulas are now recalculated every draw cycle which is a
	// bit silly unless there has been a resize. So to optimize we should
	// use an extra flag or do the calculation in a separate routine.

	const unsigned x = x_(local_variables) + xOffset;
	const unsigned y = y_(local_variables) + yOffset;
	const unsigned w = w_(local_variables);
	const unsigned h = h_(local_variables);

//	DBG_G_D << "Text: drawing text '" << text
//		<< "' drawn from " << x << ',' << y
//		<< " width " << w << " height " << h
//		<< " canvas size " << canvas->w << ',' << canvas->h << ".\n";

//	VALIDATE(static_cast<int>(x) < canvas->w &&
//		 static_cast<int>(y) < canvas->h,
//		 _("Text doesn't start on canvas."));

	// A text might be to long and will be clipped.
	if(surf->w > static_cast<int>(w)) {
		WRN_G_D << "Text: text is too wide for the canvas and will be clipped.\n";
	}

	if(surf->h > static_cast<int>(h)) {
		WRN_G_D << "Text: text is too high for the canvas and will be clipped.\n";
	}

	//SDL_Rect dst = { x, y, canvas->w, canvas->h };
	//blit_surface(surf, 0, canvas, &dst);
	::blit_surface(x, y, surf);
}

} // namespace

/***** ***** ***** ***** ***** CANVAS ***** ***** ***** ***** *****/

tcanvas::tcanvas() :
	shapes_(),
	w_(0),
	h_(0),
//	canvas_(),
	variables_(),
	dirty_(true)
{
}

tcanvas::tcanvas(const config& cfg) :
	shapes_(),
	w_(0),
	h_(0),
//	canvas_(),
	variables_(),
	dirty_(true)
{
	parse_cfg(cfg);
}

void tcanvas::draw(const config& cfg)
{
	parse_cfg(cfg);
	// @TODO: .... check this...
	draw(0, 0, true);
}

void tcanvas::draw(int xOffset, int yOffset, const bool force)
{
	log_scope2(gui_draw, "Canvas: drawing.");
	if(!dirty_ && !force) {
		DBG_G_D << "Canvas: nothing to draw.\n";
		return;
	}

	if(dirty_) {
		get_screen_size_variables(variables_);
		variables_.add("width",variant(w_));
		variables_.add("height",variant(h_));
	}

	// create surface
//	DBG_G_D << "Canvas: create new empty canvas.\n";
//	canvas_.assign(create_neutral_surface(w_, h_));

	// draw items
	for(std::vector<tshape_ptr>::iterator itor =
			shapes_.begin(); itor != shapes_.end(); ++itor) {
		log_scope2(gui_draw, "Canvas: draw shape.");

//		(*itor)->draw(canvas_, variables_);
		(*itor)->draw(xOffset, yOffset, variables_);
	}

	dirty_ = false;
}

void tcanvas::parse_cfg(const config& cfg)
{
	log_scope2(gui_parse, "Canvas: parsing config.");
	shapes_.clear();

	for(config::all_children_iterator itor =
			cfg.ordered_begin(); itor != cfg.ordered_end(); ++itor) {

		std::string type((*itor).first);
		const config& data = *((*itor).second);

		DBG_G_P << "Canvas: found shape of the type " << type << ".\n";

		if(type == "line") {
			shapes_.push_back(new tline(data));
		} else if(type == "rectangle") {
			shapes_.push_back(new trectangle(data));
		} else if(type == "image") {
			shapes_.push_back(new timage(data));
		} else if(type == "text") {
			shapes_.push_back(new ttext(data));
		} else {
			ERR_G_P << "Canvas: found a shape of an invalid type " << type << ".\n";
			assert(false); // FIXME remove in production code.
		}
	}
}

/***** ***** ***** ***** ***** SHAPE ***** ***** ***** ***** *****/

void tcanvas::tshape::put_pixel(ptrdiff_t start, Uint32 colour, unsigned w, unsigned x, unsigned y)
{
	*reinterpret_cast<Uint32*>(start + (y * w * 4) + x * 4) = colour;
}

void tcanvas::tshape::draw_line(//surface& canvas, 
								int xOffset, int yOffset,
								Uint32 colour,
		const unsigned x1, unsigned y1, const unsigned x2, unsigned y2)
{
/*	
	colour = SDL_MapRGBA(canvas->format,
		((colour & 0xFF000000) >> 24),
		((colour & 0x00FF0000) >> 16),
		((colour & 0x0000FF00) >> 8),
		((colour & 0x000000FF)));

	ptrdiff_t start = reinterpret_cast<ptrdiff_t>(canvas->pixels);
	unsigned w = canvas->w;

	DBG_G_D << "Shape: draw line from "
		<< x1 << ',' << y1 << " to " << x2 << ',' << y2
		<< " canvas width " << w << " canvas height "
		<< canvas->h << ".\n";

	assert(static_cast<int>(x1) < canvas->w);
	assert(static_cast<int>(x2) < canvas->w);
	assert(static_cast<int>(y1) < canvas->h);
	assert(static_cast<int>(y2) < canvas->h);

	// use a special case for vertical lines
	if(x1 == x2) {
		if(y2 < y1) {
			std::swap(y1, y2);
		}

		for(unsigned y = y1; y <= y2; ++y) {
			put_pixel(start, colour, w, x1, y);
		}
		return;
	}

	// use a special case for horizontal lines
	if(y1 == y2) {
		for(unsigned x  = x1; x <= x2; ++x) {
			put_pixel(start, colour, w, x, y1);
		}
		return;
	}

	// draw based on Bresenham on wikipedia
	int dx = x2 - x1;
	int dy = y2 - y1;
	int slope = 1;
	if (dy < 0) {
		slope = -1;
		dy = -dy;
	}

	// Bresenham constants
	int incE = 2 * dy;
	int incNE = 2 * dy - 2 * dx;
	int d = 2 * dy - dx;
	int y = y1;

	// Blit
	for (unsigned x = x1; x <= x2; ++x) {
		put_pixel(start, colour, w, x, y);
		if (d <= 0) {
			d += incE;
		} else {
			d += incNE;
			y += slope;
		}
	}
 */
	
	::draw_line(colour, x1+xOffset, y1+yOffset, x2+xOffset, y2+yOffset);
}

} // namespace gui2
/*WIKI
 * @page = GUICanvasWML
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 * [[Category: Generated]]
 *
 */

/*WIKI
 * @page = GUIVariable
 * @order = ZZZZZZ_footer
 *
 * [[Category: WML Reference]]
 * [[Category: GUI WML Reference]]
 * [[Category: Generated]]
 *
 */
