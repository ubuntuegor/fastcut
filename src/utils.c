#include "utils.h"
#include <libavutil/parseutils.h>
#include <stdlib.h>
#include "globals.h"

void print_usage() {
  av_log(NULL, AV_LOG_INFO,
         "Usage: fastcut [options] <input_file> <start_time> <end_time> "
         "<output_file>\n\n"
         "Options:\n"
         "  -i <n>       ignore track number n\n");
}

void parse_options_from_argv(int argc, const char** argv) {
  int param_count = 0;
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      // named parameter
      i++;
      continue;
    }

    switch (param_count) {
      case 0:
        o.input_path = argv[i];
        break;
      case 1:
        if (av_parse_time(&(o.start_time_ms), argv[i], 1) < 0) {
          av_log(NULL, AV_LOG_FATAL, "Failed to parse start time\n");
          exit(1);
        }
        break;
      case 2:
        if (av_parse_time(&(o.end_time_ms), argv[i], 1) < 0) {
          av_log(NULL, AV_LOG_FATAL, "Failed to parse end time\n");
          exit(1);
        }
        break;
      case 3:
        o.output_path = argv[i];
        break;
      default:
        av_log(NULL, AV_LOG_FATAL, "Too many parameters\n");
        print_usage();
        exit(1);
        break;
    }

    param_count++;
  }

  if (param_count < NB_OPTIONS_UNNAMED_PARAMS) {
    av_log(NULL, AV_LOG_FATAL, "Not enough parameters\n");
    print_usage();
    exit(1);
  }
}

void ignore_streams_from_argv(int argc, const char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && argv[i][1] == 'i' && argv[i][2] == '\0') {
      if (i == argc - 1) {
        av_log(NULL, AV_LOG_FATAL, "-i requires a track number\n");
        print_usage();
        exit(1);
      }
      i++;
      const char* end_ptr = argv[i];
      int stream_number = (int)strtol(argv[i], (char**)&end_ptr, 10);

      if (argv[i] == end_ptr || *end_ptr != '\0') {
        av_log(NULL, AV_LOG_FATAL, "-i requires a track number\n");
        print_usage();
        exit(1);
      }

      if (stream_number < 0 || stream_number >= input_file->nb_streams) {
        av_log(NULL, AV_LOG_FATAL, "Track number %d does not exist\n",
               stream_number);
        exit(1);
      }

      stream_ctxs[stream_number]->ignore = 1;
    }
  }
}

void ignore_unsupported_streams() {
  int nb_streams = input_file->nb_streams;
  for (int i = 0; i < nb_streams; ++i) {
    StreamContext* stream_ctx = stream_ctxs[i];
    if (stream_ctx->ignore)
      continue;
    if (stream_ctx->type == AVMEDIA_TYPE_VIDEO) {
      switch (input_file->streams[i]->codecpar->codec_id) {
        case AV_CODEC_ID_H264:
        case AV_CODEC_ID_H265:
          break;
        default:
          av_log(NULL, AV_LOG_WARNING,
                 "Track %u uses unsupported codec %s - ignoring\n", i,
                 avcodec_get_name(input_file->streams[i]->codecpar->codec_id));
          stream_ctx->ignore = 1;
          break;
      }
    }
  }
}
