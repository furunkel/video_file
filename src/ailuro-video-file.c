//#include <glib.h>

#include "ailuro-video-file.h"
#include "ailuro-core.h"
#include <pthread.h>

int
ailuro_video_file_open_codec(AiluroVideoFile *video_file)
{
  AVCodec *codec;
  int ret;

  DEBUG( "CODEC MUTEX: %p\n", video_file->codec_mutex);
  pthread_mutex_lock(&video_file->codec_mutex);
  DEBUG( "CODEC MUTEX LOCKED: %p\n", video_file->codec_mutex);
  if(video_file->video_codec_context->codec == NULL)
  {
    DEBUG( "FIND DECODER\n");
    codec = avcodec_find_decoder(video_file->video_codec_context->codec_id);
    DEBUG( "/FIND DECODER\n");
    if(codec == NULL)
    {
      DEBUG( "Could not find codec");
      return FALSE;  
    }

    DEBUG( "OPEN CODEC\n");
    if((ret = avcodec_open2(video_file->video_codec_context, codec, NULL)) < 0)
    {
      DEBUG( "Could not open codec (%d)\n", ret);
      return FALSE;
    }
    DEBUG( "/OPEN CODEC\n");
  }
  pthread_mutex_unlock(&video_file->codec_mutex);
  return TRUE;
}


void
ailuro_video_file_destroy(AiluroVideoFile *video_file)
{
  pthread_mutex_lock(&video_file->codec_mutex);
  if(video_file->video_codec_context != NULL)
    avcodec_close(video_file->video_codec_context);

  if(video_file->audio_codec_context != NULL)
    avcodec_close(video_file->audio_codec_context);
  pthread_mutex_unlock(&video_file->codec_mutex);

  if(video_file->format_context != NULL) {
    avformat_close_input(&video_file->format_context);
  }

  pthread_mutex_destroy(&video_file->codec_mutex);
  free(video_file->filename);
}

static void
ailuro_video_file_load_metadata(AiluroVideoFile *file)
{
  int dar_num;
  int dar_den;

  file->metadata.width = file->video_codec_context->width;
  file->metadata.height = file->video_codec_context->height;

  file->metadata.duration = ((double)file->format_context->duration) / (double)AV_TIME_BASE;
  file->metadata.fps = av_q2d(file->video_stream->r_frame_rate);

  file->metadata.par = av_q2d(file->video_stream->sample_aspect_ratio);

  av_reduce(&dar_num, &dar_den,
           file->video_codec_context->width * file->video_stream->sample_aspect_ratio.num,
           file->video_codec_context->height * file->video_stream->sample_aspect_ratio.den,
           1024*1024);

  file->metadata.dar = (double) dar_num / (double) dar_den;
}

bool
ailuro_video_file_init(AiluroVideoFile *video_file, const char *filename)
{
  AVFormatContext *format_context = NULL;
  AVCodecContext *video_codec_context = NULL;
  AVCodecContext *audio_codec_context = NULL;
  AVStream *video_stream = NULL;
  AVStream *audio_stream = NULL;
  unsigned i;
  int ret;

  video_file->filename = strdup(filename);

  pthread_mutex_init(&video_file->codec_mutex, NULL);

  if((ret = avformat_open_input(&format_context, filename, NULL, NULL)) != 0)
  {
    WARN( "av_open_input_file (%s, %d)\n", filename, ret);
    goto fail;
  }

  pthread_mutex_lock(&video_file->codec_mutex);
  if(avformat_find_stream_info(format_context, NULL) < 0)
  {
    WARN( "stream_info\n");
    goto fail;
  }
  pthread_mutex_unlock(&video_file->codec_mutex);

  for(i = 0; i < format_context->nb_streams; i++)
  {
    if(video_stream == NULL && format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    {
      video_stream = format_context->streams[i];
    }

    if(audio_stream == NULL && format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      audio_stream = format_context->streams[i];
    }
  }

  if(video_stream == NULL)
  {
    WARN( "video stream\n");
    goto fail;  
  }

  video_codec_context = avcodec_alloc_context3(NULL);
  audio_codec_context = avcodec_alloc_context3(NULL);

  avcodec_parameters_to_context(video_codec_context, video_stream->codecpar);

  if(audio_stream != NULL) {
    avcodec_parameters_to_context(audio_codec_context, audio_stream->codecpar);
  }

//  if(video_codec_context->time_base.num > 1000 && video_codec_context->time_base.den == 1)
//    video_codec_context->time_base.den=1000;

  video_file->format_context = format_context;
  video_file->video_codec_context = video_codec_context;
  video_file->audio_codec_context = video_codec_context;
  video_file->audio_stream = audio_stream;
  video_file->video_stream = video_stream;

  ailuro_video_file_load_metadata(video_file);

  return true;

fail:
  video_file->last_error = errno;
  video_file->last_error_str = strerror(errno);
  WARN( "VideoFile creation failed\n");
  return false;

}
