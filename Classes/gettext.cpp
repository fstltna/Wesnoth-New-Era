/* $Id: gettext.cpp 31858 2009-01-01 10:27:41Z mordante $ */
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

#include "global.hpp"

#include "gettext.hpp"

#include <cstring>

char const *egettext(char const *msgid)
{
	return msgid[0] == '\0' ? msgid : gettext(msgid);
}

const char* sgettext (const char *msgid)
{
	const char *msgval = gettext (msgid);
	if (msgval == msgid) {
		msgval = std::strrchr (msgid, '^');
		if (msgval == NULL)
			msgval = msgid;
		else
			msgval++;
	}
	return msgval;
}

const char* dsgettext (const char * domainname, const char *msgid)
{
//	bind_textdomain_codeset(domainname, "UTF-8");
	const char *msgval = gettext(msgid); //dgettext (domainname, msgid);
	if (msgval == msgid) {
		msgval = std::strrchr (msgid, '^');
		if (msgval == NULL)
			msgval = msgid;
		else
			msgval++;
	}
	return msgval;
}

const char* sngettext (const char *singular, const char *plural, int n)
{
	const char *msgval = gettext(singular); //ngettext (singular, plural, n);
	if (msgval == singular) {
		msgval = std::strrchr (singular, '^');
		if (msgval == NULL)
			msgval = singular;
		else
			msgval++;
	}
	return msgval;
}

const char* dsngettext (const char * domainname, const char *singular, const char *plural, int n)
{
	//bind_textdomain_codeset(domainname, "UTF-8");
	const char *msgval = gettext(singular); //dngettext (domainname, singular, plural, n);
	if (msgval == singular) {
		msgval = std::strrchr (singular, '^');
		if (msgval == NULL)
			msgval = singular;
		else
			msgval++;
	}
	return msgval;
}
