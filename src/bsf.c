#include "bsf.h"
#include "globals.h"

void open_bsfs() {
  int nb_streams = input_file->nb_streams;
  for (int i = 0; i < nb_streams; ++i) {
    StreamContext* stream_ctx = stream_ctxs[i];
    AVStream* in_stream = input_file->streams[i];
    AVStream* out_stream =
        output_file->streams[stream_ctx->output_stream_index];
    if (stream_ctx->ignore)
      continue;
    int ret;
    switch (in_stream->codecpar->codec_id) {
      case AV_CODEC_ID_H264:
        ret = av_bsf_list_parse_str("h264_mp4toannexb", &(stream_ctx->bsf_ctx));
        break;
      case AV_CODEC_ID_H265:
        ret = av_bsf_list_parse_str("hevc_mp4toannexb", &(stream_ctx->bsf_ctx));
        break;
      case AV_CODEC_ID_AAC:
        ret = av_bsf_list_parse_str("aac_adtstoasc", &(stream_ctx->bsf_ctx));
        break;
    }

    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING, "Failed to create BSF for track %u\n", i);
      continue;
    }

    AVBSFContext* bsf_ctx = stream_ctx->bsf_ctx;

    ret = avcodec_parameters_copy(bsf_ctx->par_in, out_stream->codecpar);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "Failed to copy BSF parameters for track %u\n", i);
      continue;
    }

    bsf_ctx->time_base_in = out_stream->time_base;

    ret = av_bsf_init(bsf_ctx);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "Failed to initialize bitstream filter: %s\n",
             bsf_ctx->filter->name);
      continue;
    }

    ret = avcodec_parameters_copy(out_stream->codecpar, bsf_ctx->par_out);
    if (ret < 0) {
      av_log(NULL, AV_LOG_WARNING,
             "Failed to copy BSF parameters for track %u\n", i);
      continue;
    }

    out_stream->time_base = bsf_ctx->time_base_out;
  }
}

void close_bsfs() {
  int nb_streams = input_file->nb_streams;
  for (int i = 0; i < nb_streams; ++i) {
    StreamContext* stream_ctx = stream_ctxs[i];
    if (stream_ctx->bsf_ctx != NULL) {
      av_bsf_free(&stream_ctx->bsf_ctx);
    }
  }
}

void send_frame_to_bsf(AVPacket* pkt, int stream_index) {
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  if (stream_ctx->bsf_ctx != NULL) {
    int ret = av_bsf_send_packet(stream_ctx->bsf_ctx, pkt);
    if (ret < 0 && ret != AVERROR_EOF) {
      av_log(NULL, AV_LOG_FATAL,
             "Failed sending packet to bsf on stream %u\n%s\n",
             pkt->stream_index, av_err2str(ret));
      exit(1);
    }
  }
}

int get_bsf_frames(AVPacket* pkt, int stream_index) {
  StreamContext* stream_ctx = stream_ctxs[stream_index];
  int ret = av_bsf_receive_packet(stream_ctx->bsf_ctx, pkt);
  if (ret < 0 && ret != AVERROR_EOF && ret != AVERROR(EAGAIN)) {
    av_log(NULL, AV_LOG_FATAL,
           "Failed receiving packet from bsf on stream %u\n%s\n",
           pkt->stream_index, av_err2str(ret));
    exit(1);
  }
  return ret;
}
