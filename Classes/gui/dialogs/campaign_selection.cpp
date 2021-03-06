/* $Id: campaign_selection.cpp 33687 2009-03-15 16:50:42Z mordante $ */
/*
   Copyright (C) 2009 by Mark de Wever <koraq@xs4all.nl>
   part of the battle for wesnoth project http://www.wesnoth.org/

   this program is free software; you can redistribute it and/or modify
   it under the terms of the gnu general public license version 2
   or at your option any later version.
   this program is distributed in the hope that it will be useful,
   but without any warranty.

   see the copying file for more details.
*/

#define GETTEXT_DOMAIN "wesnoth-lib"

#include "gui/dialogs/campaign_selection.hpp"

#include "gui/dialogs/helper.hpp"
#include "gui/widgets/gui2image.hpp"
#include "gui/widgets/listbox.hpp"
#include "gui/widgets/scroll_label.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"

namespace gui2 {

/*WIKI
 * @page = GUIWindowDefinitionWML
 * @order = 2_campaign_selection
 *
 * == Campaign selection ==
 *
 * This shows the dialog choose which campaign the user wants to play. This
 * dialog is under construction and only used with --new-widgets.
 */

void tcampaign_selection::campaign_selected(twindow& window)
{
	tlistbox* list = dynamic_cast<tlistbox*>(
			window.find_widget("campaign_list", false));
	VALIDATE(list, missing_widget("campaign_list"));

	// Get the selected row
	config::child_list::const_iterator itor = begin_
			+ list->get_selected_row();

	tscroll_label* scroll_label = dynamic_cast<tscroll_label*>(
			window.find_widget("description", false));
	if(scroll_label) {
		scroll_label->set_label((**itor)["description"]);
	}

	timage* image = dynamic_cast<timage*>(
			window.find_widget("image", false));
	if(image) {
		image->set_label((**itor)["image"]);
	}

	window.invalidate_layout();
}

twindow* tcampaign_selection::build_window(CVideo& video)
{
	return build(video, get_id(CAMPAIGN_SELECTION));
}

void tcampaign_selection::pre_show(CVideo& /*video*/, twindow& window)
{
	tlistbox* list = dynamic_cast<tlistbox*>(
			window.find_widget("campaign_list", false));
	VALIDATE(list, missing_widget("campaign_list"));

	list->set_callback_value_change(dialog_callback
			<tcampaign_selection, &tcampaign_selection::campaign_selected>);

	for(config::child_list::const_iterator itor = begin_;
			itor != end_; ++itor) {

		string_map item;
		item.insert(std::make_pair("icon", (**itor)["icon"]));
		item.insert(std::make_pair("label", (**itor)["name"]));
		list->add_row(item);
	}

	campaign_selected(window);
}

void tcampaign_selection::post_show(twindow& window)
{
		tlistbox* list = dynamic_cast<tlistbox*>(
				window.find_widget("campaign_list", false));
		assert(list);

		choice_ = list->get_selected_row();
}

} // namespace gui2
