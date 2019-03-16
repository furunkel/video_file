#include "vf-thumbnailer.h"
#include "vf-core.h"
#include <libavutil/imgutils.h>

#define FRAME_ALIGN 16

static void
planes_to_jpeg(VfThumbnailer* thumbnailer,
              int quality,
              unsigned char** rdata,
              size_t* size);

static int64_t
frame_monotony(VfThumbnailer* thumbnailer, AVFrame* frame);

static bool
vf_file_open_codec(VfFile *video_file, VfThumbnailer *thumbnailer)
{
  AVCodec *codec;
  int ret;

  VF_DEBUG( "CODEC MUTEX");
  pthread_mutex_lock(&video_file->codec_mutex);
  VF_DEBUG( "CODEC MUTEX LOCKED");
  if(video_file->video_codec_context->codec == NULL)
  {
    VF_DEBUG( "FIND DECODER");
    codec = avcodec_find_decoder(video_file->video_codec_context->codec_id);
    VF_DEBUG( "/FIND DECODER");
    if(codec == NULL)
    {
      VF_DEBUG( "Could not find codec");
      return false;
    }

    VF_DEBUG( "OPEN CODEC");
    if((ret = avcodec_open2(video_file->video_codec_context, codec, NULL)) < 0)
    {
      VF_DEBUG( "Could not open codec (%d)\n", ret);
      VF_SET_AV_ERROR(thumbnailer, ret);
      return false;
    }
    VF_DEBUG( "/OPEN CODEC");
  }
  pthread_mutex_unlock(&video_file->codec_mutex);
  return true;
}

void
vf_thumbnailer_destroy(VfThumbnailer* thumbnailer)
{
  unsigned n;
//  jpeg_destroy_compress(&thumbnailer->compressor);

  if(thumbnailer->scaled_frames != NULL) {
    for (n = 0; n < thumbnailer->n; n++) {
      //    free(thumbnailer->scaled_frames[n]->data);
      av_frame_free(&thumbnailer->scaled_frames[n]);
    }
  }
  av_frame_free(&thumbnailer->frame);
  av_frame_free(&thumbnailer->joined_frame);

  av_free(thumbnailer->scaled_frames_buffer);
  av_free(thumbnailer->joined_frames_buffer);

  tjDestroy(thumbnailer->tjInstance);
}

static void set_tj_error(VfThumbnailer *thumbnailer) {
  thumbnailer->last_error = tjGetErrorCode(thumbnailer->tjInstance);
  strncpy(thumbnailer->last_error_str, tjGetErrorStr2(thumbnailer->tjInstance), sizeof(thumbnailer->last_error_str));
  VF_DEBUG("JPEG error: %s\n", thumbnailer->last_error_str);
}

bool
vf_thumbnailer_init(VfThumbnailer* thumbnailer,
                        VfFile* file,
                        int width,
                        unsigned n)
{
  unsigned int i;
  int height;
  size_t buffer_size;

  thumbnailer->video_file = file;
  thumbnailer->width = width;
  thumbnailer->position = -1;

  if (n <= 0)
    n = 1;

  thumbnailer->n = n;

  VF_DEBUG("OPEN CODEC");

  if(!vf_file_open_codec(file, thumbnailer)) {
      return false;
  }

  VF_DEBUG("/OPEN CODEC");

  if (file->video_stream->sample_aspect_ratio.num &&
      av_cmp_q(file->video_stream->sample_aspect_ratio,
               file->video_stream->codecpar->sample_aspect_ratio)) {
    /*  AVRational display_aspect_ratio;
      av_reduce(&display_aspect_ratio.num, &display_aspect_ratio.den,
                codec_context->width * video_stream->sample_aspect_ratio.num,
                codec_context->height * video_stream->sample_aspect_ratio.den,
                1024*1024);*/

    height = width *
             (file->video_stream->sample_aspect_ratio.den *
              file->video_codec_context->height) /
             (file->video_stream->sample_aspect_ratio.num *
              file->video_codec_context->width);
  } else
    /* Fallback to 16:9 */
    height = (int)(width / 1.7777);

  //  height = size *
  //  format_context->streams[thumbnailer->video_stream_index]->sample_aspect_ratio.den
  //  /
  //                 format_context->streams[thumbnailer->video_stream_index]->sample_aspect_ratio.num;

  thumbnailer->sws_ctx = sws_getContext(file->video_codec_context->width,
                                        file->video_codec_context->height,
                                        file->video_codec_context->pix_fmt,
                                        width,
                                        height,
                                        AV_PIX_FMT_YUV444P,
                                        SWS_FAST_BILINEAR,
                                        NULL,
                                        NULL,
                                        NULL);

  if (thumbnailer->sws_ctx == NULL) {
    VF_DEBUG("Could not create sws context");
    goto fail;
  }

  thumbnailer->width = width;
  thumbnailer->height = height;
  thumbnailer->frame = av_frame_alloc();

  thumbnailer->scaled_frames = av_mallocz_array(n, sizeof(AVFrame*));
  size_t frame_size =
    (size_t) av_image_get_buffer_size(AV_PIX_FMT_YUV444P, width, height, FRAME_ALIGN);
  buffer_size = n * frame_size;
  thumbnailer->scaled_frames_buffer = av_mallocz((size_t)buffer_size);

  thumbnailer->joined_frame = av_frame_alloc();

  int joined_buffer_size =
    av_image_get_buffer_size(AV_PIX_FMT_YUV444P, width * (int)n, height, FRAME_ALIGN);
  thumbnailer->joined_frames_buffer = av_mallocz((size_t)joined_buffer_size);

    av_image_fill_arrays(thumbnailer->joined_frame->data,
                         thumbnailer->joined_frame->linesize,
                         thumbnailer->joined_frames_buffer,
                         AV_PIX_FMT_YUV444P, width * (int)n, height, FRAME_ALIGN);


  for (i = 0; i < n; i++) {
    size_t offset = (size_t)frame_size * i;
    thumbnailer->scaled_frames[i] = av_frame_alloc();
    av_image_fill_arrays(thumbnailer->scaled_frames[i]->data,
                         thumbnailer->scaled_frames[i]->linesize,
                         thumbnailer->scaled_frames_buffer + offset,
                         AV_PIX_FMT_YUV444P,
                         width,
                         height,
                         FRAME_ALIGN);
  }

  thumbnailer->tjInstance = tjInitCompress();
  if (thumbnailer->tjInstance == NULL) {
    set_tj_error(thumbnailer);
    goto fail;
  }

  return true;

fail:
  VF_DEBUG("Thumbnailer creation failed");
  return false;
}

bool
vf_thumbnailer_get_frame(VfThumbnailer* thumbnailer,
                             double seconds,
                             unsigned char** data,
                             size_t* size,
                             bool accurate,
                             bool filter_monoton)
{
  AVPacket* packet;
  AVFrame* frame;
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  VfFile* file = thumbnailer->video_file;

  bool done = false;
  bool first_decoded_frame;
  unsigned n = 0;
  int video_stream_index;

  VF_DEBUG("FILTER MONOTONY: %d", filter_monoton);

  if(!vf_file_open_codec(file, thumbnailer)) {
      VF_DEBUG("opening codec failed");
      goto fail;
  }

  packet = &thumbnailer->packet;

  format_context = file->format_context;
  codec_context = file->video_codec_context;
  video_stream_index = file->video_stream->index;
  frame = thumbnailer->frame;

  int64_t position = (int64_t)(AV_TIME_BASE * seconds);
  if (format_context->duration < position || position < 0) {
    goto fail;
  }

  if (format_context->start_time != AV_NOPTS_VALUE)
    position += format_context->start_time;

  int frames_not_taken_because_of_monoty = 0;

  int64_t video_position = av_rescale_q(position, AV_TIME_BASE_Q, file->video_stream->time_base);
  int64_t seek_position = video_position;

reseek: {
    VF_DEBUG("seeking to %ld...\n", seek_position);
    int ret = avformat_seek_file(format_context,
                               video_stream_index, //-1,
                               0,
                               seek_position,
                               seek_position,
                               0);

    if(ret < 0) {
      VF_DEBUG("seek_file error: %d", ret);
      VF_SET_AV_ERROR(thumbnailer, ret);
      goto fail;
    }
  }

  first_decoded_frame = true;

  while(!done) {

    int ret = av_read_frame(format_context, packet);
    if(ret < 0) {
      VF_DEBUG("read_frame error: %d", ret);
      VF_SET_AV_ERROR(thumbnailer, ret);
      goto fail;
    }

    int64_t prev_packet_pts = AV_NOPTS_VALUE;
    if (packet->stream_index == video_stream_index) {
      int ret;
      ret = avcodec_send_packet(codec_context, packet);
      VF_DEBUG("send ret: %d (ts: %ld)\n", ret, packet->pts);
      if(ret < 0) {
          if(ret == AVERROR(EAGAIN) || ret == AVERROR_INVALIDDATA) {
            goto next_frame;
          } else {
            VF_SET_AV_ERROR(thumbnailer, ret);
            goto fail;
          }
      }

      ret = avcodec_receive_frame(codec_context, frame);
      VF_DEBUG("recv ret: %d (ts: %ld)\n", ret, packet->pts);
      if(ret < 0) {
          if(ret == AVERROR(EAGAIN)) {
            goto next_frame;
          } else {
            VF_SET_AV_ERROR(thumbnailer, ret);
            goto fail;
          }
      }

      if(packet->pts != AV_NOPTS_VALUE && packet->pts >= video_position) {
        VF_DEBUG("possible decode frame %ld\n", packet->pts);

        if(first_decoded_frame) {
            if(accurate && (packet->pts > video_position && !(prev_packet_pts < video_position))) {
              VF_DEBUG("FIRST DECODED FRAME IS BEYOND target pos: %ld > %ld\n", packet->pts, video_position);
              int64_t seek_delta = av_rescale_q(5 * AV_TIME_BASE, AV_TIME_BASE_Q, file->video_stream->time_base);
              VF_DEBUG("reseeking, old seek pos: %ld", seek_position);
              seek_position = MAX(0, seek_position - seek_delta);
              VF_DEBUG("reseeking, new seek pos: %ld", seek_position);
              av_packet_unref(packet);
              avcodec_flush_buffers(codec_context);
              goto reseek;
            } else {
              first_decoded_frame = false;
            }
        }
        VF_DEBUG("recv ret: %d\n", ret);

        clock_t c = clock();
        VF_DEBUG("decode: %lf\n", (double)(clock() - c) / CLOCKS_PER_SEC);
        VF_DEBUG("GOT_FRAME! (%d/%d): %ld\n", n, thumbnailer->n, frame->pts);

        sws_scale(thumbnailer->sws_ctx,
                  (const uint8_t* const*)frame->data,
                  frame->linesize,
                  0,
                  codec_context->height,
                  (uint8_t* const*)thumbnailer->scaled_frames[n]->data,
                  thumbnailer->scaled_frames[n]->linesize);

        VF_DEBUG("scale: %lf\n", (double)(clock() - c) / CLOCKS_PER_SEC);

        if (filter_monoton) {
          int64_t monotony =
            frame_monotony(thumbnailer, thumbnailer->scaled_frames[n]);
          VF_DEBUG("MONOTONY: %ld\n", monotony);

          if (monotony < 40000 && frames_not_taken_because_of_monoty < 125) {
            frames_not_taken_because_of_monoty++;
            goto next_frame;
          }
        }

        frames_not_taken_because_of_monoty = 0;
        n++;
        VF_DEBUG("FRAMES: %u/%u\n", n, thumbnailer->n);
        if (n == thumbnailer->n) {
          done = 1;
        }
    }
    prev_packet_pts = packet->pts;
  }

  next_frame:
    av_packet_unref(packet);
  }

  VF_DEBUG("encoding as jpeg...\n");

  planes_to_jpeg(thumbnailer, 80, data, size);

  return true;

fail:
  *data = NULL;
  *size = 0;
  return false;
}

static void
planes_to_jpeg(VfThumbnailer* thumbnailer,
               int quality,
               unsigned char** rdata,
               size_t* size)
{

  tjhandle tjInstance = thumbnailer->tjInstance;
  int flags = TJFLAG_FASTDCT;



  // join frames

  for (unsigned i = 0; i < 3; i++) {
    for (unsigned j = 0; j < thumbnailer->n; j++) {
      av_image_copy_plane(thumbnailer->joined_frame->data[i] + j * (unsigned) thumbnailer->width,
                          thumbnailer->joined_frame->linesize[i],
                          thumbnailer->scaled_frames[j]->data[i],
                          thumbnailer->scaled_frames[j]->linesize[i],
                          thumbnailer->width,
                          thumbnailer->height);
    }
  }

  const unsigned char** planes = (unsigned char **) thumbnailer->joined_frame->data;
  int* strides = thumbnailer->joined_frame->linesize;
  *rdata = NULL;

  int error = tjCompressFromYUVPlanes(tjInstance,
                                      planes,
                                      (int)thumbnailer->n * thumbnailer->width,
                                      strides,
                                      thumbnailer->height,
                                      TJSAMP_444,
                                      rdata,
                                      size,
                                      quality,
                                      flags);
  if (error < 0) {
    set_tj_error(thumbnailer);
    goto error;
  }

  return;
error:
  *rdata = NULL;
  *size = 0;
}

 static int64_t
 frame_monotony(VfThumbnailer *thumbnailer, AVFrame *frame)
{
  int64_t monotony;

  int i, width, height;
  width = thumbnailer->width;
  height = thumbnailer->height;

  monotony = 0;

  for(i = width + 1; i < height * (width - 1); i+= 16)
  {

    monotony += ABS(frame->data[0][i] - frame->data[0][i - 1]) +
                ABS( *(frame->data[0] + width + i) - *(frame->data[0] + width
                + i - 1)) + ABS( *(frame->data[0] - width + i) -
                *(frame->data[0] - width + i - 1));
  }

  return monotony;
}
