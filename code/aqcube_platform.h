#pragma once

#if __cplusplus
extern "C" {
#endif

#include <stdint.h>

// TODO(joe): Still trying to decide what kind of type names I like.
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float r32;

typedef int32_t s32;
typedef uint32_t u32;

typedef int64_t s64;
typedef uint64_t u64;

// TODO(joe): This constant should move into the math specific files.
#define PI32 3.14159265359f

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) ((Kilobytes(Value)) * 1024)
#define Gigabytes(Value) ((Megabytes(Value)) * 1024)

// Note(joe): These are services to the game provided by the platform layer.
typedef struct read_file_result
{
    void *Contents;
    int SizeInBytes;
} read_file_result;
read_file_result DEBUGWin32ReadFile(char *Filename);
bool DEBUGWin32WriteFile(char *Filename, void *Memory, uint64 FileSize);
void Win32FreeMemory(void *Memory);

// Note(joe): These are services to the platform layer provided by the game.
typedef struct game_memory
{
    uint64 PermanentStorageSize;
    void *PermanentStorage; // This should always be cleared to zero.
    uint64 TransientStorageSize;
    void *TransientStorage; // This should always be cleared to zero.

    // TODO(joe): Add function pointers when we need access to the platform specific services from the game.

    bool IsInitialized;
} game_memory;

typedef struct button_state
{
    bool IsDown;
} button_state;
typedef struct game_controller_input
{
    float dt;

    button_state Up;
    button_state Down;
    button_state Left;
    button_state Right;
} game_controller_input;
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
    int16 *Samples;
    int SampleCount;
    int16 ToneVolume;
    uint16 SamplesPerSec;
} game_sound_buffer;

#define UPDATE_GAME_AND_RENDER(name) void (name)(game_memory *Memory, game_back_buffer *BackBuffer, game_controller_input *Input)
typedef UPDATE_GAME_AND_RENDER(update_game_and_render);

#define GET_SOUND_SAMPLES(name) void (name)(game_sound_buffer *SoundBuffer, game_memory *Memory)
typedef GET_SOUND_SAMPLES(get_sound_samples);

#if __cplusplus
}
#endif

