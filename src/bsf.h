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

#ifndef FASTCUT_BSF_H
#define FASTCUT_BSF_H
#include <libavformat/avformat.h>

void open_bsfs();
void close_bsfs();
void send_frame_to_bsf(AVPacket* pkt, int stream_index);
int get_bsf_frames(AVPacket* pkt, int stream_index);
#endif
