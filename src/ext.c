#include <ruby.h>
#include "vf-video-file.h"
#include "vf-thumbnailer.h"

void video_file_free(void* data)
{
    vf_file_destroy(data);
    free(data);
}

size_t video_file_size(const void* _data)
{
    return sizeof(VfFile);
}

static const rb_data_type_t video_file_type = {
    .wrap_struct_name = "video_file",
    .function = {
        .dmark = NULL,
        .dfree = video_file_free,
        .dsize = video_file_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE video_file_alloc(VALUE self)
{
    /* allocate */
    VfFile *video_file = RB_ZALLOC(VfFile);
    /* wrap */
    return TypedData_Wrap_Struct(self, &video_file_type, video_file);
}

VALUE video_file_m_initialize(VALUE self, VALUE path)
{
    Check_Type(path, T_STRING);

    VfFile *video_file;
    TypedData_Get_Struct(self, VfFile, &video_file_type, video_file);
    if(!vf_file_init(video_file, StringValueCStr(path))) {
        rb_raise(rb_eArgError, "%s", video_file->last_error_str);
    }
    return self;
}


//VALUE video_file_m_duration(VALUE self) {
//  vfVideoFile *video_file;
//  TypedData_Get_Struct(self, vfVideoFile, &video_file_type, video_file);

//  return rb_float_new(video_file->metadata.duration);
//}

#define VIDEO_FILE_DECL_METADATA_READER_INT(name) \
  VALUE video_file_m_ ## name (VALUE self) { \
    VfFile *video_file; \
    TypedData_Get_Struct(self, VfFile, &video_file_type, video_file); \
    return INT2FIX(video_file->metadata . name); \
  }

#define VIDEO_FILE_DECL_METADATA_READER_FLOAT(name) \
  VALUE video_file_m_ ## name (VALUE self) { \
    VfFile *video_file; \
    TypedData_Get_Struct(self, VfFile, &video_file_type, video_file); \
    return rb_float_new(video_file->metadata . name); \
  }

VIDEO_FILE_DECL_METADATA_READER_FLOAT(duration)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(fps)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(dar)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(par)

VIDEO_FILE_DECL_METADATA_READER_INT(width)
VIDEO_FILE_DECL_METADATA_READER_INT(height)



typedef struct {
  VfThumbnailer thumbnailer;
  VALUE video_file;
} Thumbnailer;


void thumbnailer_mark(void* data)
{
    Thumbnailer *thumbnailer = (Thumbnailer *) data;
    if(!RB_NIL_P(thumbnailer->video_file)) {
      rb_gc_mark(thumbnailer->video_file);
    }
}

void thumbnailer_free(void* data)
{
    vf_thumbnailer_destroy(data);
    free(data);
}

size_t thumbnailer_size(const void* _data)
{
    return sizeof(VfThumbnailer);
}

static const rb_data_type_t thumbnailer_type = {
    .wrap_struct_name = "thumbnailer",
    .function = {
        .dmark = thumbnailer_mark,
        .dfree = thumbnailer_free,
        .dsize = thumbnailer_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE thumbnailer_alloc(VALUE self)
{
    /* allocate */
    Thumbnailer *thumbnailer = RB_ZALLOC(Thumbnailer);
    thumbnailer->video_file = Qnil;

    /* wrap */
    return TypedData_Wrap_Struct(self, &thumbnailer_type, thumbnailer);
}

VALUE thumbnailer_m_initialize(VALUE self, VALUE rb_video_file, VALUE rb_width, VALUE rb_n)
{
    Check_TypedStruct(rb_video_file, &video_file_type);

    Thumbnailer *thumbnailer;
    VfFile *video_file;
    TypedData_Get_Struct(self, Thumbnailer, &thumbnailer_type, thumbnailer);
    TypedData_Get_Struct(rb_video_file, VfFile, &video_file_type, video_file);

    int width = FIX2INT(rb_width);
    unsigned n = FIX2UINT(rb_n);

    if(!vf_thumbnailer_init(&thumbnailer->thumbnailer, video_file, width, n)) {
        rb_raise(rb_eRuntimeError, "%s", thumbnailer->thumbnailer.last_error_str);
    }
    return self;
}

VALUE thumbnailer_m_get(VALUE self, VALUE rb_second, VALUE rb_filter_monoton, VALUE rb_accurate) {

    bool filter_monoton = RB_TEST(rb_filter_monoton);
    bool accurate = RB_TEST(rb_filter_monoton);
    double second = rb_num2dbl(rb_second);

    uint8_t *data = NULL;
    size_t size;

    Thumbnailer *thumbnailer;
    TypedData_Get_Struct(self, Thumbnailer, &thumbnailer_type, thumbnailer);

    bool succ = vf_thumbnailer_get_frame(&thumbnailer->thumbnailer, second, &data, &size, filter_monoton, accurate);

    if(size > 0 && data != NULL) {
      VALUE rb_data = rb_str_new((char *) data, size);
      free(data);
      return rb_data;
    } else {
        rb_raise(rb_eRuntimeError, "%s", thumbnailer->thumbnailer.last_error_str);
        return Qnil;
    }
}

void Init_libvideo_file()
{
    /* ... */

    VALUE mVideoFile = rb_define_module("VideoFile");
    VALUE cFile = rb_define_class_under(mVideoFile, "File", rb_cData);

    rb_define_alloc_func(cFile, video_file_alloc);
    rb_define_method(cFile, "initialize", video_file_m_initialize, 1);

    rb_define_method(cFile, "duration", video_file_m_duration, 0);
    rb_define_method(cFile, "fps", video_file_m_fps, 0);
    rb_define_method(cFile, "dar", video_file_m_dar, 0);
    rb_define_method(cFile, "par", video_file_m_par, 0);

    rb_define_method(cFile, "width", video_file_m_width, 0);
    rb_define_method(cFile, "height", video_file_m_height, 0);


    VALUE cThumbnailer = rb_define_class_under(mVideoFile, "Thumbnailer", rb_cData);
    rb_define_alloc_func(cThumbnailer, thumbnailer_alloc);
    rb_define_method(cThumbnailer, "initialize", thumbnailer_m_initialize, 3);
    rb_define_method(cThumbnailer, "get", thumbnailer_m_get, 3);
}
