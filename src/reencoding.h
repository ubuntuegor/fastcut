#ifndef FASTCUT_REENCODING_H
#define FASTCUT_REENCODING_H
#include <libavformat/avformat.h>

void encode_frame(int stream_index);
void reencode_packet(int stream_index, AVPacket* pkt);
#endif
