#ifndef FASTCUT_STREAM_CTX_H
#define FASTCUT_STREAM_CTX_H
#include <libavformat/avformat.h>

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
  AVBSFContext* bsf_ctx;
  AVFrame* frame;
  AVPacket* enc_pkt;
  int64_t start_pts;
  int64_t first_occured_pts;
  int64_t end_pts;
  int64_t pts_last_gop_start;
  int64_t first_gop_dts_shift;
  int64_t last_gop_dts_shift;
  StreamState state;
} StreamContext;

void allocate_stream_contexts(int nb_streams);
void free_stream_contexts(int nb_streams);
#endif