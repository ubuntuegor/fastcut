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

#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include "bsf.h"
#include "fast_cut.h"
#include "globals.h"
#include "preprocess.h"
#include "utils.h"

Options o;
AVFormatContext* input_file;
AVFormatContext* output_file;
StreamContext** stream_ctxs;

void open_input_file(const char* input_path) {
  int ret = avformat_open_input(&input_file, input_path, NULL, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "%s\n", av_err2str(ret));
    exit(1);
  }

  if (ret = avformat_find_stream_info(input_file, NULL) < 0) {
    av_log(NULL, AV_LOG_FATAL, "%s\n", av_err2str(ret));
    exit(1);
  }
}

void open_output_file() {
  int ret =
      avformat_alloc_output_context2(&output_file, NULL, NULL, o.output_path);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "%s\n", av_err2str(ret));
    exit(1);
  }

  int nb_streams = input_file->nb_streams;
  int output_index = 0;
  for (int i = 0; i < nb_streams; ++i) {
    if (stream_ctxs[i]->ignore)
      continue;

    stream_ctxs[i]->output_stream_index = output_index++;

    AVStream* output_stream = avformat_new_stream(output_file, NULL);
    if (!output_stream) {
      av_log(NULL, AV_LOG_FATAL, "Failed allocating output stream\n");
      exit(1);
    }

    AVCodecParameters* in_codecpar = input_file->streams[i]->codecpar;
    ret = avcodec_parameters_copy(output_stream->codecpar, in_codecpar);
    if (ret < 0) {
      av_log(NULL, AV_LOG_FATAL,
             "Failed to copy codec parameters on track %d\n", i);
      exit(1);
    }

    output_stream->time_base = input_file->streams[i]->time_base;
  }

  if (!(output_file->oformat->flags & AVFMT_NOFILE)) {
    ret = avio_open(&output_file->pb, o.output_path, AVIO_FLAG_WRITE);
    if (ret < 0) {
      av_log(NULL, AV_LOG_FATAL, "Could not open output file '%s'\n",
             o.output_path);
      exit(1);
    }
  }

  ret = avformat_write_header(output_file, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Error occurred when opening output file\n");
    exit(1);
  }
}

int main(int argc, const char** argv) {
  parse_options_from_argv(argc, argv);
  if (o.start_time_ms > o.end_time_ms) {
    av_log(NULL, AV_LOG_FATAL, "Start time cannot be later than end time\n");
    exit(1);
  }

  open_input_file(o.input_path);
  int nb_streams = input_file->nb_streams;
  allocate_stream_contexts(nb_streams);

  ignore_streams_from_argv(argc, argv);
  ignore_unsupported_streams();

  open_output_file();
  open_bsfs();

  do_preprocess();
  do_fast_cut();

  av_write_trailer(output_file);
  close_bsfs();
  free_stream_contexts(nb_streams);
  avformat_close_input(&input_file);
  if (output_file && !(output_file->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output_file->pb);
  avformat_free_context(output_file);
  return 0;
}
