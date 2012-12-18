#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "mstring.h"

string_t dump_log();

int  print_log(string_t txt);

int  print_warning(string_t txt);

int  print_error(string_t txt);

int  print_debug(string_t txt);

#endif /* __DISPLAY_H__ */
