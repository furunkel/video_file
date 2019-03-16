#include <ruby.h>
#include "vf-file.h"
#include "vf-thumbnailer.h"

static VALUE eError;

void file_free(void* data)
{
    vf_file_destroy(data);
    free(data);
}

size_t file_size(const void* _data)
{
    return sizeof(VfFile);
}

static const rb_data_type_t file_type = {
    .wrap_struct_name = "file",
    .function = {
        .dmark = NULL,
        .dfree = file_free,
        .dsize = file_size,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE file_alloc(VALUE self)
{
    /* allocate */
    VfFile *file = RB_ZALLOC(VfFile);
    /* wrap */
    return TypedData_Wrap_Struct(self, &file_type, file);
}

VALUE file_m_initialize(VALUE self, VALUE path)
{
    Check_Type(path, T_STRING);

    VfFile *file;
    TypedData_Get_Struct(self, VfFile, &file_type, file);
    if(!vf_file_init(file, StringValueCStr(path))) {
        rb_raise(eError, "%s", file->last_error_str);
    }
    return self;
}


//VALUE file_m_duration(VALUE self) {
//  vfVideoFile *file;
//  TypedData_Get_Struct(self, vfVideoFile, &file_type, file);

//  return rb_float_new(file->metadata.duration);
//}

#define VIDEO_FILE_DECL_METADATA_READER_INT(name) \
  VALUE file_m_ ## name (VALUE self) { \
    VfFile *file; \
    TypedData_Get_Struct(self, VfFile, &file_type, file); \
    return INT2FIX(file->metadata . name); \
  }

#define VIDEO_FILE_DECL_METADATA_READER_FLOAT(name) \
  VALUE file_m_ ## name (VALUE self) { \
    VfFile *file; \
    TypedData_Get_Struct(self, VfFile, &file_type, file); \
    return rb_float_new(file->metadata . name); \
  }

VIDEO_FILE_DECL_METADATA_READER_FLOAT(duration)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(fps)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(dar)
VIDEO_FILE_DECL_METADATA_READER_FLOAT(par)

VIDEO_FILE_DECL_METADATA_READER_INT(width)
VIDEO_FILE_DECL_METADATA_READER_INT(height)



typedef struct {
  VfThumbnailer thumbnailer;
  VALUE file;
} Thumbnailer;


void thumbnailer_mark(void* data)
{
    Thumbnailer *thumbnailer = (Thumbnailer *) data;
    if(!RB_NIL_P(thumbnailer->file)) {
      rb_gc_mark(thumbnailer->file);
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
    thumbnailer->file = Qnil;

    /* wrap */
    return TypedData_Wrap_Struct(self, &thumbnailer_type, thumbnailer);
}

VALUE thumbnailer_m_initialize(VALUE self, VALUE rb_file, VALUE rb_width, VALUE rb_n)
{
    Check_TypedStruct(rb_file, &file_type);

    Thumbnailer *thumbnailer;
    VfFile *file;
    TypedData_Get_Struct(self, Thumbnailer, &thumbnailer_type, thumbnailer);
    TypedData_Get_Struct(rb_file, VfFile, &file_type, file);

    int width = FIX2INT(rb_width);
    unsigned n = FIX2UINT(rb_n);

    if(!vf_thumbnailer_init(&thumbnailer->thumbnailer, file, width, n)) {
        rb_raise(eError, "%s", thumbnailer->thumbnailer.last_error_str);
    }
    return self;
}

VALUE thumbnailer_m_file(VALUE self) {
    Thumbnailer *thumbnailer;
    TypedData_Get_Struct(self, Thumbnailer, &thumbnailer_type, thumbnailer);
    return thumbnailer->file;
}

VALUE thumbnailer_m_get__(VALUE self, VALUE rb_second, VALUE rb_accurate, VALUE rb_filter_monoton) {

    double second = rb_num2dbl(rb_second);
    bool accurate = RB_TEST(rb_accurate);
    bool filter_monoton = RB_TEST(rb_filter_monoton);

    uint8_t *data = NULL;
    size_t size;

    Thumbnailer *thumbnailer;
    TypedData_Get_Struct(self, Thumbnailer, &thumbnailer_type, thumbnailer);

    bool succ = vf_thumbnailer_get_frame(&thumbnailer->thumbnailer, second, &data, &size, accurate, filter_monoton);

    if(size > 0 && data != NULL) {
      VALUE rb_data = rb_str_new((char *) data, size);
      free(data);
      return rb_data;
    } else {
        rb_raise(eError, "%s", thumbnailer->thumbnailer.last_error_str);
        return Qnil;
    }
}


void Init_libvideo_file()
{
    /* ... */

    VALUE mVideoFile = rb_define_module("VideoFile");
    VALUE cFile = rb_define_class_under(mVideoFile, "File", rb_cData);
    eError = rb_define_class_under(mVideoFile, "Error", rb_eStandardError);

    rb_define_alloc_func(cFile, file_alloc);
    rb_define_method(cFile, "initialize", file_m_initialize, 1);

    rb_define_method(cFile, "duration", file_m_duration, 0);
    rb_define_method(cFile, "fps", file_m_fps, 0);
    rb_define_method(cFile, "dar", file_m_dar, 0);
    rb_define_method(cFile, "par", file_m_par, 0);

    rb_define_method(cFile, "width", file_m_width, 0);
    rb_define_method(cFile, "height", file_m_height, 0);


    VALUE cThumbnailer = rb_define_class_under(mVideoFile, "Thumbnailer", rb_cData);
    rb_define_alloc_func(cThumbnailer, thumbnailer_alloc);
    rb_define_method(cThumbnailer, "initialize", thumbnailer_m_initialize, 3);
    rb_define_method(cThumbnailer, "file", thumbnailer_m_file, 0);
    rb_define_private_method(cThumbnailer, "get__", thumbnailer_m_get__, 3);
}
