#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "mstring.h"

string_t dump_log();

int  print_log(char *txt, ...);

int  print_warning(char *txt, ...);

int  print_error(char *txt, ...);

int  print_debug(char *txt, ...);

#endif /* __DISPLAY_H__ */
