/* vim: set tabstop=2 expandtab: */
#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct _AiluroVideoFile AiluroVideoFile;

struct _AiluroVideoFile
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
ailuro_video_file_init(AiluroVideoFile *video_file, const char* filename);

int
ailuro_video_file_open_codec(AiluroVideoFile* video_file);

void
ailuro_video_file_destroy(AiluroVideoFile* video_file);
