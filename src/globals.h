/* Copyright (C) 2021  Egor Kukoverov
 *
 * This file is part of fastcut.
 *
 * fastcut is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fastcut is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fastcut.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef FASTCUT_GLOBALS_H
#define FASTCUT_GLOBALS_H
#include "stream_ctx.h"

#define NB_OPTIONS_UNNAMED_PARAMS 4
typedef struct Options {
  const char* input_path;
  int64_t start_time_ms;
  int64_t end_time_ms;
  const char* output_path;
} Options;

extern Options o;
extern AVFormatContext* input_file;
extern AVFormatContext* output_file;
extern StreamContext** stream_ctxs;
#endif
