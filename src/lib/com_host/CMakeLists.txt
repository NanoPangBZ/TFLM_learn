cmake_minimum_required(VERSION 3.10)

project(com_host)

# 仅在作为顶层项目独立构建时才设置这些全局配置，
# 作为子模块（add_subdirectory）时不污染父项目。
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # 设置C++标准
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    # 添加调试信息
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug)
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_C_FLAGS_DEBUG "-g -O0")
endif()

# 手动逐文件指定源文件
set(ALL_SOURCES
    com_host.c
    private/com_host_packager.c
    private/com_host_port.c
    private/com_host_lib/com_host_crc16.c
    private/com_host_lib/com_host_list.c
    private/com_host_lib/com_host_ringbuf.c
    private/com_host_ll_depend/com_host_ll_heap.c
    private/com_host_ll_depend/com_host_ll_systime.c
    private/com_host_package/com_host_your_way_studio_packager.c
)

# 创建静态库
add_library(${PROJECT_NAME} STATIC ${ALL_SOURCES})

# 将头文件目录绑定到 target（PUBLIC），父项目通过
# target_link_libraries(<parent_target> com_host) 即可自动继承包含路径
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# MSVC：对 target 本身启用 /utf-8，避免影响父项目其他 target
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()
