/*
 * Demo 0 — SDL3 基础界面：一个 28×28 的可绘制网格画布
 *
 * 学习目标：
 *   1. SDL3 的完整骨架：SDL_Init → 事件循环 → RenderClear/RenderPresent → 清理
 *   2. 把窗口当作"虚拟单片机帧缓冲"——数据是一块像素数组，渲染时画出来
 *   3. 这是后续 MNIST 手写数字识别项目的画板雏形
 *
 * 操作：
 *   鼠标左键按住拖动 —— 涂抹格子
 *   C                  —— 清空画布
 *   ESC / 关闭窗口      —— 退出
 *
 * 构建（在仓库根目录执行）：
 *   cmake -S demo/0_sdl_setup -B demo/0_sdl_setup/build -G "MinGW Makefiles"
 *   cmake --build demo/0_sdl_setup/build
 * 运行：demo/0_sdl_setup/build/sdl_canvas.exe
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <stdint.h>
#include <string.h>

#define GRID_SIZE 28                 /* 28x28，对应 MNIST 数字尺寸 */
#define CELL_PX   20                 /* 每个格子的像素边长 */
#define WINDOW_W  (GRID_SIZE * CELL_PX)   /* 560 */
#define WINDOW_H  (GRID_SIZE * CELL_PX)   /* 560 */

/* "帧缓冲"：0 = 空白，1 = 已涂抹。真实 MCU 上就是一块显存/位图。 */
static uint8_t canvas[GRID_SIZE][GRID_SIZE];

static void canvas_clear(void)
{
    memset(canvas, 0, sizeof(canvas));
}

static void canvas_paint(int cx, int cy)
{
    if (cx >= 0 && cx < GRID_SIZE && cy >= 0 && cy < GRID_SIZE)
        canvas[cy][cx] = 1;
}

/* 屏幕像素坐标 -> 网格坐标并涂抹 */
static void paint_at(float px, float py)
{
    canvas_paint((int)(px / CELL_PX), (int)(py / CELL_PX));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    canvas_clear();

    /* 1. 初始化视频子系统（SDL3 中 SDL_Init 返回 bool，失败为 false） */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* 2. 建窗口 + 渲染器 */
    SDL_Window *win = SDL_CreateWindow("SDL3 画布 — 鼠标涂抹 / C 清空 / ESC 退出",
                                       WINDOW_W, WINDOW_H, 0);
    if (!win) {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        SDL_Log("CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    const SDL_Color bg   = { 30,  30,  40, 255 };  /* 背景 */
    const SDL_Color grid = { 60,  60,  72, 255 };  /* 网格线 */
    const SDL_Color ink  = { 240, 200, 80, 255 };  /* 墨迹（已涂抹的格子） */

    int running = 1;
    int mouse_down = 0;

    /* 3. 事件循环：处理输入 -> 更新状态 -> 渲染 */
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_QUIT:
                running = 0;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_ESCAPE)
                    running = 0;
                else if (ev.key.key == SDLK_C)
                    canvas_clear();
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    mouse_down = 1;
                    paint_at(ev.button.x, ev.button.y);
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (ev.button.button == SDL_BUTTON_LEFT)
                    mouse_down = 0;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (mouse_down)
                    paint_at(ev.motion.x, ev.motion.y);
                break;
            default:
                break;
            }
        }

        /* 4. 渲染：清屏 -> 画墨迹 -> 画网格线 -> 上屏 */
        SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, ink.r, ink.g, ink.b, ink.a);
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                if (canvas[y][x]) {
                    SDL_FRect r = { (float)(x * CELL_PX), (float)(y * CELL_PX),
                                    (float)CELL_PX, (float)CELL_PX };
                    SDL_RenderFillRect(ren, &r);
                }
            }
        }

        SDL_SetRenderDrawColor(ren, grid.r, grid.g, grid.b, grid.a);
        for (int i = 0; i <= GRID_SIZE; i++) {
            float p = (float)(i * CELL_PX);
            SDL_RenderLine(ren, p, 0.0f, p, (float)WINDOW_H);
            SDL_RenderLine(ren, 0.0f, p, (float)WINDOW_W, p);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(16);   /* 约 60 FPS，避免空转烧 CPU */
    }

    /* 5. 清理（逆序释放） */
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
