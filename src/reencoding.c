#include "reencoding.h"
#include "encoder.h"
#include "globals.h"

void encode_frame(int stream_index) {
  int ret;
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  ret = avcodec_send_frame(stream_ctx->enc_ctx, stream_ctx->frame);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Encoding stream %u failed\n", stream_index);
    exit(1);
  }
  while (ret >= 0) {
    ret = avcodec_receive_packet(stream_ctx->enc_ctx, stream_ctx->enc_pkt);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      ret = 0;
      break;
    }

    AVRational in_time_base = input_file->streams[stream_index]->time_base;
    AVRational out_time_base =
        output_file->streams[stream_ctx->output_stream_index]->time_base;
    stream_ctx->enc_pkt->stream_index = stream_ctx->output_stream_index;
    if (stream_ctx->enc_pkt->pts != AV_NOPTS_VALUE)
      stream_ctx->enc_pkt->pts -= stream_ctx->first_occured_pts;
    if (stream_ctx->enc_pkt->dts != AV_NOPTS_VALUE)
      stream_ctx->enc_pkt->dts -= stream_ctx->first_occured_pts;
    if (stream_ctx->state == SEEKING_AND_FIRST_GOP) {
      stream_ctx->enc_pkt->dts =
          stream_ctx->enc_pkt->pts - stream_ctx->first_gop_dts_shift;
    }
    if (stream_ctx->state == LAST_GOP) {
      stream_ctx->enc_pkt->dts =
          stream_ctx->enc_pkt->pts - stream_ctx->last_gop_dts_shift;
    }
    av_packet_rescale_ts(stream_ctx->enc_pkt, in_time_base, out_time_base);
    ret = av_interleaved_write_frame(output_file, stream_ctx->enc_pkt);
    av_packet_unref(stream_ctx->enc_pkt);
  }
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Encoding stream %u failed\n", stream_index);
    exit(1);
  }
}

void reencode_packet(int stream_index, AVPacket* pkt) {
  int ret;
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  ret = avcodec_send_packet(stream_ctx->dec_ctx, pkt);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Decoding stream %u failed\n", stream_index);
    exit(1);
  }
  while (ret >= 0) {
    ret = avcodec_receive_frame(stream_ctx->dec_ctx, stream_ctx->frame);
    if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
      ret = 0;
      break;
    }
    if (stream_ctx->frame->pts >= stream_ctx->start_pts &&
        stream_ctx->frame->pts < stream_ctx->end_pts) {
      if (stream_ctx->first_occured_pts == AV_NOPTS_VALUE) {
        stream_ctx->first_occured_pts = stream_ctx->frame->pts;
      }
      if (stream_ctx->enc_ctx == NULL) {
        av_log(NULL, AV_LOG_INFO, "Start reencoding track %u\n", stream_index);
        open_encoder(stream_index);
      }
      encode_frame(stream_index);
    }
  }
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Decoding stream %u failed\n", stream_index);
    exit(1);
  }
}
