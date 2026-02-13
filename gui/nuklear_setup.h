/*
 * nuklear_setup.h — Nuklear configuration and implementation for SpeciesID GUI.
 *
 * Include this ONCE in a single .c file (main_gui.c). It pulls in the
 * nuklear implementation and the SDL2 renderer backend.
 */
#ifndef SPECIESID_NUKLEAR_SETUP_H
#define SPECIESID_NUKLEAR_SETUP_H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION

/* SDL2 headers live in <SDL2/…> on Homebrew / system installs, but the
   nuklear_sdl_renderer.h backend expects bare <SDL.h>.  The -I flag from
   sdl2-config already adds the SDL2/ directory, so <SDL.h> resolves.       */
#define NK_SDL_RENDERER_SDL_H <SDL.h>

#include "nuklear.h"

#define NK_SDL_RENDERER_IMPLEMENTATION
#include "nuklear_sdl_renderer.h"

#endif /* SPECIESID_NUKLEAR_SETUP_H */
