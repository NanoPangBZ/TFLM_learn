#pragma once

/****************************产品定义**************************************/

// 固定头魔数
#define MAGIC_NUNBER "TFLMLearn"

// 系列 - 学习/验证工具
#define SERIES_NAME     "Learning Tools"
// 产品
#define PRODUCT_NAME    "TFLM_learn_framework"
// 项目
#define PROJECT_NAME    "TFLM_learn"
// 描述
#define BRIEF_DESCRIPTION "用于学习与验证 TFLM 的基础软件框架（SDL3 后端）"
// 硬件平台
#define BOARD_NAME "PC (SDL3 simulated MCU)"

/****************************版本定义**************************************/

#define MAJOR_VERSION 0
#define MINOR_VERSION 1
#define PATCH_VERSION 0

#define VERSION_IS_RELEASE      0   // 是否为发布版
#define VERSION_IS_STABLE       0   // 是否为稳定版
#define VERSION_IS_LTS          0   // 是否为长期支持版
#define VERSION_IS_DEPRECATED   0   // 是否为弃用版
#define VERSION_IS_EXPERIMENTAL 1   // 是否为实验性版
#define VERSION_IS_INTERNAL     1   // 是否为内部版
#define VERSION_IS_DEMO         0   // 是否为演示版
#define VERSION_IS_OEM          0   // 是否为OEM版本
#define VERSION_IS_OBM          0   // 是否为OBM版本

#define VERSION_ENABLE_CORE_DUMP        0
#define VERSION_ENABLE_OVERFLOW_CHECK   0
#define VERSION_ENABLE_ASSERTIONS       0
#define VERSION_ENABLE_DEBUG_LOGGING    0
#define VERSION_ENABLE_WDT              0
