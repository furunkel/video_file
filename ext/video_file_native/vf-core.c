#include "vf-core.h"
#include <libavformat/avformat.h>

void vf_init() {
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
#endif
}
