#include "preprocess.h"
#include "globals.h"

void find_start_of_last_gop() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_WARNING, "Failed to seek the file");
  }

  av_log(NULL, AV_LOG_INFO, "Analyzing file...\n");

  AVPacket* pkt = av_packet_alloc();
  int nb_streams = input_file->nb_streams;
  int last_frame_in_cut = 1;
  int* is_end_pts_in_stream = (int*)av_mallocz(nb_streams * sizeof(int));
  while (av_read_frame(input_file, pkt) >= 0) {
    StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
    if (stream_ctx->type != AVMEDIA_TYPE_VIDEO || stream_ctx->ignore)
      continue;
    if (pkt->pts >= stream_ctx->end_pts) {
      is_end_pts_in_stream[pkt->stream_index] = 1;
    }
    if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts != AV_NOPTS_VALUE) {
      if (pkt->pts < stream_ctx->end_pts || last_frame_in_cut) {
        stream_ctx->pts_last_gop_start = pkt->pts;
      }
    }
    if (pkt->pts < stream_ctx->end_pts) {
      last_frame_in_cut = 1;
    } else {
      last_frame_in_cut = 0;
    }
    av_packet_unref(pkt);
  }

  av_packet_free(&pkt);

  for (int i = 0; i < nb_streams; ++i) {
    if (!is_end_pts_in_stream[i]) {
      stream_ctxs[i]->pts_last_gop_start = AV_NOPTS_VALUE;
    }
  }
  av_freep(&is_end_pts_in_stream);
}
