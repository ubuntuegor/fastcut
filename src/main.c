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

typedef enum StreamState {
  SEEKING_AND_FIRST_GOP,
  COPYING,
  LAST_GOP,
  ENDED
} StreamState;
typedef struct StreamContext {
  int ignore;
  int output_stream_index;
  enum AVMediaType type;
  AVCodecContext* dec_ctx;
  AVCodecContext* enc_ctx;
  AVFrame* frame;
  AVPacket* enc_pkt;
  int64_t start_pts;
  int64_t first_occured_pts;
  int64_t end_pts;
  int64_t pts_last_gop_start;
  StreamState state;
} StreamContext;

Options o;
AVFormatContext* input_file;
AVFormatContext* output_file;
StreamContext** stream_ctxs;

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

void allocate_stream_contexts(int nb_streams) {
  stream_ctxs = av_mallocz(nb_streams * sizeof(StreamContext*));
  for (int i = 0; i < nb_streams; ++i) {
    stream_ctxs[i] = av_mallocz(sizeof(StreamContext));
    stream_ctxs[i]->state = SEEKING_AND_FIRST_GOP;
    stream_ctxs[i]->ignore = 0;
    stream_ctxs[i]->start_pts = av_rescale_q(o.start_time_ms, AV_TIME_BASE_Q,
                                             input_file->streams[i]->time_base);
    stream_ctxs[i]->end_pts = av_rescale_q(o.end_time_ms, AV_TIME_BASE_Q,
                                           input_file->streams[i]->time_base);
    stream_ctxs[i]->pts_last_gop_start = AV_NOPTS_VALUE;
    stream_ctxs[i]->first_occured_pts = AV_NOPTS_VALUE;
    stream_ctxs[i]->type = input_file->streams[i]->codecpar->codec_type;
    stream_ctxs[i]->dec_ctx = NULL;
    stream_ctxs[i]->enc_ctx = NULL;
  }
}

void free_stream_contexts(int nb_streams) {
  for (int i = 0; i < nb_streams; ++i) {
    av_free(stream_ctxs[i]);
  }
  av_freep(&stream_ctxs);
}

void ignore_unsupported_streams() {
  // TODO
}

void ignore_streams_from_args(int argc, const char** argv) {
  // TODO
}

void first_pass() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_WARNING, "Failed to seek the file");
  }

  AVPacket* pkt = av_packet_alloc();
  int nb_streams = input_file->nb_streams;
  int last_frame_in_cut = 1;
  int* is_end_pts_in_stream = av_mallocz(nb_streams * sizeof(int));
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
  // AVCodecParameters* codecpar = avcodec_parameters_alloc();
  // avcodec_parameters_from_context(codecpar, stream_ctx->dec_ctx);
  // if (avcodec_parameters_to_context(
  //         stream_ctx->enc_ctx, input_file->streams[stream_index]->codecpar) <
  //     0) {
  //   av_log(NULL, AV_LOG_FATAL, "Failed to copy parameters\n");
  //   exit(1);
  // }
  // avcodec_parameters_free(&codecpar);
  stream_ctx->enc_ctx->time_base =
      output_file->streams[stream_ctx->output_stream_index]->time_base;

  ret = avcodec_open2(stream_ctx->enc_ctx, encoder, NULL);
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Cannot open video encoder for stream #%u\n",
           stream_index);
    exit(1);
  }

  stream_ctx->enc_pkt = av_packet_alloc();
}

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
      if (stream_ctx->enc_ctx == NULL)
        open_encoder(stream_index);
      encode_frame(stream_index);
    }
  }
  if (ret < 0) {
    av_log(NULL, AV_LOG_FATAL, "Decoding stream %u failed\n", stream_index);
    exit(1);
  }
}

void flush_decoder(int stream_index) {
  reencode_packet(stream_index, NULL);
}

void close_decoder(int stream_index) {
  av_frame_unref(stream_ctxs[stream_index]->frame);
  av_frame_free(&(stream_ctxs[stream_index]->frame));
  avcodec_free_context(&(stream_ctxs[stream_index]->dec_ctx));
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

void copy_packet(AVPacket* pkt) {
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

void process_video_packet(AVPacket* pkt) {
  int ret;
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  switch (stream_ctx->state) {
    case SEEKING_AND_FIRST_GOP:
      if (pkt->flags & AV_PKT_FLAG_KEY && pkt->pts >= stream_ctx->start_pts) {
        if (stream_ctx->dec_ctx != NULL) {
          flush_decoder(pkt->stream_index);
          close_decoder(pkt->stream_index);
        }
        if (stream_ctx->enc_ctx != NULL) {
          flush_encoder(pkt->stream_index);
          close_encoder(pkt->stream_index);
        }
        stream_ctx->state = COPYING;
        if (stream_ctx->first_occured_pts == AV_NOPTS_VALUE)
          stream_ctx->first_occured_pts = pkt->pts;
        process_video_packet(pkt);
        break;
      }
      if (stream_ctx->pts_last_gop_start != AV_NOPTS_VALUE &&
          pkt->pts >= stream_ctx->pts_last_gop_start) {
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

void process_copy_packet(AVPacket* pkt) {
  StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
  switch (stream_ctx->state) {
    case SEEKING_AND_FIRST_GOP:
      if (pkt->pts >= stream_ctx->start_pts) {
        stream_ctx->state = COPYING;
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

int have_all_streams_ended() {
  int res = 1;
  for (int i = 0; i < input_file->nb_streams; ++i) {
    if (!stream_ctxs[i]->ignore && stream_ctxs[i]->state != ENDED) {
      res = 0;
      break;
    }
  }
  return res;
}

void second_pass() {
  if (avformat_seek_file(input_file, -1, INT64_MIN, o.start_time_ms,
                         o.start_time_ms, 0) < 0) {
    av_log(NULL, AV_LOG_FATAL, "Failed to seek the file");
    exit(1);
  }

  AVPacket* pkt = av_packet_alloc();
  int i = 0;
  while (av_read_frame(input_file, pkt) >= 0) {
    StreamContext* stream_ctx = stream_ctxs[pkt->stream_index];
    switch (stream_ctx->type) {
      case AVMEDIA_TYPE_VIDEO:
        process_video_packet(pkt);
        break;
      default:
        process_copy_packet(pkt);
        break;
    }

    av_packet_unref(pkt);

    if (i % 50 == 0 && have_all_streams_ended()) {
      break;
    }

    i++;
  }

  av_packet_free(&pkt);
}

int main(int argc, const char** argv) {
  parse_options_from_argv(&o, argc, argv);
  if (o.start_time_ms > o.end_time_ms) {
    av_log(NULL, AV_LOG_FATAL, "Start time cannot be later than end time\n");
    exit(1);
  }

  open_input_file(o.input_path);
  int nb_streams = input_file->nb_streams;
  allocate_stream_contexts(nb_streams);

  ignore_unsupported_streams();
  ignore_streams_from_args(argc, argv);

  open_output_file();

  first_pass();
  second_pass();

  av_write_trailer(output_file);

  free_stream_contexts(nb_streams);
  avformat_close_input(&input_file);
  if (output_file && !(output_file->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output_file->pb);
  avformat_free_context(output_file);
  return 0;
}
