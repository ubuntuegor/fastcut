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

#include "encoder.h"
#include <libavcodec/avcodec.h>
#include "globals.h"
#include "reencoding.h"

void open_encoder(int stream_index) {
  int ret;
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  AVCodec* encoder = avcodec_find_encoder(stream_ctx->dec_ctx->codec_id);
  if (!encoder) {
    av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
    exit(1);
  }
  stream_ctx->enc_ctx = avcodec_alloc_context3(encoder);
  stream_ctx->enc_ctx->width = stream_ctx->dec_ctx->width;
  stream_ctx->enc_ctx->height = stream_ctx->dec_ctx->height;
  stream_ctx->enc_ctx->sample_aspect_ratio =
      stream_ctx->dec_ctx->sample_aspect_ratio;
  stream_ctx->enc_ctx->pix_fmt = stream_ctx->dec_ctx->pix_fmt;
  stream_ctx->enc_ctx->profile = stream_ctx->dec_ctx->profile;
  stream_ctx->enc_ctx->level = stream_ctx->dec_ctx->level;
  stream_ctx->enc_ctx->colorspace = stream_ctx->dec_ctx->colorspace;
  stream_ctx->enc_ctx->time_base =
      output_file->streams[stream_ctx->output_stream_index]->time_base;
  stream_ctx->enc_ctx->max_b_frames = 0;

  ret = avcodec_open2(stream_ctx->enc_ctx, encoder, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Cannot open video encoder for stream #%u\n",
           stream_index);
    exit(1);
  }

  stream_ctx->enc_pkt = av_packet_alloc();
}

void flush_encoder(int stream_index) {
  stream_ctxs[stream_index]->frame = NULL;
  encode_frame(stream_index);
}

void close_encoder(int stream_index) {
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  av_packet_free(&(stream_ctx->enc_pkt));
  avcodec_free_context(&(stream_ctx->enc_ctx));
}
