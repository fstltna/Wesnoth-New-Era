/* $Id: gettext.hpp 31858 2009-01-01 10:27:41Z mordante $ */
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

#ifndef GETTEXT_HPP_INCLUDED
#define GETTEXT_HPP_INCLUDED

/**
 * How to use gettext for wesnoth source files:
 * 1) include this header file in the .cpp file
 * 2) add this include to set the correct textdomain, in this example
 *    wesnoth-lib (not required for the domain 'wesnoth', required for all
 *    other textdomains):
 *    #define GETTEXT_DOMAIN "wesnoth-lib"
 * 3) make sure, that the source file is listed in the respective POTFILES.in
 *    for the textdomain, in the case of wesnoth-lib it is this file:
 *    po/wesnoth-lib/POTFILES.in
 * This should be all that is required to have your strings that are marked
 * translateable in the po files and translated ingame. So you at least have
 * to mark the strings translateable, too. ;)
 */

// gettext-related declarations

// KP: commented out
//#include <libintl.h>


const char* egettext(const char*);
const char* sgettext(const char*);
const char* dsgettext(const char * domainname, const char *msgid);
const char* sngettext(const char *singular, const char *plural, int n);
const char* dsngettext(const char * domainname, const char *singular, const char *plural, int n);

#ifdef GETTEXT_DOMAIN
# define _(String) dsgettext(GETTEXT_DOMAIN,String)
# define _n(String1,String2,Int) dsngettext(String1,String2,Int)
# ifdef gettext
#  undef gettext
# endif
# define gettext(String) dsgettext(GETTEXT_DOMAIN,String)
# define sgettext(String) dsgettext(GETTEXT_DOMAIN,String)
# define sngettext(String1,String2,Int) dsngettext(GETTEXT_DOMAIN,String1,String2,Int)
#else
# define _(String) sgettext(String)
# define _n(String1,String2,Int) sngettext(String1,String2,Int)
# define gettext(a)		(a)
#endif

#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/*
#define gettext(std::string)			std::string
#define gettext_noop(std::string)	std::string
#define N_(std::string)				std::string
#define _(std::string)				std::string
#define _n(std::string1, std::string2, Int)	std::string1
#define dsgettext(Domainname, msgid)	msgid
#define sgettext(std::string)		std::string
#define egettext(std::string)		std::string
#define sngettext(Singular, plural, Int)	Singular
*/




 
 
#endif
