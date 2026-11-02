# TFLM_LEARN：手写数字 / TFLM 学习目标。
# 不编编码器；app / service 在此勾选，framework 由根 CMakeLists 统一包含。

# ============================================================================
# 数字识别 app 选择：直接注释 / 取消注释源码，不使用 CMake option。
#
# 当前全部注释（仅跑公共框架）。启用某一版时只保留一组。
# ============================================================================

set(TFLM_LEARN_SOURCES
    # ----- app 公共调试显示（中间图像输出到 bsp_display 区域）-----
    ${CMAKE_SOURCE_DIR}/src/app/common/debug_display.c

    # ----- num_discern_1bit（1-bit 20x20）-----
    ${CMAKE_SOURCE_DIR}/src/app/num_discern_1bit/num_discern_1bit.c
    ${CMAKE_SOURCE_DIR}/src/app/num_discern_1bit/num_discern_1bit_model.c

    # ----- uint8 28x28 版 -----
    # ${CMAKE_SOURCE_DIR}/src/app/num_discern_uint8/num_discern_uint8.c
    # ${CMAKE_SOURCE_DIR}/src/app/num_discern_uint8/num_discern_uint8_model.c

    # ----- float 28x28 版 -----
    # ${CMAKE_SOURCE_DIR}/src/app/num_discern/num_discern.c
    # ${CMAKE_SOURCE_DIR}/src/app/num_discern/num_discern_model.c
    # ${CMAKE_SOURCE_DIR}/src/test/canvas_service/canvas_service_test.c

    # ----- service -----
    ${CMAKE_SOURCE_DIR}/src/service/canvas/canvas_service.c
)

if(TFLM_LEARN_SOURCES)
    target_sources(${TFLM_APP_TARGET} PRIVATE ${TFLM_LEARN_SOURCES})
endif()
