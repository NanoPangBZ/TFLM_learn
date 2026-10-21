cmake_minimum_required(VERSION 3.16)
project(sdl3_smoke C)

# SDL3 is vendored by scripts/fetch_sdl3.py into third_party/SDL3/.
# The mingw package ships a CMake config; point SDL3_DIR at it.
set(SDL3_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../third_party/SDL3/x86_64-w64-mingw32")
set(SDL3_DIR "${SDL3_PREFIX}/lib/cmake/SDL3" CACHE PATH "SDL3 CMake package directory")
find_package(SDL3 CONFIG REQUIRED)

add_executable(sdl3_smoke main.c)
target_link_libraries(sdl3_smoke PRIVATE SDL3::SDL3)

# Copy SDL3.dll next to the executable so it runs without touching PATH.
add_custom_command(TARGET sdl3_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL3_PREFIX}/bin/SDL3.dll"
        $<TARGET_FILE_DIR:sdl3_smoke>
    COMMENT "Copying SDL3.dll")
