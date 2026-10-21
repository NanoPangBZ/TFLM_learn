# 定位并链接 vendored 的 SDL3 / SDL3_ttf（scripts/fetch_deps.py 下载）。
# 用法：
#   include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")
#   add_executable(foo main.c)
#   sdl3_attach(foo)          # SDL3
#   sdl3_ttf_attach(foo)      # SDL3_ttf + 字体（可选）

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT TARGET SDL3::SDL3)
    set(SDL3_PREFIX "${REPO_ROOT}/third_party/SDL3/x86_64-w64-mingw32")
    set(SDL3_DIR "${SDL3_PREFIX}/lib/cmake/SDL3" CACHE PATH "SDL3 CMake package directory")
    find_package(SDL3 CONFIG REQUIRED)
endif()

function(sdl3_attach target)
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_PREFIX}/bin/SDL3.dll" $<TARGET_FILE_DIR:${target}>)
    endif()
endfunction()

# SDL3_ttf（静态内置 FreeType）+ 复制 DejaVu 字体
if(NOT TARGET SDL3_ttf::SDL3_ttf)
    set(SDL3_TTF_PREFIX "${REPO_ROOT}/third_party/SDL3_ttf/x86_64-w64-mingw32")
    set(SDL3_ttf_DIR "${SDL3_TTF_PREFIX}/lib/cmake/SDL3_ttf" CACHE PATH "SDL3_ttf CMake package directory")
    find_package(SDL3_ttf CONFIG REQUIRED)
endif()

function(sdl3_ttf_attach target)
    target_link_libraries(${target} PRIVATE SDL3_ttf::SDL3_ttf)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_TTF_PREFIX}/bin/SDL3_ttf.dll" $<TARGET_FILE_DIR:${target}>)
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${REPO_ROOT}/third_party/fonts/DejaVuSans.ttf" $<TARGET_FILE_DIR:${target}>)
endfunction()
