/* SDL3 smoke test: open a window, clear it, and quit.
 *
 * This is the "virtual MCU display" primtive the learning path builds on —
 * later projects will draw NN output (curves, digits, heatmaps) right here.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("SDL3 smoke", 320, 240, 0);
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

    SDL_SetRenderDrawColor(ren, 30, 40, 90, 255);
    SDL_RenderClear(ren);
    SDL_RenderPresent(ren);
    SDL_Delay(800);

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();

    SDL_Log("SDL3 %d.%d.%d OK", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
    return 0;
}
