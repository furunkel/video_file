cmake_minimum_required(VERSION 3.5)
project(video_file)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(SOURCE_FILES 
src/ext.c
src/vf-core.c
src/vf-core.h
src/vf-thumbnailer.c
src/vf-thumbnailer.h
src/vf-util.c
src/vf-util.h
src/vf-video-file.c
src/vf-video-file.h
)

pkg_check_modules(LIBAV REQUIRED libavutil libavformat libavcodec libswscale)
pkg_check_modules(JPEG REQUIRED libturbojpeg)

find_package (Threads REQUIRED)
find_package(Ruby REQUIRED)

add_library(video_file SHARED ${SOURCE_FILES})
target_include_directories(video_file PRIVATE ${RUBY_INCLUDE_DIRS})
target_link_libraries(video_file ${LIBAV_LIBRARIES} ${JPEG_LIBRARIES} ${RUBY_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
