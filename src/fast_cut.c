#include "fast_cut.h"
#include "bsf.h"
#include "decoder.h"
#include "encoder.h"
#include "globals.h"
#include "reencoding.h"

static void copy_packet(AVPacket* pkt) {
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  AVRational in_time_base = input_file->streams[pkt->stream_index]->time_base;
  AVRational out_time_base =
      output_file->streams[stream_ctx->output_stream_index]->time_base;
  pkt->stream_index = stream_ctx->output_stream_index;
  if (pkt->pts != AV_NOPTS_VALUE)
    pkt->pts -= stream_ctx->first_occured_pts;
  if (pkt->dts != AV_NOPTS_VALUE)
    pkt->dts -= stream_ctx->first_occured_pts;
  av_packet_rescale_ts(pkt, in_time_base, out_time_base);
  if (av_interleaved_write_frame(output_file, pkt) < 0) {
    av_log(NULL, AV_LOG_FATAL, "Failed to copy packet\n");
    exit(1);
  }
}

static void process_video_packet(AVPacket* pkt) {
  int ret;
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  switch (stream_ctx->state) {
    case SEEKING_AND_FIRST_GOP:
      if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts >= stream_ctx->start_pts) {
        // keyframe in cut segment - should start copying
        if (stream_ctx->dec_ctx != NULL) {
          flush_decoder(pkt->stream_index);
          close_decoder(pkt->stream_index);
        }
        if (stream_ctx->enc_ctx != NULL) {
          flush_encoder(pkt->stream_index);
          close_encoder(pkt->stream_index);
        }
        av_log(NULL, AV_LOG_INFO, "Copying track %u\n", pkt->stream_index);
        stream_ctx->state = COPYING;
        if (stream_ctx->first_occured_pts == AV_NOPTS_VALUE)
          stream_ctx->first_occured_pts = pkt->pts;
        process_video_packet(pkt);
        break;
      }
      if (stream_ctx->pts_last_gop_start != AV_NOPTS_VALUE &&
          pkt->pts >= stream_ctx->pts_last_gop_start) {
        // met last GOP
        if (stream_ctx->dec_ctx != NULL) {
          flush_decoder(pkt->stream_index);
          close_decoder(pkt->stream_index);
        }
        stream_ctx->state = LAST_GOP;
        process_video_packet(pkt);
        break;
      }
      if (stream_ctx->dec_ctx == NULL) {
        open_decoder(pkt->stream_index);
      }

      reencode_packet(pkt->stream_index, pkt);

      break;
    case COPYING:
      if (pkt->pts >= stream_ctx->end_pts) {
        stream_ctx->state = ENDED;
        break;
      }
      if (stream_ctx->pts_last_gop_start != AV_NOPTS_VALUE &&
          pkt->pts >= stream_ctx->pts_last_gop_start) {
        stream_ctx->state = LAST_GOP;
        process_video_packet(pkt);
        break;
      }

      copy_packet(pkt);
      break;
    case LAST_GOP:
      if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts >= stream_ctx->end_pts) {
        flush_decoder(pkt->stream_index);
        close_decoder(pkt->stream_index);
        flush_encoder(pkt->stream_index);
        close_encoder(pkt->stream_index);
        stream_ctx->state = ENDED;
      }
      if (stream_ctx->dec_ctx == NULL) {
        open_decoder(pkt->stream_index);
      }

      reencode_packet(pkt->stream_index, pkt);
      break;
  }
}

static void process_copy_packet(AVPacket* pkt) {
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  switch (stream_ctx->state) {
    case SEEKING_AND_FIRST_GOP:
      if (pkt->pts >= stream_ctx->start_pts) {
        stream_ctx->state = COPYING;
        if (stream_ctx->first_occured_pts == AV_NOPTS_VALUE)
          stream_ctx->first_occured_pts = pkt->pts;
        process_copy_packet(pkt);
      }
      break;
    case COPYING:
      if (pkt->pts >= stream_ctx->end_pts) {
        stream_ctx->state = ENDED;
        break;
      }

      copy_packet(pkt);
      break;
  }
}

static int have_all_streams_ended() {
  int res = 1;
  for (int i = 0; i < input_file->nb_streams; ++i) {
    StreamContext* stream_ctx = stream_ctxs[i];
    if (!(stream_ctx->ignore) && stream_ctx->state != ENDED) {
      res = 0;
      break;
    }
  }
  return res;
}

void process_packet(AVPacket* pkt) {
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  switch (stream_ctx->type) {
    case AVMEDIA_TYPE_VIDEO:
      process_video_packet(pkt);
      break;
    default:
      process_copy_packet(pkt);
      break;
  }
}

void do_fast_cut() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_FATAL, "Failed to seek the file");
    exit(1);
  }

  AVPacket* pkt = av_packet_alloc();

  int ret = 0;
  while (ret >= 0) {
    int ret = av_read_frame(input_file, pkt);
    if (ret == AVERROR_EOF) {
      ret = 0;
      break;
    }

    int stream_index = pkt->stream_index;
    StreamContext* stream_ctx = stream_ctxs[stream_index];
    if (stream_ctx->ignore)
      continue;

    if (stream_ctx->bsf_ctx) {
      send_frame_to_bsf(pkt, stream_index);
      while (get_bsf_frames(pkt, stream_index) >= 0) {
        process_packet(pkt);
      }
    } else {
      process_packet(pkt);
    }

    av_packet_unref(pkt);

    if (have_all_streams_ended())
      break;
  }

  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Failed to read input file\n%s\n",
           av_err2str(ret));
    exit(1);
  }

  if (!have_all_streams_ended()) {
    int nb_streams = input_file->nb_streams;
    for (int i = 0; i < nb_streams; ++i) {
      StreamContext* stream_ctx = stream_ctxs[i];
      if (stream_ctx->bsf_ctx != NULL) {
        send_frame_to_bsf(NULL, i);
        while (get_bsf_frames(pkt, i) >= 0) {
          process_packet(pkt);
        }
      }
    }
  }

  av_packet_free(&pkt);
}
