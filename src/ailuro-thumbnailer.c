#include "ailuro-thumbnailer.h"
#include "ailuro-core.h"
#include <libavutil/imgutils.h>

#define FRAME_ALIGN 16

static void
planes_to_jpeg(AiluroThumbnailer* thumbnailer,
              int quality,
              unsigned char** rdata,
              size_t* size);

static int64_t
frame_monotony(AiluroThumbnailer* thumbnailer, AVFrame* frame);

void
ailuro_thumbnailer_destroy(AiluroThumbnailer* thumbnailer)
{
  unsigned n;
//  jpeg_destroy_compress(&thumbnailer->compressor);

  for (n = 0; n < thumbnailer->n; n++) {
    //    free(thumbnailer->scaled_frames[n]->data);
    av_frame_free(&thumbnailer->scaled_frames[n]);
  }
  av_frame_free(&thumbnailer->frame);
  av_frame_free(&thumbnailer->joined_frame);

  av_free(thumbnailer->scaled_frames_buffer);
  av_free(thumbnailer->joined_frames_buffer);

  tjDestroy(thumbnailer->tjInstance);
}

static void set_tj_error(AiluroThumbnailer *thumbnailer) {
  thumbnailer->last_error = tjGetErrorCode(thumbnailer->tjInstance);
  strncpy(thumbnailer->last_error_str, tjGetErrorStr2(thumbnailer->tjInstance), sizeof(thumbnailer->last_error_str));
  DEBUG("JPEG error: %s\n", thumbnailer->last_error_str);
}

bool
ailuro_thumbnailer_init(AiluroThumbnailer* thumbnailer,
                        AiluroVideoFile* file,
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

  DEBUG("OPEN CODEC\n");
  ailuro_video_file_open_codec(file);
  DEBUG("/OPEN CODEC\n");

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
    DEBUG("Could not create sws context\n");
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

  return thumbnailer;

fail:
  DEBUG("Thumbnailer creation failed\n");
  return NULL;
}

static void
set_av_error(AiluroThumbnailer* thumbnailer, int error) {
    thumbnailer->last_error = error;
    av_strerror(error, thumbnailer->last_error_str, sizeof(thumbnailer->last_error_str));
}

int
ailuro_thumbnailer_get_frame(AiluroThumbnailer* thumbnailer,
                             double seconds,
                             unsigned char** data,
                             size_t* size,
                             int filter_monoton)
{
  AVPacket* packet;
  AVFrame* frame;
  AVFormatContext* format_context;
  AVCodecContext* codec_context;
  AiluroVideoFile* file = thumbnailer->video_file;

  int done = 0;
  int before_frame = 0;
  int have_probe_packet = 0;

  unsigned n = 0;
  int video_stream_index;

  fprintf(stderr, "FILTER MONOTONY: %d\n", filter_monoton);

  ailuro_video_file_open_codec(file);

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

reseek:;

  DEBUG( "RESEEK\n");

  while (!before_frame) {
      DEBUG("Seeking to : %ld %ld\n", position, position);
      int ret = avformat_seek_file(format_context,
                                   video_stream_index, //-1,
                                   0,
                                   seek_position,
                                   seek_position,
                                   0);
      DEBUG("return: %d\n", ret);
      //      position = MAX(0, position - keyint_length);

      while (!have_probe_packet) {
        int ret = av_read_frame(format_context, packet);
        if (ret < 0) {
          set_av_error(thumbnailer, ret);
          goto fail;
        }
        have_probe_packet = (packet->stream_index == video_stream_index &&
                             packet->pts != AV_NOPTS_VALUE);

        DEBUG("probe_packet: %ld\n", packet->pts);
        if (!have_probe_packet) {
          av_packet_unref(packet);
        }
      }
      /* Once reached this packet is the allocated probe packet*/
      DEBUG("behind_frame packet: %ld <= %ld\n", packet->pts, video_position);
      before_frame = packet->pts <= video_position;
  }

  DEBUG("before frame found\n");
  bool first_decoded_frame = true;
  have_probe_packet = false;

  while (before_frame && !done) {
    int64_t prev_packet_pts = AV_NOPTS_VALUE;
    if (packet->stream_index == video_stream_index) {
      int ret;
      ret = avcodec_send_packet(codec_context, packet);
      DEBUG("send ret: %d (ts: %ld)\n", ret, packet->pts);
      if(ret < 0) {
          if(ret == AVERROR(EAGAIN) || ret == AVERROR_INVALIDDATA) {
            goto next_frame;
          } else {
            set_av_error(thumbnailer, ret);
            goto fail;
          }
      }

        ret = avcodec_receive_frame(codec_context, frame);
        if(ret < 0) {
            if(ret == AVERROR(EAGAIN)) {
              goto next_frame;
            } else {
              set_av_error(thumbnailer, ret);
              goto fail;
            }
        }

      if(packet->pts != AV_NOPTS_VALUE && packet->pts >= video_position) {
        DEBUG("possible decode frame %ld\n", packet->pts);

        if(first_decoded_frame) {
            if(packet->pts > video_position && !(prev_packet_pts < video_position)) {
              DEBUG("FIRST DECODED FRAME IS BEYOND target pos: %ld > %ld\n", packet->pts, video_position);
              int64_t seek_delta = av_rescale_q(5 * AV_TIME_BASE, AV_TIME_BASE_Q, file->video_stream->time_base);
              DEBUG("reseeking, old seek pos: %ld", seek_position);
              seek_position = MAX(0, seek_position - seek_delta);
              DEBUG("reseeking, new seek pos: %ld", seek_position);
              before_frame = false;
              av_packet_unref(packet);
              avcodec_flush_buffers(codec_context);
              goto reseek;
            } else {
              first_decoded_frame = false;
            }
        }
        DEBUG("recv ret: %d\n", ret);

        clock_t c = clock();
        DEBUG("decode: %lf\n", (double)(clock() - c) / CLOCKS_PER_SEC);
        DEBUG("GOT_FRAME! (%d/%d): %ld\n", n, thumbnailer->n, frame->pts);

        sws_scale(thumbnailer->sws_ctx,
                  (const uint8_t* const*)frame->data,
                  frame->linesize,
                  0,
                  codec_context->height,
                  (uint8_t* const*)thumbnailer->scaled_frames[n]->data,
                  thumbnailer->scaled_frames[n]->linesize);

        DEBUG("scale: %lf\n", (double)(clock() - c) / CLOCKS_PER_SEC);

        if (filter_monoton) {
          int64_t monotony =
            frame_monotony(thumbnailer, thumbnailer->scaled_frames[n]);
          DEBUG("MONOTONY: %ld\n", monotony);

          if (monotony < 40000 && frames_not_taken_because_of_monoty < 125) {
            frames_not_taken_because_of_monoty++;
            goto next_frame;
          }
        }

        frames_not_taken_because_of_monoty = 0;
        n++;
        DEBUG("FRAMES: %u/%u\n", n, thumbnailer->n);
        if (n == thumbnailer->n) {
          done = 1;
        }
    }
    prev_packet_pts = packet->pts;
  }

  next_frame:
    av_packet_unref(packet);
    if (!done) {
      if (av_read_frame(format_context, packet) < 0) {
        break;
      }
    }
    //thumbnailer->position = packet->pts;
  }
  DEBUG("encoding as jpeg...\n\n\n");

  planes_to_jpeg(thumbnailer, 80, data, size);

  return TRUE;

fail:
  *data = NULL;
  *size = 0;
  return FALSE;
}

static void
planes_to_jpeg(AiluroThumbnailer* thumbnailer,
               int quality,
               unsigned char** rdata,
               size_t* size)
{

  tjhandle tjInstance = thumbnailer->tjInstance;
  int flags = TJFLAG_FASTDCT;



  // join frames

  for (unsigned i = 0; i < 3; i++) {
    size_t off = 0;
    for (unsigned j = 0; j < thumbnailer->n; j++) {
      av_image_copy_plane(thumbnailer->joined_frame->data[i] + j * (unsigned) thumbnailer->width,
                          thumbnailer->joined_frame->linesize[i],
                          thumbnailer->scaled_frames[j]->data[i],
                          thumbnailer->scaled_frames[j]->linesize[i],
                          thumbnailer->width,
                          thumbnailer->height);

      off += thumbnailer->width * thumbnailer->height;
    }
  }

  const unsigned char** planes = (const unsigned char **) thumbnailer->joined_frame->data;
  int* strides = thumbnailer->joined_frame->linesize;
  *rdata = NULL;

  int error = tjCompressFromYUVPlanes(tjInstance,
                                      planes,
                                      (int) (thumbnailer->n * thumbnailer->width),
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

// static void
// frame_to_jpeg(AiluroThumbnailer *thumbnailer,
//              int quality, unsigned char **rdata,
//              size_t *size)
//{

//    clock_t c;
//    c = clock();
//    unsigned n;
//    AVFrame *frame = NULL;

//    /* 16 MCU size */
//    size_t n_scanlines = 8;//DCTSIZE;
//    struct jpeg_compress_struct *jc = &thumbnailer->compressor;

////    JSAMPROW *y = alloca(sizeof(JSAMPROW) * n_scanlines);
////    JSAMPROW *cb = alloca(sizeof(JSAMPROW) * n_scanlines);
////    JSAMPROW *cr = alloca(sizeof(JSAMPROW) * n_scanlines);

//    JSAMPROW y[n_scanlines],cb[n_scanlines ],cr[n_scanlines];
//    JSAMPARRAY data[3];

//    data[0] = y;
//    data[1] = cb;
//    data[2] = cr;

//    thumbnailer->compressor.comp_info[0].h_samp_factor = 1;
//    thumbnailer->compressor.comp_info[0].v_samp_factor = 1;
//    thumbnailer->compressor.comp_info[1].h_samp_factor = 1;
//    thumbnailer->compressor.comp_info[1].v_samp_factor = 1;
//    thumbnailer->compressor.comp_info[2].h_samp_factor = 1;
//    thumbnailer->compressor.comp_info[2].v_samp_factor = 1;
//    thumbnailer->compressor.max_h_samp_factor = 1;
//    thumbnailer->compressor.max_v_samp_factor = 1;

//    jpeg_set_quality(jc, quality, TRUE);
//    thumbnailer->compressor.dct_method = JDCT_FASTEST;

//    jpeg_mem_dest(jc, rdata, size);
//    jpeg_start_compress(jc, TRUE);

//    size_t pixels_per_scanline = (size_t) (thumbnailer->n * (unsigned)
//    thumbnailer->width);

////    uint8_t **src_cursors = alloca(3 * (size_t) thumbnailer->n *
/// sizeof(uint8_t *)); /    uint8_t *scanlines = alloca(3 * n_scanlines *
/// pixels_per_scanline * sizeof(uint8_t));

//    uint8_t *src_cursors[3][thumbnailer->n];
//    uint8_t scanlines[3][n_scanlines][pixels_per_scanline];

//    for(n = 0; n < thumbnailer->n; n++)
//    {
//       frame = thumbnailer->scaled_frames[n];
////       *(src_cursors + 0 * thumbnailer->n  + n) = frame->data[0];
////       *(src_cursors + 1 * thumbnailer->n + n) = frame->data[1];
////       *(src_cursors + 2 * thumbnailer->n + n) = frame->data[2];
//       src_cursors[0][n] = frame->data[0];
//       src_cursors[1][n] = frame->data[1];
//       src_cursors[2][n] = frame->data[2];

//    }

//    size_t plane;
//    size_t n_scanline_blocks = thumbnailer->compressor.image_height /
//    n_scanlines; size_t scanline_block; size_t scanline; uint8_t
//    *scanline_ptr;

//    int a = 0;

//    for(scanline_block = 0; scanline_block < n_scanline_blocks;
//    scanline_block++)
//    {
//      for(plane = 0; plane < 3; plane++)
//      {
//      	for(scanline = 0; scanline < n_scanlines; scanline++)
//        {
////          scanline_ptr = scanlines + plane * n_scanlines *
/// pixels_per_scanline + scanline * pixels_per_scanline;
//                  scanline_ptr = scanlines[plane][scanline];
//          for(n = 0; n < thumbnailer->n; n++)
//          {

////            memcpy(scanline_ptr, (src_cursors + plane * thumbnailer->n + n),
///(size_t) thumbnailer->width);
//            //scanline_ptr += thumbnailer->width;
////            *(src_cursors + plane * thumbnailer->n + n) +=
/// thumbnailer->width; //frame->linesize[plane];

//            memcpy(scanline_ptr, src_cursors[plane][n], thumbnailer->width);
//            scanline_ptr += thumbnailer->width;
//            src_cursors[plane][n] += thumbnailer->width;
//            //frame->linesize[plane];
//          }
////          data[plane][scanline] = scanlines + plane * n_scanlines *
/// pixels_per_scanline + scanline * pixels_per_scanline;
//          data[plane][scanline] = scanlines[plane][scanline];
//        }
//      }
//      jpeg_write_raw_data(jc, data, (JDIMENSION) n_scanlines);
//      a+= 16;
//      DEBUG("a = %d\n", n_scanlines);
//      DEBUG("%d %d\n", jc->next_scanline, jc->image_height);
//    }

//    DEBUG("a = %d\n", a);
//    DEBUG("%d %d\n", jc->next_scanline, jc->image_height);

//    jpeg_finish_compress(jc);

//    DEBUG("to jpeg: %lf\n", (double)(clock() - c) / CLOCKS_PER_SEC);
//}

 static int64_t
 frame_monotony(AiluroThumbnailer *thumbnailer, AVFrame *frame)
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
