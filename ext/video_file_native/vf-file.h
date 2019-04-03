/* vim: set tabstop=2 expandtab: */
#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdbool.h>


#define VF_SET_AV_ERROR(obj, error) \
    do { \
      obj->last_error = error; \
      av_strerror(error, obj->last_error_str, sizeof(obj->last_error_str)); \
   } while(0);

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
  char last_error_str[1024];
};

bool
vf_file_init(VfFile *video_file, const char* filename);

void
vf_file_destroy(VfFile* video_file);
