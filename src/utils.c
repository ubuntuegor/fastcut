#include "utils.h"
#include <libavutil/parseutils.h>

void print_usage() {
  av_log(NULL, AV_LOG_INFO, "Usage: \n");
  // TODO
}

void parse_options_from_argv(int argc, const char** argv) {
  int param_count = 0;
  for (int i = 1; i < argc; i++) {
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

void ignore_unsupported_streams() {
  // TODO
}

void ignore_streams_from_argv(int argc, const char** argv) {
  // TODO
}
