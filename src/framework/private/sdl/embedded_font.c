#include "framework/embedded_font.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

TTF_Font* embedded_font_open_dejavu(float ptsize)
{
    SDL_IOStream* io;

    if (g_embedded_dejavu_sans_size == 0u) {
        return NULL;
    }

    io = SDL_IOFromConstMem(g_embedded_dejavu_sans, (size_t)g_embedded_dejavu_sans_size);
    if (!io) {
        return NULL;
    }

    /* closeio=true：字体关闭时一并释放 IOStream */
    return TTF_OpenFontIO(io, true, ptsize);
}
