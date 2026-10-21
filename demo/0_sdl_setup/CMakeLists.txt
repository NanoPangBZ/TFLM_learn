cmake_minimum_required(VERSION 3.16)
# 约定：target / 可执行文件名 = demo 目录名（0_sdl_setup），
# 这样 .vscode/launch.json 能按 demo/<名>/build/<名>.exe 通用定位调试目标。
project(0_sdl_setup C)

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")

add_executable(0_sdl_setup main.c)
sdl3_attach(0_sdl_setup)
sdl3_ttf_attach(0_sdl_setup)

# 开启警告 + 调试信息（-g 生成符号表，F5 断点调试必需；-O0 关闭优化，便于单步）。
if(MSVC)
    target_compile_options(0_sdl_setup PRIVATE /W4 /Zi /Od)
else()
    target_compile_options(0_sdl_setup PRIVATE -Wall -Wextra -g -O0)
endif()
