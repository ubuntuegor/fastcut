#ifndef FASTCUT_TYPES_H
#define FASTCUT_TYPES_H
#include <libavformat/avformat.h>

#define NB_OPTIONS_UNNAMED_PARAMS 4
typedef struct Options {
  const char* input_path;
  int64_t start_time_ms;
  int64_t end_time_ms;
  const char* output_path;
} Options;

typedef enum StreamState {
  SEEKING_AND_FIRST_GOP,
  COPYING,
  LAST_GOP,
  ENDED
} StreamState;

typedef struct StreamContext {
  int ignore;
  int output_stream_index;
  enum AVMediaType type;
  AVCodecContext* dec_ctx;
  AVCodecContext* enc_ctx;
  AVFrame* frame;
  AVPacket* enc_pkt;
  int64_t start_pts;
  int64_t first_occured_pts;
  int64_t end_pts;
  int64_t pts_last_gop_start;
  StreamState state;
} StreamContext;
#endif
