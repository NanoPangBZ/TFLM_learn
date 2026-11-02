# 定位并静态链接 vendored 的 SDL3 / SDL3_ttf（scripts/build_sdl_static.py 编出）。
# 用法：
#   include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")
#   add_executable(foo main.c)
#   sdl3_attach(foo)          # SDL3（静态）
#   sdl3_ttf_attach(foo)      # SDL3_ttf + 字体（可选）
#
# 产物 exe 不依赖 SDL3.dll / SDL3_ttf.dll；MinGW 下再加 -static，
# 避免 libgcc/libstdc++/libwinpthread 等运行时 DLL。

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(SDL_STATIC_PREFIX "${REPO_ROOT}/third_party/sdl_static")
if(NOT EXISTS "${SDL_STATIC_PREFIX}/lib/cmake/SDL3/SDL3Config.cmake")
    message(FATAL_ERROR
        "Static SDL3 not found at:\n  ${SDL_STATIC_PREFIX}\n"
        "Run: powershell -ExecutionPolicy Bypass -File .\\setup.ps1\n"
        "  or: python scripts/build_sdl_static.py")
endif()

if(NOT TARGET SDL3::SDL3-static AND NOT TARGET SDL3::SDL3)
    set(SDL3_DIR "${SDL_STATIC_PREFIX}/lib/cmake/SDL3" CACHE PATH "SDL3 CMake package directory" FORCE)
    find_package(SDL3 CONFIG REQUIRED)
endif()

# MinGW：把 libgcc/libstdc++/winpthread 也链进 exe，做到无第三方 DLL 可拷走运行。
function(_tflm_mingw_fully_static target)
    if(MINGW)
        target_link_options(${target} PRIVATE -static)
    endif()
endfunction()

function(sdl3_attach target)
    if(TARGET SDL3::SDL3-static)
        target_link_libraries(${target} PRIVATE SDL3::SDL3-static)
    elseif(TARGET SDL3::SDL3)
        target_link_libraries(${target} PRIVATE SDL3::SDL3)
    else()
        message(FATAL_ERROR "SDL3 static/shared CMake target not found")
    endif()
    _tflm_mingw_fully_static(${target})
endfunction()

# SDL3_ttf（静态内置 FreeType）+ 复制 DejaVu 字体（数据文件，非 DLL）
if(NOT TARGET SDL3_ttf::SDL3_ttf-static AND NOT TARGET SDL3_ttf::SDL3_ttf)
    set(SDL3_ttf_DIR "${SDL_STATIC_PREFIX}/lib/cmake/SDL3_ttf" CACHE PATH "SDL3_ttf CMake package directory" FORCE)
    find_package(SDL3_ttf CONFIG REQUIRED)
endif()

function(sdl3_ttf_attach target)
    if(TARGET SDL3_ttf::SDL3_ttf-static)
        target_link_libraries(${target} PRIVATE SDL3_ttf::SDL3_ttf-static)
    elseif(TARGET SDL3_ttf::SDL3_ttf)
        target_link_libraries(${target} PRIVATE SDL3_ttf::SDL3_ttf)
    else()
        message(FATAL_ERROR "SDL3_ttf static/shared CMake target not found")
    endif()
    _tflm_mingw_fully_static(${target})
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${REPO_ROOT}/third_party/fonts/DejaVuSans.ttf" $<TARGET_FILE_DIR:${target}>)
endfunction()
