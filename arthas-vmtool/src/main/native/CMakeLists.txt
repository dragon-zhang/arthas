cmake_minimum_required(VERSION 3.17)
project(arthas-native)

set(CMAKE_CXX_STANDARD 14)

add_library(jni-lib SHARED src/jni-library.cpp)

#使用环境变量来include，把不同系统的兼容问题交给jdk解决
include_directories("include")
include_directories(head)

IF (WIN32)
    include_directories(head/windows)
    include_directories(head/windows/include)
    include_directories(head/windows/include/ddk)
    include_directories(head/windows/include/gdiplus)
    include_directories(head/windows/include/GL)
    include_directories(head/windows/include/psdk_inc)
    include_directories(head/windows/include/sdks)
    include_directories(head/windows/include/sec_api)
    include_directories(head/windows/include/sys)
    include_directories(head/windows/include/wrl)
ELSEIF (APPLE)
    include_directories(head/macos)
ELSEIF (common/linux)
    include_directories(head/linux)
ENDIF ()