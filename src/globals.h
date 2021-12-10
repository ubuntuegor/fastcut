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
