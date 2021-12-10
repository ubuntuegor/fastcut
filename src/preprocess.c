#include "preprocess.h"
#include "globals.h"

typedef struct PreprocessContext {
  int ended;
  int is_end_pts_in_stream;
  int has_set_first_dts_shift;
  int64_t max_pts;
  int64_t last_dts_shift;
  int64_t last_dts_shift_k;
} PreprocessContext;

static PreprocessContext** allocate_preprocess_ctx(int nb_streams) {
  PreprocessContext** preprocess_ctxs =
      (PreprocessContext**)av_mallocz(nb_streams * sizeof(PreprocessContext*));
  for (int i = 0; i < nb_streams; ++i) {
    preprocess_ctxs[i] =
        (PreprocessContext*)av_mallocz(sizeof(PreprocessContext));
    preprocess_ctxs[i]->ended = 0;
    preprocess_ctxs[i]->is_end_pts_in_stream = 0;
    preprocess_ctxs[i]->has_set_first_dts_shift = 0;
    preprocess_ctxs[i]->last_dts_shift = 0;
    preprocess_ctxs[i]->last_dts_shift_k = 0;
    preprocess_ctxs[i]->max_pts = AV_NOPTS_VALUE;
  }
  return preprocess_ctxs;
}

static void free_preprocess_ctx(PreprocessContext** preprocess_ctxs,
                                int nb_streams) {
  for (int i = 0; i < nb_streams; ++i) {
    av_free(preprocess_ctxs[i]);
  }
  av_free(preprocess_ctxs);
}

static int have_all_streams_ended(PreprocessContext** preprocess_ctxs,
                                  int nb_streams) {
  int res = 1;
  for (int i = 0; i < nb_streams; ++i) {
    StreamContext* stream_ctx = stream_ctxs[i];
    PreprocessContext* preprocess_ctx = preprocess_ctxs[i];
    if (!(stream_ctx->ignore) && stream_ctx->type == AVMEDIA_TYPE_VIDEO &&
        !(preprocess_ctx->ended)) {
      res = 0;
      break;
    }
  }
  return res;
}

void do_preprocess() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_WARNING, "Failed to seek the file");
  }

  av_log(NULL, AV_LOG_INFO, "Analyzing file...\n");

  AVPacket* pkt = av_packet_alloc();

  int nb_streams = input_file->nb_streams;
  // if the cut point is right before a keyframe, set last gop start to it
  PreprocessContext** preprocess_ctxs = allocate_preprocess_ctx(nb_streams);

  while (av_read_frame(input_file, pkt) >= 0) {
    StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
    PreprocessContext* preprocess_ctx = preprocess_ctxs[pkt->stream_index];
    if (stream_ctx->type != AVMEDIA_TYPE_VIDEO || stream_ctx->ignore ||
        preprocess_ctx->ended)
      continue;

    if (pkt->pts >= stream_ctx->end_pts) {
      preprocess_ctx->is_end_pts_in_stream = 1;
      preprocess_ctx->ended = 1;
      stream_ctx->last_gop_dts_shift = preprocess_ctx->last_dts_shift_k;
    }

    if (preprocess_ctx->max_pts == AV_NOPTS_VALUE ||
        pkt->pts > preprocess_ctx->max_pts) {
      preprocess_ctx->max_pts = pkt->pts;
    }

    if (pkt->flags & AV_PKT_FLAG_KEY &&
        (pkt->pts < stream_ctx->end_pts || preprocess_ctx->ended)) {
      stream_ctx->pts_last_gop_start = pkt->pts;
    }

    if (!(preprocess_ctx->has_set_first_dts_shift) &&
        pkt->pts >= stream_ctx->start_pts && (pkt->flags & AV_PKT_FLAG_KEY)) {
      stream_ctx->first_gop_dts_shift = preprocess_ctx->last_dts_shift;
      preprocess_ctx->has_set_first_dts_shift = 1;
    }

    preprocess_ctx->last_dts_shift = preprocess_ctx->max_pts - pkt->dts;
    if (pkt->flags & AV_PKT_FLAG_KEY) {
      preprocess_ctx->last_dts_shift_k = pkt->pts - pkt->dts;
    }

    av_packet_unref(pkt);

    if (have_all_streams_ended(preprocess_ctxs, nb_streams))
      break;
  }

  av_packet_free(&pkt);

  for (int i = 0; i < nb_streams; ++i) {
    if (!(preprocess_ctxs[i]->is_end_pts_in_stream)) {
      stream_ctxs[i]->pts_last_gop_start = AV_NOPTS_VALUE;
    }
  }
}
