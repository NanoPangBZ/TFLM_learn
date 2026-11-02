cmake_minimum_required(VERSION 3.16)

# TFLM_learn 基础软件框架（SDL3 后端）
# 根 CMakeLists 统一编译 framework / arch / 公共 bsp / lib；
# per-target 配置与 app/service 勾选见 src/target/<TFLM_TARGET>/。
# 构建入口：根目录 build.bat -> scripts/build.py -> 本 CMakeLists
# 可执行文件名与 TFLM_TARGET 一致（如 TFLM_LEARN.exe / platform_develop.exe）。
project(tflm_learn C)

# 未指定构建类型时默认 Debug（配合 build.py 的 debug/release 两个配置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

# 必须手动选择编译目标（由 build.py 扫描 src/target/ 后传入 -DTFLM_TARGET=...）
if(NOT DEFINED TFLM_TARGET OR TFLM_TARGET STREQUAL "")
    message(FATAL_ERROR
        "TFLM_TARGET is required. Example: -DTFLM_TARGET=TFLM_LEARN\n"
        "Targets are directories under src/target/ that contain CMakeLists.txt.")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/src/target/${TFLM_TARGET}/CMakeLists.txt")
    message(FATAL_ERROR
        "Unknown TFLM_TARGET='${TFLM_TARGET}': missing src/target/${TFLM_TARGET}/CMakeLists.txt")
endif()
message(STATUS "TFLM_TARGET=${TFLM_TARGET}")

# 子目录 CMakeLists 通过此变量追加源码 / 链接选项
set(TFLM_APP_TARGET ${TFLM_TARGET})

include("${CMAKE_CURRENT_LIST_DIR}/cmake/sdl3.cmake")

# 公共源码：framework / arch / 公共 bsp / lib / main
# app / service / encoder / test 由 src/target/<TFLM_TARGET>/CMakeLists.txt 追加
set(COMMON_SOURCES
    src/main.c

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

    # ---- arch ----
    src/arch/private/sdl/arch_init.c
    src/arch/private/sdl/arch_display.c

    # ---- bsp（公共；encoder 由需要它的 target 追加）----
    src/bsp/private/sdl/bsp_display_sdl.c
    src/bsp/private/sdl/bsp_touch_sdl.c
    src/bsp/private/sdl/bsp_storage_sdl.c
)

add_executable(${TFLM_APP_TARGET} ${COMMON_SOURCES})

# target cfg（含 version.h）优先，其次 src/
target_include_directories(${TFLM_APP_TARGET} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/target/${TFLM_TARGET}
    ${CMAKE_CURRENT_LIST_DIR}/src
)

sdl3_attach(${TFLM_APP_TARGET})

# 追加本目标的 app / service / 专属 bsp / test
set(TFLM_TARGET_NEED_TTF OFF)
add_subdirectory(src/target/${TFLM_TARGET})
# POST_BUILD 必须与 add_executable 同目录，故 ttf 在根侧 attach
if(TFLM_TARGET_NEED_TTF)
    sdl3_ttf_attach(${TFLM_APP_TARGET})
endif()

# 编译选项按构建类型：Debug 带调试符号关优化（断点单步），Release 开优化。
if(MSVC)
    target_compile_options(${TFLM_APP_TARGET} PRIVATE /W4)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${TFLM_APP_TARGET} PRIVATE /O2 /DNDEBUG)
    else()
        target_compile_options(${TFLM_APP_TARGET} PRIVATE /Zi /Od)
    endif()
else()
    target_compile_options(${TFLM_APP_TARGET} PRIVATE -Wall -Wextra)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${TFLM_APP_TARGET} PRIVATE -O2 -DNDEBUG)
    else()
        target_compile_options(${TFLM_APP_TARGET} PRIVATE -g -O0)
    endif()
endif()
