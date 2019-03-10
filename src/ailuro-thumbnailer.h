/* vim: set tabstop=2 expandtab: */
#pragma once

#include <stddef.h>
#include <stdio.h>
#include <jpeglib.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <pthread.h>

#include "ailuro-video-file.h"

typedef struct _AiluroThumbnailer AiluroThumbnailer;

struct _AiluroThumbnailer
{
  AiluroVideoFile *video_file;

  struct jpeg_compress_struct compressor;
  struct jpeg_error_mgr error_mgr;

  int width;
  int height;
  unsigned n;

  char* setup_filename;
  AVFrame* frame;
  AVFrame** scaled_frames;
  AVFrame *joined_frame;
  uint8_t *scaled_frames_buffer;
  uint8_t *joined_frames_buffer;
  AVPacket packet;
  int64_t position;

  struct SwsContext* sws_ctx;

  int last_error;
  char *last_error_str;

};

bool
ailuro_thumbnailer_init(AiluroThumbnailer *thumbnailer, AiluroVideoFile* file, int width, unsigned n);

int
ailuro_thumbnailer_get_frame(AiluroThumbnailer* thumbnailer,
                             double seconds,
                             unsigned char** data,
                             size_t* size,
                             int filter_monoton);

void
ailuro_thumbnailer_destroy(AiluroThumbnailer* thumbnailer);