#pragma once

// TODO(joe): These are service to the game provided by the platform layer.
// ex. Opening a file.

// Note(joe): These are service to the platform layer provided by the game.
struct game_state
{
    int OffsetX;
    int OffsetY;

    int ToneHz;
};

struct game_back_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};
void UpdateGameAndRender(game_back_buffer *BackBuffer, game_state *GameState);

struct sound_buffer
{
    int16 *Samples;
    int SampleCount;
    int16 ToneVolume; 
    int WavePeriod;
};
void GetSoundSamples(sound_buffer *SoundBuffer);
