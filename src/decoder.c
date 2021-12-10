/* Copyright (C) 2021  Egor Kukoverov
 *
 * This file is part of fastcut.
 *
 * fastcut is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * fastcut is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with fastcut.  If not, see <https://www.gnu.org/licenses/>.
 */

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
