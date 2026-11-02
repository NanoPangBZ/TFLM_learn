# platform_develop：平台 / BSP 开发目标。
# 含编码器与 BSP 测试；默认不编 num_discern / canvas。

target_sources(${TFLM_APP_TARGET} PRIVATE
    # ----- bsp encoder / keyboard（本目标专属）-----
    ${CMAKE_SOURCE_DIR}/src/bsp/private/sdl/bsp_encoder_sdl.c
    ${CMAKE_SOURCE_DIR}/src/bsp/private/sdl/bsp_keyboard_sdl.c

    # ----- test -----
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_display/bsp_display_test.c
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_touch/bsp_touch_test.c
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_encoder/bsp_encoder_test.c
    ${CMAKE_SOURCE_DIR}/src/test/bsp_keyboard/bsp_keyboard_test.c
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_audio_system/bsp_audio_system_test.c
    ${CMAKE_SOURCE_DIR}/src/test/bsp_audio_system/bsp_audio_system_all_mix_test.c
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_storage/bsp_storage_test.c
    # ${CMAKE_SOURCE_DIR}/src/test/bsp_storage/bsp_storage_rw_test.c

    # ----- app / service（需要时取消注释）-----
    # ${CMAKE_SOURCE_DIR}/src/app/common/debug_display.c
    # ${CMAKE_SOURCE_DIR}/src/service/canvas/canvas_service.c
)

# 编码器名字用 SDL_ttf；attach 须在根目录（目标创建处）执行
set(TFLM_TARGET_NEED_TTF ON PARENT_SCOPE)
