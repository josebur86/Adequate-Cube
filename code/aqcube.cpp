#include "aqcube.h"

#include <cmath>

static void Render(game_back_buffer *BackBuffer, game_state *GameState)
{
    int32 *Pixel = (int32 *)BackBuffer->Memory;
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            uint8 b = GameState->OffsetX + XIndex;
            uint8 g = GameState->OffsetY + YIndex;
            uint8 r = GameState->OffsetX + XIndex + GameState->OffsetY + YIndex;
            *Pixel++ = (b << 0 | g << 8 | r << 16);
        }
    }
}

void UpdateGameAndRender(game_back_buffer *BackBuffer, game_state *GameState)
{
    Render(BackBuffer, GameState);
}

void GetSoundSamples(sound_buffer *SoundBuffer)
{
    // TODO(joe): This is pretty hacky but the real game won't be generating sounds
    // using sine anyway. Remove this when we can actually play sound files.
    static float tSine = 0.0f;

    int16 *Sample = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        uint16 ToneValue = (uint16)(SoundBuffer->ToneVolume*SineValue);

        *Sample++ = ToneValue; // Left
        *Sample++ = ToneValue; // Right

        tSine += 2*PI32 * 1.0f/(float)SoundBuffer->WavePeriod;
    }
}
