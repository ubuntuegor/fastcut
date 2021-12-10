#ifndef FASTCUT_UTILS_H
#define FASTCUT_UTILS_H
#include "globals.h"

void print_usage();
void parse_options_from_argv(int argc, const char** argv);
void ignore_unsupported_streams();
void ignore_streams_from_argv(int argc, const char** argv);
#endif
