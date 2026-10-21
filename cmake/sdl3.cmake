# Shared setup for the vendored SDL3 (downloaded by scripts/fetch_sdl3.py).
#
# After including this file you get:
#   - target SDL3::SDL3            -> link against it
#   - function sdl3_attach(<tgt>)  -> link + copy SDL3.dll next to the exe (Windows)
#
# Usage in a demo's CMakeLists.txt:
#   include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/sdl3.cmake")
#   add_executable(foo main.c)
#   sdl3_attach(foo)

if(NOT TARGET SDL3::SDL3)
    # Repo root is the parent of this file's directory (cmake/).
    get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(SDL3_PREFIX "${REPO_ROOT}/third_party/SDL3/x86_64-w64-mingw32")
    set(SDL3_DIR "${SDL3_PREFIX}/lib/cmake/SDL3" CACHE PATH "SDL3 CMake package directory")
    find_package(SDL3 CONFIG REQUIRED)
endif()

function(sdl3_attach target)
    target_link_libraries(${target} PRIVATE SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL3_PREFIX}/bin/SDL3.dll"
                $<TARGET_FILE_DIR:${target}>
            COMMENT "Copying SDL3.dll")
    endif()
endfunction()
