#ifndef NEWSBEUTER_CONFIG__H
#define NEWSBEUTER_CONFIG__H

#define PACKAGE				"newsbeuter"
#define PROGRAM_NAME			PACKAGE
#define PROGRAM_VERSION			"2.2"
#define PROGRAM_URL			"http://www.newsbeuter.org/"

#define NEWSBEUTER_PATH_SEP			"/"
#define NEWSBEUTER_CONFIG_SUBDIR	".newsbeuter"

#include <libintl.h>
#include <locale.h>

#ifdef _
#undef _
#endif

#define _(string) gettext(string)

/* #define NO_DEBUG */ // only enable this #define if you want to disable all debug logging.

#endif
