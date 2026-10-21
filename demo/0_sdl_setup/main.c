/* Demo 0 — SDL3 全像素画板：左键画 / 右键擦 / C 清空 / ESC 退出 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdint.h>

#define FB_W 320
#define FB_H 320
#define BRUSH_RADIUS 4

/* RGBA8888 打包：高字节 R、低字节 A（0xRRGGBBAA） */
#define RGBA(r, g, b, a) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(g) << 16) | ((uint32_t)(r) << 24))
#define COLOR_BG   RGBA(40, 40, 60, 255)
#define COLOR_INK  RGBA(240, 200, 80, 255)
#define COLOR_HINT RGBA(190, 190, 210, 255)

static uint32_t fb[FB_W * FB_H];

static void fb_clear(void)
{
    for (int i = 0; i < FB_W * FB_H; i++) fb[i] = COLOR_BG;
}

static void fb_put(int x, int y, uint32_t c)
{
    if (x >= 0 && x < FB_W && y >= 0 && y < FB_H) fb[y * FB_W + x] = c;
}

static void fb_brush(int cx, int cy, int r, uint32_t c)
{
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx * dx + dy * dy <= r * r) fb_put(cx + dx, cy + dy, c);
}

/* 两点间按半径间隔插值盖章，快速拖动不断笔 */
static void fb_stroke(int x0, int y0, int x1, int y1, int r, uint32_t c)
{
    int dx = x1 - x0, dy = y1 - y0;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int steps = (ax > ay ? ax : ay) / r;
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; i++)
        fb_brush(x0 + dx * i / steps, y0 + dy * i / steps, r, c);
}

/* 5x7 点阵字，与 FONT_CHARS 顺序对应，每字节低 5 位为一行 */
static const char FONT_CHARS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ:";
static const uint8_t FONT_GLYPHS[][7] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F},
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
};

static const uint8_t *font_glyph(char c)
{
    if (c >= 'a' && c <= 'z') c -= 32;
    for (int i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return FONT_GLYPHS[i];
    return FONT_GLYPHS[0];
}

static void fb_text(int x, int y, const char *s, uint32_t c, int scale)
{
    while (*s) {
        const uint8_t *g = font_glyph(*s++);
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (g[row] & (1u << (4 - col)))
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++)
                            fb_put(x + col * scale + sx, y + row * scale + sy, c);
        x += 6 * scale;
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("%s", SDL_GetError()); return 1; }

    SDL_Window *win = SDL_CreateWindow("SDL3 全像素画板", FB_W, FB_H, 0);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H) : NULL;
    if (!tex) { SDL_Log("%s", SDL_GetError()); goto done; }

    fb_clear();

    int running = 1, mouse_down = 0, prev_x = 0, prev_y = 0;
    uint32_t color = COLOR_INK;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = 0;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_ESCAPE) running = 0;
                else if (ev.key.key == SDLK_C) fb_clear();
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button == SDL_BUTTON_LEFT || ev.button.button == SDL_BUTTON_RIGHT) {
                    mouse_down = 1;
                    color = ev.button.button == SDL_BUTTON_LEFT ? COLOR_INK : COLOR_BG;
                    prev_x = (int)ev.button.x;
                    prev_y = (int)ev.button.y;
                    fb_brush(prev_x, prev_y, BRUSH_RADIUS, color);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ev.button.button == SDL_BUTTON_LEFT || ev.button.button == SDL_BUTTON_RIGHT)
                    mouse_down = 0;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouse_down) {
                    int cx = (int)ev.motion.x, cy = (int)ev.motion.y;
                    fb_stroke(prev_x, prev_y, cx, cy, BRUSH_RADIUS, color);
                    prev_x = cx;
                    prev_y = cy;
                }
                break;
            }
        }

        fb_text(2, FB_H - 8, "L:DRAW R:ERASE C:CLEAR ESC:QUIT", COLOR_HINT, 1);
        SDL_UpdateTexture(tex, NULL, fb, FB_W * (int)sizeof(uint32_t));
        SDL_RenderTexture(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

done:
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
