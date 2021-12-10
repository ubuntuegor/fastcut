#include "stream_ctx.h"
#include "globals.h"

void allocate_stream_contexts(int nb_streams) {
  stream_ctxs =
      (StreamContext**)av_mallocz(nb_streams * sizeof(StreamContext*));
  for (int i = 0; i < nb_streams; ++i) {
    stream_ctxs[i] = (StreamContext*)av_mallocz(sizeof(StreamContext));
    stream_ctxs[i]->state = SEEKING_AND_FIRST_GOP;
    stream_ctxs[i]->ignore = 0;
    stream_ctxs[i]->start_pts = av_rescale_q(o.start_time_ms, AV_TIME_BASE_Q,
                                             input_file->streams[i]->time_base);
    stream_ctxs[i]->end_pts = av_rescale_q(o.end_time_ms, AV_TIME_BASE_Q,
                                           input_file->streams[i]->time_base);
    stream_ctxs[i]->pts_last_gop_start = AV_NOPTS_VALUE;
    stream_ctxs[i]->first_occured_pts = AV_NOPTS_VALUE;
    stream_ctxs[i]->first_gop_dts_shift = 0;
    stream_ctxs[i]->last_gop_dts_shift = 0;
    stream_ctxs[i]->type = input_file->streams[i]->codecpar->codec_type;
    stream_ctxs[i]->dec_ctx = NULL;
    stream_ctxs[i]->enc_ctx = NULL;
    stream_ctxs[i]->bsf_ctx = NULL;
  }
}

void free_stream_contexts(int nb_streams) {
  for (int i = 0; i < nb_streams; ++i) {
    av_free(stream_ctxs[i]);
  }
  av_freep(&stream_ctxs);
}
