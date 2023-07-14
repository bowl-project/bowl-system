#ifndef SYSTEM_H
#define SYSTEM_H

#include <bowl/api.h>
#include <bowl/unicode.h>

#if defined(OS_UNIX)
    #include <unistd.h>
    #include <errno.h>
#elif defined(OS_WINDOWS)
    #include <direct.h>
    #include <errno.h>
#endif

BowlValue system_exit(BowlStack stack);

BowlValue system_change_directory(BowlStack stack);

BowlValue system_directory(BowlStack stack);

#endif
