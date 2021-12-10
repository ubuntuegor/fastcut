#include "decoder.h"
#include <libavcodec/avcodec.h>
#include "globals.h"
#include "reencoding.h"

void open_decoder(int stream_index) {
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  AVCodec* codec = avcodec_find_decoder(
      input_file->streams[stream_index]->codecpar->codec_id);

  if (!codec) {
    av_log(NULL, AV_LOG_FATAL, "Failed to find decoder for stream %u\n",
           stream_index);
    exit(1);
  }
  stream_ctx->dec_ctx = avcodec_alloc_context3(codec);
  if (!stream_ctx->dec_ctx) {
    av_log(NULL, AV_LOG_FATAL,
           "Failed to allocate the decoder context for stream %u\n",
           stream_index);
    exit(1);
  }
  if (avcodec_parameters_to_context(
          stream_ctx->dec_ctx, input_file->streams[stream_index]->codecpar) <
      0) {
    av_log(NULL, AV_LOG_FATAL,
           "Failed to copy decoder parameters to input decoder context "
           "for stream #%u\n",
           stream_index);
    exit(1);
  }
  if (avcodec_open2(stream_ctx->dec_ctx, codec, NULL) < 0) {
    av_log(NULL, AV_LOG_FATAL, "Failed to open decoder for stream %u\n",
           stream_index);
    exit(1);
  }

  stream_ctx->frame = av_frame_alloc();
}

void flush_decoder(int stream_index) {
  reencode_packet(stream_index, NULL);
}

void close_decoder(int stream_index) {
  av_frame_unref(stream_ctxs[stream_index]->frame);
  av_frame_free(&(stream_ctxs[stream_index]->frame));
  avcodec_free_context(&(stream_ctxs[stream_index]->dec_ctx));
}
