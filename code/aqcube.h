#pragma once

/*
 * TODO(joe):
 *  - I haven't decided how I want to break up the different layers yet.  I
 *    don't really like the C style of prefixing every function name with the
 *    layer that it belongs to (e.g. PlaformReadFile). I kind of prefer moving
 *    the layer name into a namespace instead (e.g. Plaform::ReadFile). It does
 *    require that the caller type two extra characters though. I think I like it
 *    for functions but not necessarily for types defined in the platform. I
 *    wouldn't like having to type Game::game_state, for example. Although
 *    game::state could be interesting...
 */

#include "aqcube_platform.h"

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) ((Kilobytes(Value)) * 1024)
#define Gigabytes(Value) ((Megabytes(Value)) * 1024)

// Note(joe): These are service to the game provided by the platform layer.
struct read_file_result
{
    void *Contents;
    int SizeInBytes;
};
read_file_result DEBUGWin32ReadFile(char *Filename);
bool DEBUGWin32WriteFile(char *Filename, void *Memory, uint64 FileSize);
void Win32FreeMemory(void *Memory);

// Note(joe): These are service to the platform layer provided by the game.
struct game_memory
{
    uint64 PermanentStorageSize;
    void *PermanentStorage; // This should always be cleared to zero.
    uint64 TransientStorageSize;
    void *TransientStorage; // This should always be cleared to zero.

    bool IsInitialized;
};

struct game_state
{
    int OffsetX;
    int OffsetY;

    int ToneHz;
};

struct button_state
{
    bool IsDown;
};
struct game_controller_input
{
    button_state Up;
    button_state Down;
    button_state Left;
    button_state Right;
};
struct game_back_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};
struct game_sound_buffer
{
    int16 *Samples;
    int SampleCount;
    int16 ToneVolume;
    int16 SamplesPerSec;
};
void UpdateGameAndRender(game_memory *Memory, game_back_buffer *BackBuffer, game_sound_buffer *SoundBuffer, game_controller_input *Input);
void GetSoundSamples(game_sound_buffer *SoundBuffer, game_state* GameState);
