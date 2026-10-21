/* Demo 0 — SDL3 全像素画板：左键画 / 右键擦 / C 清空 / ESC 退出 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdint.h>

#define FB_W 320
#define FB_H 320
#define BRUSH_RADIUS 4

/* RGBA8888 打包：高字节 R、低字节 A（0xRRGGBBAA） */
#define RGBA(r, g, b, a) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(g) << 16) | ((uint32_t)(r) << 24))
#define COLOR_BG   RGBA(40, 40, 60, 255)
#define COLOR_INK  RGBA(240, 200, 80, 255)

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

int main(int argc, char **argv)
{
    (void)argc; (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) { SDL_Log("%s", SDL_GetError()); return 1; }
    if (!TTF_Init()) { SDL_Log("%s", SDL_GetError()); return 1; }

    int fmaj, fmin, fpat;
    TTF_GetFreeTypeVersion(&fmaj, &fmin, &fpat);
    SDL_Log("FreeType %d.%d.%d", fmaj, fmin, fpat);

    SDL_Window *win = SDL_CreateWindow("SDL3 全像素画板", FB_W, FB_H, 0);
    SDL_Renderer *ren = win ? SDL_CreateRenderer(win, NULL) : NULL;
    SDL_Texture *tex = ren ? SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888,
                                               SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H) : NULL;
    if (!tex) { SDL_Log("%s", SDL_GetError()); goto done; }

    fb_clear();

    /* 文字提示：SDL3_ttf 渲染一次，每帧叠在画布底部 */
    char path[512];
    SDL_snprintf(path, sizeof(path), "%sDejaVuSans.ttf", SDL_GetBasePath());
    TTF_Font *font = TTF_OpenFont(path, 14.0f);
    SDL_Color fg = {190, 190, 210, 255};
    SDL_Surface *surf = font ? TTF_RenderText_Blended(font, "L: draw  R: erase  C: clear  Esc: quit",
                                                      SDL_strlen("L: draw  R: erase  C: clear  Esc: quit"), fg) : NULL;
    SDL_Texture *htex = surf ? SDL_CreateTextureFromSurface(ren, surf) : NULL;
    SDL_FRect hdst = {2.0f, 2.0f, 0.0f, 0.0f};
    if (htex) {
        hdst.y = (float)(FB_H - surf->h - 4);
        hdst.w = (float)surf->w;
        hdst.h = (float)surf->h;
    }

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

        SDL_UpdateTexture(tex, NULL, fb, FB_W * (int)sizeof(uint32_t));
        SDL_RenderTexture(ren, tex, NULL, NULL);
        if (htex) SDL_RenderTexture(ren, htex, NULL, &hdst);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

done:
    if (htex) SDL_DestroyTexture(htex);
    if (surf) SDL_DestroySurface(surf);
    if (font) TTF_CloseFont(font);
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
