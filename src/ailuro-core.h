#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
//#include <glib.h>

#define DEBUG_FLAG

void
ailuro_core_init();

#define WARN(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

#ifdef DEBUG_FLAG
#define DEBUG(format, ...) fprintf(stderr, format, ##__VA_ARGS__)
#else
#define DEBUG(format, ...)
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef ABS
#define ABS(a) (((a) < 0) ? -(a) : (a))
#endif

#ifndef MAX
#define MAX(a, b) ((a < b) ? (b) : (a))
#endif

#ifndef MIN
#define MIN(a, b) ((a < b) ? (a) : (b))
#endif
