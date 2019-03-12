/* vim: set tabstop=2 expandtab: */
#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct _VfFile VfFile;

struct _VfFile
{
  AVFormatContext* format_context;
  AVCodecContext* video_codec_context;
  AVCodecContext* audio_codec_context;
  char* filename;

  AVStream* video_stream;
  AVStream* audio_stream;

  pthread_mutex_t codec_mutex;

  struct {
    int height;
    int width;
    int fps_num;
    int fps_den;

    double duration;
    double fps;
    double dar;
    double par;
  } metadata;

  int last_error;
  char *last_error_str;
};

bool
vf_file_init(VfFile *video_file, const char* filename);

int
vf_file_open_codec(VfFile* video_file);

void
vf_file_destroy(VfFile* video_file);
