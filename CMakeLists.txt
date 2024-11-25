cmake_minimum_required(VERSION 3.10)
project(ClipboardViewer)

set(CMAKE_CXX_STANDARD 17)

add_executable(ClipboardViewer main.cpp)

target_link_libraries(ClipboardViewer gdi32)