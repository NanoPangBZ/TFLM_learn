#pragma once

#include <stddef.h>

struct TTF_Font;

#ifdef __cplusplus
extern "C" {
#endif

/* DejaVuSans.ttf 嵌入数据（由构建时 scripts/embed_bin.py 生成） */
extern const unsigned char g_embedded_dejavu_sans[];
extern const unsigned int  g_embedded_dejavu_sans_size;

/**
 * @brief 从嵌入字库打开 TTF 字体
 * @param ptsize 字号（pt）
 * @return 字体句柄；失败返回 NULL（调用前需已 TTF_Init）
 * @note 返回的字体由调用方 TTF_CloseFont；底层 IO 随 OpenFontIO(closeio=true) 自动关闭。
 */
struct TTF_Font* embedded_font_open_dejavu(float ptsize);

#ifdef __cplusplus
}
#endif
