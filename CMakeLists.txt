cmake_minimum_required(VERSION 3.16)

# TFLM_learn 基础软件框架（SDL3 后端）
# 目录结构镜像参考工程 YouWei Embedded Platform 的 src/ 布局：
#   src/main.c                   统一入口
#   src/framework/*.h            框架公开接口（与参考逐字一致）
#   src/framework/private/sdl/*.c SDL3 后端实现
#   src/lib/com_host/            通信主机库（自包含，逐字拷贝）
#   src/lib/symbol_table/        外部符号表
# 构建入口：根目录 build.bat -> scripts/build.py -> 本 CMakeLists
project(tflm_framework C)

# 未指定构建类型时默认 Debug（配合 build.py 的 debug/release 两个目标）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/sdl3.cmake")

set(FRAMEWORK_SOURCES
    src/main.c
    src/app/app_demo.c

    # ---- framework / private / sdl ----
    src/framework/private/sdl/e_critical.c
    src/framework/private/sdl/e_sched.c
    src/framework/private/sdl/e_mutex.c
    src/framework/private/sdl/e_sig.c
    src/framework/private/sdl/e_heap.c
    src/framework/private/sdl/e_log.c
    src/framework/private/sdl/e_polling_pool.c
    src/framework/private/sdl/e_pend_sv.c
    src/framework/private/sdl/e_framework_core.c
    src/framework/private/sdl/e_kv.c
    src/framework/private/sdl/e_com.c
    src/framework/private/sdl/e_online_monitor.c
    src/framework/private/sdl/c_list.c
    src/framework/private/sdl/c_queue.c
    src/framework/private/sdl/c_ringbuf.c
    src/framework/private/sdl/c_function_regedit.c
    src/framework/private/sdl/c_extern_symbol_table.c

    # ---- lib / com_host ----
    src/lib/com_host/com_host.c
    src/lib/com_host/private/com_host_packager.c
    src/lib/com_host/private/com_host_port.c
    src/lib/com_host/private/com_host_lib/com_host_crc16.c
    src/lib/com_host/private/com_host_lib/com_host_list.c
    src/lib/com_host/private/com_host_lib/com_host_ringbuf.c
    src/lib/com_host/private/com_host_ll_depend/com_host_ll_heap.c
    src/lib/com_host/private/com_host_ll_depend/com_host_ll_systime.c
    src/lib/com_host/private/com_host_package/com_host_your_way_studio_packager.c

    # ---- lib / symbol_table ----
    src/lib/symbol_table/symbol_table.c
)

add_executable(tflm_framework ${FRAMEWORK_SOURCES})

# 统一以 src/ 为包含根，使 framework/*.h、framework_cfg.h、version.h、
# lib/com_host/*.h 等按参考工程的 include 约定解析。
target_include_directories(tflm_framework PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

sdl3_attach(tflm_framework)

# 编译选项按构建类型：Debug 带调试符号关优化（断点单步），Release 开优化。
if(MSVC)
    target_compile_options(tflm_framework PRIVATE /W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(tflm_framework PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(tflm_framework PRIVATE /Zi /Od)
    endif()
else()
    target_compile_options(tflm_framework PRIVATE -Wall -Wextra)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(tflm_framework PRIVATE -O2 -DNDEBUG)
    else()
        target_compile_options(tflm_framework PRIVATE -g -O0)
    endif()
endif()
