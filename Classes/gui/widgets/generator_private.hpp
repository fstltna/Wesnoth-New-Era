/* $Id: generator_private.hpp 33670 2009-03-15 12:06:50Z mordante $ */
/*
   copyright (C) 2008 - 2009 by mark de wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#ifndef GUI_WIDGETS_GENERATOR_PRIVATE_HPP_INCLUDED
#define GUI_WIDGETS_GENERATOR_PRIVATE_HPP_INCLUDED

#include "gui/widgets/generator.hpp"

#include "asserts.hpp"
#include "foreach.hpp"
#include "gui/widgets/grid.hpp"
#include "gui/widgets/selectable.hpp"
#include "gui/widgets/toggle_button.hpp"
#include "gui/widgets/toggle_panel.hpp"

namespace gui2 {

/**
 * Contains the policies for the tgenerator class.
 */
namespace policy {

/***** ***** ***** ***** Minimum selection ***** ***** ***** *****/

/** Contains the policy for the minimum number of selected items. */
namespace minimum_selection {

/** Must select at least one item. */
struct tone
	: public virtual tgenerator_
{
	/**
	 * Called when an item is created.
	 *
	 * @param index               The index of the new item.
	 */
	void create_item(const unsigned index);

	/**
	 * Called when the users wants to deselect an item.
	 *
	 * If the item can be deselected this function should call
	 * do_deselect_item() to make the deslection happen. If not allowed no
	 * action needs to be taken.
	 *
	 * @param index               The index of the item to deselect.
	 *
	 * @returns                   Whether the item was deselected, some
	 *                            actions might happen automatically upon
	 *                            deselecting, so if this function returns
	 *                            false the caller should make sure the
	 *                            select state is restored.
	 */
	bool deselect_item(const unsigned index);

	/**
	 * Called just before an item is deleted.
	 *
	 * This function can if needed select another items to try to obey the
	 * policy.
	 *
	 * @param index              The index of the item to be deleted.
	 */
	void delete_item(const unsigned index);
};

/** No minimum selection. */
struct tnone
	: public virtual tgenerator_
{

	/** See minimum_selection::tone::create_item() */
	void create_item(const unsigned /*index*/) {}

	/** See minimum_selection::tone::deselect_item() */
	bool deselect_item(const unsigned index)
	{
		do_deselect_item(index);
		return true;
	}

	/** See ::minimum_selection::tone::delete_item() */
	void delete_item(const unsigned index)
	{
		if(is_selected(index)) {
			do_deselect_item(index);
		}
	}
};

} // namespace minimum_selection

/***** ***** ***** ***** Maximum selection ***** ***** ***** *****/

/** Contains the policy for the maximum number of selected items. */
namespace maximum_selection {

/** May select only one item. */
struct tone
	: public virtual tgenerator_
{
	/**
	 * Called when an item is selected.
	 *
	 * This function can deselect other items to obey the policy. This
	 * function should always call do_select_item() so the new item does get
	 * selected.
	 *
	 * @param index               The item to select.
	 */
	void select_item(const unsigned index)
	{
		if(get_selected_item_count() == 1) {
			// deselect current.
			do_deselect_item(get_selected_item());
			// select new.
			do_select_item(index);
		}
	}
};

/** No maximum amount of items to select. */
struct tinfinite
	: public virtual tgenerator_
{
	void select_item(const unsigned index)
	{
		do_select_item(index);
	}
};

} // namespace maximum_selection

/***** ***** ***** ***** Placement ***** ***** ***** *****/

/** Controls how new items are placed. */
namespace placement {

/**
 * Places the items in a horizontal row.
 *
 * @todo Implement.
 */
struct thorizontal_list
	: public virtual tgenerator_
{
	/**
	 * Called when an item is created.
	 *
	 * This function should place the new item.
	 *
	 * @param index                  The index of the new item.
	 */
	void create_item(const unsigned /*index*/) { ERROR_LOG(false); }

	/**
	 * Calculates the best size for the generator.
	 *
	 * @return                    The best size,
	 */
	tpoint calculate_best_size() const
		{ ERROR_LOG(false); }

	/**
	 * Sets the size of the generator.
	 *
	 * @param origin              The origin of the generator.
	 * @param size                The size of the generator.
	 */
	void set_size(const tpoint& /*origin*/, const tpoint& /*size*/)
		{ ERROR_LOG(false); }

	/**
	 * Sets the origin of the generator.
	 *
	 * @param origin              The origin of the generator.
	 */
	void set_origin(const tpoint& /*origin*/)
		{ ERROR_LOG(false); }

	/**
	 * Sets the visible area of the generator.
	 *
	 * @param area                The visible area.
	 */
	void set_visible_area(const SDL_Rect& /*area*/)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	twidget* find_widget(const tpoint&, const bool) { ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	const twidget* find_widget(const tpoint&, const bool) const
		{ ERROR_LOG(false); }

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tgenerator_. */
	void handle_key_up_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_down_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_left_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_right_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }
};

/** Places the items in a vertical column. */
struct tvertical_list
	: public virtual tgenerator_
{
	tvertical_list();

	/** See thorizontal_list::create_item(). */
	void create_item(const unsigned index);

	/** See thorizontal_list::calculate_best_size(). */
	tpoint calculate_best_size() const;

	/** See thorizontal_list::set_size(). */
	void set_size(const tpoint& origin, const tpoint& size);

	/** See thorizontal_list::set_origin(). */
	void set_origin(const tpoint& origin);

	/** See thorizontal_list::set_visible_area(). */
	void set_visible_area(const SDL_Rect& area);

	/** See thorizontal_list::find_widget(). */
	twidget* find_widget(const tpoint& coordinate, const bool must_be_active);

	/** See thorizontal_list::find_widget(). */
	const twidget* find_widget(const tpoint& coordinate,
			const bool must_be_active) const;

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tgenerator_. */
	void handle_key_up_arrow(SDLMod modifier, bool& handled);

	/** Inherited from tgenerator_. */
	void handle_key_down_arrow(SDLMod modifier, bool& handled);

	/** Inherited from tgenerator_. */
	void handle_key_left_arrow(SDLMod /*modifier*/, bool& /*handled*/)
		{ /* do nothing */ }

	/** Inherited from tgenerator_. */
	void handle_key_right_arrow(SDLMod /*modifier*/, bool& /*handled*/)
		{ /* do nothing */ }

	// FIXME we need a delete handler as well,
	// when deleting the last item we need to remove the placed flag.

	// FIXME we also need a clear function, called when
	// clear is called.
private:
	/**
	 * Has the grid already been placed?
	 *
	 * If the grid is placed it's no problem set the location of the new
	 * item,it hasn't been placed, there's no information about its location
	 * so do nothing.
	 */
	bool placed_;
};

/**
 * Places the items in a grid.
 *
 * The items will be placed in rows and columns. It has to be determined
 * whether the number of columns will be fixed or variable.
 *
 * @todo Implement.
 */
struct tmatrix
	: public virtual tgenerator_
{
	/** See thorizontal_list::create_item(). */
	void create_item(const unsigned /*index*/) { ERROR_LOG(false); }

	/** See thorizontal_list::calculate_best_size(). */
	tpoint calculate_best_size() const
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_size(). */
	void set_size(const tpoint& /*origin*/, const tpoint& /*size*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_origin(). */
	void set_origin(const tpoint& /*origin*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_visible_area(). */
	void set_visible_area(const SDL_Rect& /*area*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::find_widget(). */
	twidget* find_widget(const tpoint&, const bool) { ERROR_LOG(false); }

	/** See thorizontal_list::find_widget(). */
	const twidget* find_widget(const tpoint&, const bool) const
		{ ERROR_LOG(false); }

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tgenerator_. */
	void handle_key_up_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_down_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_left_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_right_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }
};

/**
 * Places the items independent of eachother.
 *
 * This is mainly meant for when only one item is shown at the same time.
 *
 * @todo Implement.
 */
struct tindependant
	: public virtual tgenerator_
{
	/** See thorizontal_list::create_item(). */
	void create_item(const unsigned /*index*/) { ERROR_LOG(false); }

	/** See thorizontal_list::calculate_best_size(). */
	tpoint calculate_best_size() const
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_size(). */
	void set_size(const tpoint& /*origin*/, const tpoint& /*size*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_origin(). */
	void set_origin(const tpoint& /*origin*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::set_visible_area(). */
	void set_visible_area(const SDL_Rect& /*area*/)
		{ ERROR_LOG(false); }

	/** See thorizontal_list::find_widget(). */
	twidget* find_widget(const tpoint&, const bool) { ERROR_LOG(false); }

	/** See thorizontal_list::find_widget(). */
	const twidget* find_widget(const tpoint&, const bool) const
		{ ERROR_LOG(false); }

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tgenerator_. */
	void handle_key_up_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_down_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_left_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }

	/** Inherited from tgenerator_. */
	void handle_key_right_arrow(SDLMod, bool&)
		{ ERROR_LOG(false); }
};

} // namespace placement

/***** ***** ***** ***** Select action ***** ***** ***** *****/

/**
 * Contains the policy for which action to take when an item is selected or
 * deselected.
 */
namespace select_action {

/** Select the item, this requires the grid to contain a tselectable_. */
struct tselect
	: public virtual tgenerator_
{
	void select(tgrid& grid, const bool select);
};

/** Show the item. */
struct tshow
	: public virtual tgenerator_
{
	/** @todo implement when set_visible works. */
	void select(tgrid& /*grid*/, const bool /*show*/)
	{
		//grid->set_visible(show);
	}
};

} // namespace select_action

} // namespace policy
/***** ***** ***** ***** tgenerator ***** ***** ***** *****/

/**
 * Basic template class to generate new items.
 *
 * The class is policy based so the behaviour can be selected.
 */
template
	    < class minimum_selection
		, class maximum_selection
		, class placement
		, class select_action
		>
class tgenerator
		: public minimum_selection
		, public maximum_selection
		, public placement
		, public select_action
{
public:

	tgenerator()
		: minimum_selection()
		, maximum_selection()
		, placement()
		, select_action()
		, selected_item_count_(0)
		, items_()
	{
	}

	~tgenerator()
	{
		clear();
	}

	/***** ***** ***** inherited ***** ****** *****/

	/** Inherited from tgenerator_. */
	void delete_item(const unsigned index)
	{
		assert(index < items_.size());

		minimum_selection::delete_item(index);

		delete items_[index];
		items_.erase(items_.begin() + index);
	}

	/** Inherited from tgenerator_. */
	void clear()
	{
		foreach(titem* item, items_) {
			delete item;
		}
		selected_item_count_ = 0;
	}

	/** Inherited from tgenerator_. */
	void select_item(const unsigned index,
			const bool select = true)
	{
		assert(index < items_.size());

		if(select && !is_selected(index)) {
			maximum_selection::select_item(index);
		} else if(is_selected(index)) {
			if(!minimum_selection::deselect_item(index)) {
				// Some items might have deseleted themselves so
				// make sure they do get selected again.
				select_action::select(get_item(index), true);
			}
		}
	}

	/** Inherited from tgenerator_. */
	bool is_selected(const unsigned index) const
	{
		assert(index < items_.size());
		return (*items_[index]).selected;
	}

	/** Inherited from tgenerator_. */
	unsigned get_item_count() const
	{
		return items_.size();
	}

	/** Inherited from tgenerator_. */
	unsigned get_selected_item_count() const
	{
		return selected_item_count_;
	}

	/** Inherited from tgenerator_. */
	int get_selected_item() const
	{

		if(selected_item_count_ == 0) {
			return -1;
		} else {
			for(size_t i = 0; i < items_.size(); ++i) {
				if((*items_[i]).selected) {
					return i;
				}
			}
			ERROR_LOG("No item selected.");
		}
	}

	/** Inherited from tgenerator_. */
	tgrid& get_item(const unsigned index)
	{
		assert(index < items_.size());
		return items_[index]->grid;
	}

	/** Inherited from tgenerator_. */
	void create_item(const int index,
			tbuilder_grid_const_ptr list_builder,
			const string_map& item_data,
			void (*callback)(twidget*))
 	{
		std::map<std::string, string_map> data;

		data.insert(std::make_pair("", item_data));
		create_item(index, list_builder, data, callback);
	}

	/** Inherited from tgenerator_. */
	void create_item(const int index,
			tbuilder_grid_const_ptr list_builder,
			const std::map<std::string /* widget id */,
			string_map>& item_data,
			void (*callback)(twidget*))
	{
		assert(list_builder);
		assert(index == -1 || static_cast<unsigned>(index) < items_.size());

		titem* item = new titem;
		list_builder->build(&item->grid);
		init(&item->grid, item_data, callback);

		const unsigned item_index = index == -1
				? items_.size()
				: index;

		items_.insert(items_.begin() + item_index, item);
		minimum_selection::create_item(item_index);
		placement::create_item(item_index);
	}

	/** Inherited from tgenerator_. */
	virtual void create_items(const int index,
			tbuilder_grid_const_ptr list_builder,
			const std::vector<std::map<std::string /*widget id*/,
			string_map> >& data,
			void (*callback)(twidget*))
	{
		impl_create_items(index, list_builder, data, callback);
	}

	/** Inherited from tgenerator_. */
	virtual void create_items(const int index,
			tbuilder_grid_const_ptr list_builder,
			const std::vector<string_map>& data,
			void (*callback)(twidget*))
	{
		impl_create_items(index, list_builder, data, callback);
	}

	/** Inherited from tgenerator_. */
	void layout_init()
	{
		foreach(titem* item, items_) {
			item->grid.layout_init();
		}
	}

	void layout_init2(const bool full_initialization)
	{
		foreach(titem* item, items_) {
			if(item->grid.get_visible() != twidget::INVISIBLE) {
				item->grid.layout_init2(full_initialization);
			}
		}
	}


	/** Inherited from tgenerator_. */
	tpoint calculate_best_size() const
	{
		return placement::calculate_best_size();
	}

	/** Inherited from tgenerator_. */
	void set_size(const tpoint& origin, const tpoint& size)
	{
 		// Inherited, so we get useful debug info.
		twidget::set_size(origin, size);

		placement::set_size(origin, size);
	}

	/** Inherited from tgenerator_. */
	void set_origin(const tpoint& origin)
	{
 		// Inherited.
		twidget::set_origin(origin);

		placement::set_origin(origin);
	}

	/** Inherited from tgenerator_. */
	void set_visible_area(const SDL_Rect& area)
	{
		placement::set_visible_area(area);
	}

	/** Inherited from tgenerator_. */
	void impl_draw_children(/*surface& frame_buffer*/)
	{
		foreach(titem* item, items_) {
			item->grid.draw_children(/*frame_buffer*/);
		}
	}

	/** Inherited from tgenerator_. */
	void child_populate_dirty_list(twindow& caller,
			const std::vector<twidget*>& call_stack)
	{
		foreach(titem* item, items_) {
			item->grid.child_populate_dirty_list(caller, call_stack);
		}
	}

	/** Inherited from tgenerator_. */
	twidget* find_widget(
			const tpoint& coordinate, const bool must_be_active)
	{
		return placement::find_widget(coordinate, must_be_active);
	}

	/** Inherited from tgenerator_. */
	const twidget* find_widget(
			const tpoint& coordinate, const bool must_be_active) const
	{
		return placement::find_widget(coordinate, must_be_active);
	}

	/***** ***** ***** ***** keyboard functions ***** ***** ***** *****/

	/** Inherited from tgenerator_. */
	void handle_key_up_arrow(SDLMod modifier, bool& handled)
	{
		placement::handle_key_up_arrow(modifier, handled);
	}

	/** Inherited from tgenerator_. */
	void handle_key_down_arrow(SDLMod modifier, bool& handled)
	{
		placement::handle_key_down_arrow(modifier, handled);
	}

	/** Inherited from tgenerator_. */
	void handle_key_left_arrow(SDLMod modifier, bool& handled)
	{
		placement::handle_key_left_arrow(modifier, handled);
	}

	/** Inherited from tgenerator_. */
	void handle_key_right_arrow(SDLMod modifier, bool& handled)
	{
		placement::handle_key_right_arrow(modifier, handled);
	}

protected:

	/** Inherited from tgenerator_. */
	const tgrid& get_item(const unsigned index) const
	{
		assert(index < items_.size());
		return items_[index]->grid;
	}

	/** Inherited from tgenerator_. */
	void do_select_item(const unsigned index) //fixme rename to impl
	{
		assert(index < items_.size());

		++selected_item_count_;
		set_item_selected(index, true);
	}

	/** Inherited from tgenerator_. */
	void do_deselect_item(const unsigned index)
	{
		assert(index < items_.size());

		--selected_item_count_;
		set_item_selected(index, false);
	}

private:

	/** Definition of an item. */
	struct titem {

		titem()
			: grid()
			, selected(false)
		{
		}

		/** The grid containing the widgets. */
		tgrid grid;

		/** Is the item selected or not. */
		bool selected;
	};

	/** The number of selected items. */
	unsigned selected_item_count_;

	/** The items in the generator. */
	std::vector<titem*> items_;

	/**
	 * Sets the selected state of an item.
	 *
	 * @param index               The item to modify.
	 * @param selected            Select or deselect.
	 */
	void set_item_selected(const unsigned index, const bool selected)
	{
		assert(index < items_.size());

		(*items_[index]).selected = selected;
		select_action::select((*items_[index]).grid, selected);
	}

	/**
	 * Helper function for create_items().
	 *
	 * @tparam T                  Type of the data, this should be one of the
	 *                            valid parameters for create_item().
	 *
	 * @param index               The item before which to add the new item,
	 *                            0 == begin, -1 == end.
	 * @param list_builder        A grid builder that's will build the
	 *                            contents of the new item.
	 * @param data                The data to initialize the parameters of
	 *                            the new item.
	 * @param callback            The callback function to call when an item
	 *                            in the grid is (de)selected.
	 */
	template<class T>
	void impl_create_items(const int index,
			tbuilder_grid_const_ptr list_builder,
			const std::vector<T>& data,
			void (*callback)(twidget*))
	{
		int i = index;
		foreach(const T& item_data, data) {
			create_item(i, list_builder, item_data, callback);
			if(i != -1) {
				++i;
			}
		}
	}

	/**
	 * Helper function to initialize a grid.
	 *
	 * @param grid                The grid to initialize.
	 * @param data                The data to initialize the parameters of
	 *                            the new item.
	 * @param callback            The callback function to call when an item
	 *                            in the grid is (de)selected.
	 */
	void init(tgrid* grid,
			const std::map<std::string /* widget id */, string_map>& data,
			void (*callback)(twidget*))
	{
		assert(grid);
		grid->set_parent(this);

		for(unsigned row = 0; row < grid->get_rows(); ++row) {
			for(unsigned col = 0; col < grid->get_cols(); ++col) {
				twidget* widget = grid->widget(row, col);
				assert(widget);

				tgrid* child_grid = dynamic_cast<tgrid*>(widget);
				ttoggle_button* btn = dynamic_cast<ttoggle_button*>(widget);
				ttoggle_panel* panel = dynamic_cast<ttoggle_panel*>(widget);

				if(btn) {
					btn->set_callback_state_change(callback);
					std::map<std::string, string_map>::const_iterator itor =
							data.find(btn->id());

					if(itor == data.end()) {
						itor = data.find("");
					}
					if(itor != data.end()) {
						btn->set_members(itor->second);
					}
				} else if(panel) {
					panel->set_callback_state_change(callback);
					panel->set_child_members(data);
				} else if(child_grid) {
					init(child_grid, data, callback);
				} else {
					ERROR_LOG("Widget type '"
							<< typeid(*widget).name() << "'.");
				}
			}
		}
	}
};

} // namespace gui2

#endif

