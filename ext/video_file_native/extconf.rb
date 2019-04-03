require 'mkmf'

pkg_config('libavutil libavformat libavcodec libswscale libturbojpeg')

create_makefile('video_file_native')


