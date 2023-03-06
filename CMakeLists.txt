cmake_minimum_required(VERSION 2.8) 

project(multithread_downloader)

set(multithread_downloader_VERSION_MAJOR 1)
set(multithread_downloader_VERSION_MINOR 0)

configure_file(
  "${PROJECT_SOURCE_DIR}/version.h.in"
  "${PROJECT_BINARY_DIR}/version.h"
  )

set(CMAKE_CXX_COMPILER "g++")

set(CMAKE_CXX_FLAGS "-std=c++11 -lpthread")

include_directories ("${PROJECT_SOURCE_DIR}/downloaders/include")
add_subdirectory("${PROJECT_SOURCE_DIR}/downloaders")


add_executable (multithread_downloader multithread_downloader.cpp)
target_link_libraries (multithread_downloader downloaders curl) 