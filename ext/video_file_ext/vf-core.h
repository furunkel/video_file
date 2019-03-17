#pragma once

#define VF_WARN(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)

#ifdef VF_ENABLE_DEBUG
#define VF_DEBUG(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)
#else
#define VF_DEBUG(format, ...)
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
