cmake_minimum_required(VERSION 3.5)
project(video_file)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(SOURCE_FILES 
ext/video_file_ext/ext.c
ext/video_file_ext/vf-thumbnailer.c
ext/video_file_ext/vf-thumbnailer.h
ext/video_file_ext/vf-util.c
ext/video_file_ext/vf-util.h
ext/video_file_ext/vf-file.c
ext/video_file_ext/vf-file.h
)
pkg_check_modules(LIBAV REQUIRED libavutil libavformat libavcodec libswscale)
pkg_check_modules(JPEG REQUIRED libturbojpeg)

find_package (Threads REQUIRED)
find_package(Ruby REQUIRED)

add_library(video_file_ext SHARED ${SOURCE_FILES})

set_target_properties(video_file_ext PROPERTIES PREFIX "")
set_target_properties(video_file_ext PROPERTIES LIBRARY_OUTPUT_DIRECTORY  "${PROJECT_SOURCE_DIR}/lib")
target_compile_definitions(video_file_ext PRIVATE AV_ENABLE_DEBUG)
target_include_directories(video_file_ext PRIVATE ${RUBY_INCLUDE_DIRS})
target_link_libraries(video_file_ext ${LIBAV_LIBRARIES} ${JPEG_LIBRARIES} ${RUBY_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
