/* vim: set tabstop=2 expandtab: */
#pragma once

#include <stddef.h>
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <pthread.h>
#include <turbojpeg.h>

#include "vf-video-file.h"

typedef struct _VfThumbnailer VfThumbnailer;

struct _VfThumbnailer
{
  VfFile *video_file;

  int width;
  int height;
  unsigned n;

  AVFrame* frame;
  AVFrame** scaled_frames;
  AVFrame *joined_frame;
  uint8_t *scaled_frames_buffer;
  uint8_t *joined_frames_buffer;
  AVPacket packet;
  int64_t position;

  struct SwsContext* sws_ctx;

  tjhandle *tjInstance;

  int last_error;
  char last_error_str[1024];

};

bool
vf_thumbnailer_init(VfThumbnailer *thumbnailer, VfFile* file, int width, unsigned n);

bool
vf_thumbnailer_get_frame(VfThumbnailer* thumbnailer,
                             double seconds,
                             unsigned char** data,
                             size_t* size,
                             bool filter_monoton,
                             bool accurate);

void
vf_thumbnailer_destroy(VfThumbnailer* thumbnailer);
