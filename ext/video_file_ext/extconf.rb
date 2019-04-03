require 'mkmf'
%w(libavutil libavformat libavcodec libswscale libturbojpeg).each do |lib|
  pkg_config(lib)
end

create_makefile('video_file_ext')
