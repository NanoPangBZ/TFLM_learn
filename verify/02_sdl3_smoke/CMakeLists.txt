cmake_minimum_required(VERSION 3.16)
project(sdl3_smoke C)

# 与主工程一致：使用 third_party/sdl_static 静态库（scripts/build_sdl_static.py）
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")

add_executable(sdl3_smoke main.c)
sdl3_attach(sdl3_smoke)
