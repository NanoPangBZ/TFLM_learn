#pragma once

#include "bsp/bsp_display.h"
#include "bsp/bsp_touch.h"

/*****************************************************画布服务配置 start***********************************************************************/

/* 使能画布服务（触摸绘制；松开后连续 1 秒无触摸则保存并 trigger） */
#define CANVAS_SERVICE_ENABLE  1

#if defined(CANVAS_SERVICE_ENABLE) && (CANVAS_SERVICE_ENABLE == 1)

/* 轮询频率 (Hz)：越高笔迹越连续 */
#ifndef CANVAS_SERVICE_TASK_FREQ_HZ
#define CANVAS_SERVICE_TASK_FREQ_HZ  60
#endif

/* 松开后无触摸多久触发保存（ms） */
#ifndef CANVAS_SERVICE_IDLE_TIMEOUT_MS
#define CANVAS_SERVICE_IDLE_TIMEOUT_MS  1000
#endif

/* 笔刷半径（像素） */
#ifndef CANVAS_SERVICE_PEN_RADIUS
#define CANVAS_SERVICE_PEN_RADIUS  3
#endif

/* 背景色 / 笔色，0xRRGGBB */
#ifndef CANVAS_SERVICE_BG_COLOR
#define CANVAS_SERVICE_BG_COLOR   0xFFFFFF
#endif
#ifndef CANVAS_SERVICE_PEN_COLOR
#define CANVAS_SERVICE_PEN_COLOR  0x000000
#endif

#define CANVAS_SERVICE_BSP_DISPLAY_DESC \
    {                                   \
        .type = BSP_DISPLAY_TYPE_USER,  \
        .idx  = 0,                      \
        .attr = BSP_DISPLAY_ATTR_DEFAULT,\
    }

#define CANVAS_SERVICE_BSP_TOUCH_DESC   \
    {                                   \
        .type = BSP_TOUCH_TYPE_USER,    \
        .idx  = 0,                      \
        .attr = BSP_TOUCH_ATTR_DEFAULT, \
    }

#endif /* CANVAS_SERVICE_ENABLE */

/*****************************************************画布服务配置 end***********************************************************************/
