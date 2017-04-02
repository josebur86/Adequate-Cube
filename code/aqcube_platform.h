#pragma once

#if __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef float r32;

typedef int8_t s8;
typedef uint8_t u8;

typedef int16_t s16;
typedef uint16_t u16;

typedef int32_t s32;
typedef uint32_t u32;

typedef int64_t s64;
typedef uint64_t u64;

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) ((Kilobytes(Value)) * 1024)
#define Gigabytes(Value) ((Megabytes(Value)) * 1024)

#define Assert(Condition) if (!(Condition)) { *(int *)0 = 0; }
#define ArrayCount(Array) sizeof((Array)) / sizeof((Array[0]))

//
// Arena
//
struct arena
{
    u64 BaseAddress;
    u64 Size;
    u64 MaxSize;
};

arena InitializeArena(u64 BaseAddress, u64 MaxSize)
{
    arena Result = {};
    Result.BaseAddress = BaseAddress;
    Result.MaxSize = MaxSize;

    return Result;
}

void *PushSize(arena *Arena, u64 Size)
{
    Assert(Arena->Size + Size <= Arena->MaxSize);

    void *Result = (void *)(Arena->BaseAddress + Arena->Size);
    
    Arena->Size += Size;

    return Result;
}

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

typedef struct loaded_bitmap
{
    bool IsValid;

    u32 Width;
    u32 Height;
    u32 Pitch;
    u8 *Pixels;
} loaded_bitmap;
typedef loaded_bitmap debug_load_bitmap(arena *Arena, char *Filename);

struct font_glyph
{
    bool IsLoaded;
    loaded_bitmap Glyph;
};
typedef loaded_bitmap debug_load_font_glyph(arena *Arena, char C);

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
#define UPDATE_GAME_AND_RENDER(name) void (name)(game_memory *Memory, game_back_buffer *BackBuffer, game_input *Input)
typedef UPDATE_GAME_AND_RENDER(update_game_and_render);

#define GET_SOUND_SAMPLES(name) void (name)(game_sound_buffer *SoundBuffer, game_memory *Memory)
typedef GET_SOUND_SAMPLES(get_sound_samples);

#if __cplusplus
}
#endif

