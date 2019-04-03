//#include <glib.h>

#include "vf-file.h"
#include "vf-core.h"
#include <pthread.h>

void
vf_file_destroy(VfFile *video_file)
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
vf_file_load_metadata(VfFile *file)
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
vf_file_init(VfFile *video_file, const char *filename)
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
    VF_SET_AV_ERROR(video_file, ret);
    VF_DEBUG("av_open_input_file (%s, %d)\n", filename, ret);
    goto fail;
  }

  pthread_mutex_lock(&video_file->codec_mutex);
  if((ret = avformat_find_stream_info(format_context, NULL)) < 0)
  {
    VF_SET_AV_ERROR(video_file, ret);
    VF_WARN( "stream_info");
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
    VF_WARN( "video stream");
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

  vf_file_load_metadata(video_file);

  return true;

fail:
  VF_DEBUG( "VideoFile creation failed");
  return false;

}
