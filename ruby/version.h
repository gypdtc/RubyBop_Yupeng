#define RUBY_VERSION "2.2.3"
#define RUBY_RELEASE_DATE "2015-05-21"
#define RUBY_PATCHLEVEL 115
#define RUBYBOP_VERSION "Rubybop version v0.5"
#define RUBYBOP_DATE "2016-04"

#define RUBY_RELEASE_YEAR 2015
#define RUBY_RELEASE_MONTH 5
#define RUBY_RELEASE_DAY 21

#include "ruby/version.h"
#include "bop_info.h"

#if !defined RUBY_LIB_VERSION && defined RUBY_LIB_VERSION_STYLE
# if RUBY_LIB_VERSION_STYLE == 3
#   define RUBY_LIB_VERSION STRINGIZE(RUBY_API_VERSION_MAJOR)"."STRINGIZE(RUBY_API_VERSION_MINOR)"."STRINGIZE(RUBY_API_VERSION_TEENY)
# elif RUBY_LIB_VERSION_STYLE == 2
#   define RUBY_LIB_VERSION STRINGIZE(RUBY_API_VERSION_MAJOR)"."STRINGIZE(RUBY_API_VERSION_MINOR)
# endif
#endif

#if RUBY_PATCHLEVEL == -1
#define RUBY_PATCHLEVEL_STR "dev"
#else
#define RUBY_PATCHLEVEL_STR "p"STRINGIZE(RUBY_PATCHLEVEL)
#endif

#ifndef RUBY_REVISION
# include "revision.h"
#endif
#ifndef RUBY_REVISION
# define RUBY_REVISION 0
#endif

#if RUBY_REVISION
# ifdef RUBY_BRANCH_NAME
#  define RUBY_REVISION_STR " "RUBY_BRANCH_NAME" "STRINGIZE(RUBY_REVISION)
# else
#  define RUBY_REVISION_STR " revision "STRINGIZE(RUBY_REVISION)
# endif
#else
# define RUBY_REVISION_STR ""
#endif
#define BOP_DESCRIPTION \
  "BOP library: " BOP_VERSION
# define RUBY_DESCRIPTION	    \
    "Ruby "RUBY_VERSION		    \
    RUBY_PATCHLEVEL_STR		    \
    " ("RUBY_RELEASE_DATE	    \
    RUBY_REVISION_STR") "	    \
    "["RUBY_PLATFORM"]"
#define RUBYBOP_DESCRIPTION \
    "Rubybop version " RUBYBOP_VERSION
# define RUBY_COPYRIGHT		    \
    "ruby - Copyright (C) "	    \
    STRINGIZE(RUBY_BIRTH_YEAR)"-"   \
    STRINGIZE(RUBY_RELEASE_YEAR)" " \
    RUBY_AUTHOR