#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/parseutils.h>

const int N_UNNAMED_PARAMS = 4;
typedef struct Options {
  const char* input_path;
  int64_t start_time_ms;
  int64_t end_time_ms;
  const char* output_path;
} Options;

typedef enum TrackState {
  SEEKING,
  FIRST_GOP,
  COPYING,
  LAST_GOP,
  ENDED
} TrackState;
typedef struct TrackContext {
  int ignore;
  enum AVMediaType type;
  AVCodecContext* dec_ctx;
  AVCodecContext* enc_ctx;
  int64_t end_pts;
  int64_t pts_last_gop_start;
  TrackState state;
} TrackContext;

Options o;
AVFormatContext* input_file;
TrackContext** track_ctxs;

void print_usage() {
  av_log(NULL, AV_LOG_INFO, "Usage: \n");
  // TODO
}

void parse_options_from_argv(Options* o, int argc, const char** argv) {
  int param_count = 0;
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] != '\0') {
      // named parameter
      i++;
      continue;
    }

    switch (param_count) {
      case 0:
        o->input_path = argv[i];
        break;
      case 1:
        if (av_parse_time(&(o->start_time_ms), argv[i], 1) < 0) {
          av_log(NULL, AV_LOG_FATAL, "Failed to parse start time\n");
          exit(1);
        }
        break;
      case 2:
        if (av_parse_time(&(o->end_time_ms), argv[i], 1) < 0) {
          av_log(NULL, AV_LOG_FATAL, "Failed to parse end time\n");
          exit(1);
        }
        break;
      case 3:
        o->output_path = argv[i];
        break;
      default:
        av_log(NULL, AV_LOG_FATAL, "Too many parameters\n");
        print_usage();
        exit(1);
        break;
    }

    param_count++;
  }

  if (param_count < N_UNNAMED_PARAMS) {
    av_log(NULL, AV_LOG_FATAL, "Not enough parameters\n");
    print_usage();
    exit(1);
  }
}

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

void allocate_track_contexts(uint nb_tracks) {
  track_ctxs = av_mallocz(nb_tracks * sizeof(TrackContext*));
  for (uint i = 0; i < nb_tracks; ++i) {
    track_ctxs[i] = av_mallocz(sizeof(TrackContext));
    track_ctxs[i]->state = SEEKING;
    track_ctxs[i]->ignore = 0;
    track_ctxs[i]->end_pts = av_rescale_q(o.end_time_ms, AV_TIME_BASE_Q,
                                          input_file->streams[i]->time_base);
    track_ctxs[i]->pts_last_gop_start = AV_NOPTS_VALUE;
    track_ctxs[i]->type = input_file->streams[i]->codecpar->codec_type;
  }
}

void ignore_unsupported_tracks() {
  // TODO
}

void ignore_tracks_from_args(int argc, const char** argv) {
  // TODO
}

void first_pass() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_WARNING, "Failed to seek the file");
  }

  AVPacket pkt;
  while (av_read_frame(input_file, &pkt) >= 0) {
    TrackContext* track_ctx = track_ctxs[pkt.stream_index];
    if (track_ctx->type != AVMEDIA_TYPE_VIDEO || track_ctx->ignore)
      continue;
    if (pkt.flags & AV_PKT_FLAG_KEY && pkt.pts != AV_NOPTS_VALUE) {
      if (pkt.pts < track_ctx->end_pts) {
        track_ctx->pts_last_gop_start = pkt.pts;
      }
    }
  }
}

int main(int argc, const char** argv) {
  parse_options_from_argv(&o, argc, argv);
  if (o.start_time_ms > o.end_time_ms) {
    av_log(NULL, AV_LOG_FATAL, "Start time cannot be later than end time\n");
    exit(1);
  }

  open_input_file(o.input_path);
  uint nb_tracks = input_file->nb_streams;
  allocate_track_contexts(nb_tracks);

  ignore_unsupported_tracks();
  ignore_tracks_from_args(argc, argv);

  first_pass();

  for (int i = 0; i < nb_tracks; ++i) {
    printf("%d: %d > %d\n", i, track_ctxs[i]->end_pts,
           track_ctxs[i]->pts_last_gop_start);
  }

  // first pass of file: remember cut points pts for each track that needs
  // to be cut

  // ? get a list of keyframes ?
  // seek to start keyframe
  // start encoding from specified frame until next keyframe (unless end
  // frame is before key) copy until final keyframe encode until last
  // keyframe

  avformat_close_input(&input_file);
  return 0;
}
