cmake_minimum_required(VERSION 3.16)
project(sdl_canvas C)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")

add_executable(sdl_canvas main.c)
sdl3_attach(sdl_canvas)

# 开启警告，尽早暴露问题（MSVC 用 /W4，GCC/Clang 用 -Wall -Wextra）。
if(MSVC)
    target_compile_options(sdl_canvas PRIVATE /W4)
else()
    target_compile_options(sdl_canvas PRIVATE -Wall -Wextra)
endif()
