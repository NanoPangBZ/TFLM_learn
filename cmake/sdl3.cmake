# 定位并静态链接 vendored 的 SDL3 / SDL3_ttf（scripts/build_sdl_static.py 编出）。
# 用法：
#   include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")
#   add_executable(foo main.c)
#   sdl3_attach(foo)          # SDL3（静态）
#   sdl3_ttf_attach(foo)      # SDL3_ttf + 嵌入 DejaVu 字库（可选）
#
# 产物 exe 不依赖 SDL3.dll / SDL3_ttf.dll / 外部 .ttf；MinGW 下再加 -static。

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

# SDL3_ttf（静态内置 FreeType）+ 字库二进制嵌入可执行文件
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

    set(_font_ttf "${REPO_ROOT}/third_party/fonts/DejaVuSans.ttf")
    if(NOT EXISTS "${_font_ttf}")
        message(FATAL_ERROR "Font not found: ${_font_ttf}\nRun setup.ps1 / scripts/fetch_deps.py first.")
    endif()

    set(_gen_dir "${CMAKE_BINARY_DIR}/generated")
    set(_gen_font_c "${_gen_dir}/embedded_dejavu_sans.c")
    find_program(_embed_py NAMES python python3 py)
    if(NOT _embed_py)
        message(FATAL_ERROR "python not found; needed to embed DejaVuSans.ttf")
    endif()

    add_custom_command(
        OUTPUT "${_gen_font_c}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_gen_dir}"
        COMMAND "${_embed_py}"
                "${REPO_ROOT}/scripts/embed_bin.py"
                "${_font_ttf}"
                "${_gen_font_c}"
                "g_embedded_dejavu_sans"
        DEPENDS "${_font_ttf}" "${REPO_ROOT}/scripts/embed_bin.py"
        COMMENT "Embedding DejaVuSans.ttf into ${_gen_font_c}"
        VERBATIM)

    target_sources(${target} PRIVATE
        "${_gen_font_c}"
        "${REPO_ROOT}/src/framework/private/sdl/embedded_font.c")
endfunction()
