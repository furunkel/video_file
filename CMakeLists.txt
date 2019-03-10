cmake_minimum_required(VERSION 3.5)
project(video_file)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")
set(SOURCE_FILES 
src/ext.c
src/ailuro-core.c
src/ailuro-core.h
src/ailuro-thumbnailer.c
src/ailuro-thumbnailer.h
src/ailuro-util.c
src/ailuro-util.h
src/ailuro-video-file.c
src/ailuro-video-file.h
)

pkg_check_modules(LIBAV REQUIRED libavutil libavformat libavcodec libswscale)
pkg_check_modules(JPEG REQUIRED libturbojpeg)

find_package (Threads REQUIRED)
find_package(Ruby REQUIRED)

add_library(video_file SHARED ${SOURCE_FILES})
target_include_directories(video_file PRIVATE ${RUBY_INCLUDE_DIRS})
target_link_libraries(video_file ${LIBAV_LIBRARIES} ${JPEG_LIBRARIES} ${RUBY_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
