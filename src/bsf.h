#ifndef FASTCUT_BSF_H
#define FASTCUT_BSF_H
#include <libavformat/avformat.h>

void open_bsfs();
void close_bsfs();
void send_frame_to_bsf(AVPacket* pkt, int stream_index);
int get_bsf_frames(AVPacket* pkt, int stream_index);
#endif
