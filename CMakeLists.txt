cmake_minimum_required(VERSION 3.5)
project(video_file)
find_package(PkgConfig REQUIRED)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(SOURCE_FILES 
ext/video_file_native/ext.c
ext/video_file_native/vf-thumbnailer.c
ext/video_file_native/vf-thumbnailer.h
ext/video_file_native/vf-util.c
ext/video_file_native/vf-util.h
ext/video_file_native/vf-file.c
ext/video_file_native/vf-file.h
ext/video_file_native/vf-core.c
ext/video_file_native/vf-core.h
)
pkg_check_modules(LIBAV REQUIRED libavutil libavformat libavcodec libswscale)
pkg_check_modules(JPEG REQUIRED libturbojpeg)

find_package (Threads REQUIRED)
find_package(Ruby REQUIRED)

add_library(video_file_native SHARED ${SOURCE_FILES})

set_target_properties(video_file_native PROPERTIES PREFIX "")
set_target_properties(video_file_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY  "${PROJECT_SOURCE_DIR}/lib")
target_compile_definitions(video_file_native PRIVATE VF_ENABLE_DEBUG)
target_include_directories(video_file_native PRIVATE ${RUBY_INCLUDE_DIRS})
target_link_libraries(video_file_native ${LIBAV_LIBRARIES} ${JPEG_LIBRARIES} ${RUBY_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
