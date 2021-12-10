#ifndef FASTCUT_GLOBALS_H
#define FASTCUT_GLOBALS_H
#include "types.h"
extern Options o;
extern AVFormatContext* input_file;
extern AVFormatContext* output_file;
extern StreamContext** stream_ctxs;
#endif
