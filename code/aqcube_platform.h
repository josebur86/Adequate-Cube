#pragma once

#if __cplusplus
extern "C" {
#endif

#include "aqcube_arena.h"

//
// Debug File Operations
//

// Note(joe): These are services to the game provided by the platform layer.
typedef struct read_file_result
{
    void *Contents;
    int SizeInBytes;
} read_file_result;
read_file_result DEBUGWin32ReadFile(char *Filename);
bool DEBUGWin32WriteFile(char *Filename, void *Memory, u64 FileSize);
void Win32FreeMemory(void *Memory);

typedef loaded_bitmap debug_load_bitmap(arena *Arena, char *Filename);

struct font_glyph
{
    bool IsLoaded;
    loaded_bitmap Glyph;
    u32 Baseline;
    s32 Top;
    s32 Bottom;
    s32 ToLeftEdge;
    s32 AdvanceWidth;
};
typedef font_glyph debug_load_font_glyph(arena *Arena, char C);
typedef s32 debug_get_font_kern_advance_for(char A, char B);

// Note(joe): These are services to the platform layer provided by the game.
typedef struct game_memory
{
    u64 PermanentStorageSize;
    void *PermanentStorage; // This should always be cleared to zero.
    u64 TransientStorageSize;
    void *TransientStorage; // This should always be cleared to zero.

    // TODO(joe): Add function pointers when we need access to the platform specific services from the game.
    debug_load_bitmap *DEBUGLoadBitmap;
    debug_load_font_glyph *DEBUGLoadFontGlyph;
    debug_get_font_kern_advance_for *DEBUGGetFontKernAdvanceFor;

    render_target *RenderTarget;
    renderer_clear *RendererClear;
    renderer_draw_bitmap *RendererDrawBitmap;

    bool IsInitialized;
} game_memory;

typedef struct game_back_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
} game_back_buffer;
typedef struct game_sound_buffer
{
    s16 *Samples;
    int SampleCount;
    s16 ToneVolume;
    u16 SamplesPerSec;
} game_sound_buffer;

struct game_input;
#define UPDATE_GAME_AND_RENDER(name) void (name)(game_memory *Memory, game_back_buffer *BackBuffer, game_input *Input, r32 LastFrameTime)
typedef UPDATE_GAME_AND_RENDER(update_game_and_render);

#define GET_SOUND_SAMPLES(name) void (name)(game_sound_buffer *SoundBuffer, game_memory *Memory)
typedef GET_SOUND_SAMPLES(get_sound_samples);

#if __cplusplus
}
#endif

