#include "aqcube.h"

#include <assert.h>
#include <math.h>

//
// Rendering
//

static void ClearBuffer(game_back_buffer *BackBuffer, uint8 R, uint8 G, uint8 B)
{
    int8 *Row = (int8 *)BackBuffer->Memory;
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        int32 *Pixel = (int32 *)Row;
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            *Pixel++ = (R << 16) | (G << 8) | (B << 0);
        }

        Row += BackBuffer->Pitch;
    }
}

static void DrawRectangle(game_back_buffer *BackBuffer, int X, int Y, int Width, int Height, uint8 R, uint8 G, uint8 B)
{
    for (int YIndex = 0; YIndex < Height; ++YIndex)
    {
        uint32 *Pixel = (uint32 *)BackBuffer->Memory + ((Y + YIndex) * BackBuffer->Width) + X;
        for (int XIndex = 0; XIndex < Width; ++XIndex)
        {
            if (X + XIndex < BackBuffer->Width && X + XIndex >= 0 && Y + YIndex < BackBuffer->Height && Y + YIndex >= 0)
            {
                *Pixel = (R << 16) | (G << 8) | (B << 0);
            }
            ++Pixel;
        }
    }
}

static void Render(game_back_buffer *BackBuffer, game_state *GameState)
{
    ClearBuffer(BackBuffer, 0, 43, 54);

    entity Ship = GameState->Ship;
    DrawRectangle(BackBuffer, (int)Ship.P.X, (int)Ship.P.Y, (int)Ship.Size.X, (int)Ship.Size.Y, 181, 137, 0);
}

extern "C" UPDATE_GAME_AND_RENDER(UpdateGameAndRender)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->Ship.P.X    = (float)BackBuffer->Width / 2;
        GameState->Ship.P.Y    = (float)BackBuffer->Height / 2;
        GameState->Ship.Size.X = 80.0f;
        GameState->Ship.Size.Y = 40.0f;

        GameState->ToneHz = 256;

        Memory->IsInitialized = true;
    }

    entity *Ship = &GameState->Ship;

    vector2 ddP = { 0.0f, 0.0f };

    if (Input->Down.IsDown)
    {
        ddP.Y = 1.0f;
    }
    if (Input->Up.IsDown)
    {
        ddP.Y = -1.0f;
    }
    if (Input->Right.IsDown)
    {
        ddP.X = 1.0f;
    }
    if (Input->Left.IsDown)
    {
        ddP.X = -1.0f;
    }
    if ((Input->Down.IsDown || Input->Up.IsDown) && (Input->Right.IsDown || Input->Left.IsDown))
    {
        ddP = ddP * 0.707f;
    }

    float ShipSpeed = 2000.0f; // pixels / sec^2
    ddP             = ddP * ShipSpeed; // TODO(joe): Overload *= for vectors
    ddP             = -5.0f * Ship->dP + ddP;

    Ship->P  = (0.5f * ddP * Square(Input->dt)) + (Ship->dP * Input->dt) + Ship->P;
    Ship->dP = (ddP * Input->dt) + Ship->dP;

    if (Ship->P.X < 0.0)
    {
        Ship->P.X  = 0.0f;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y < 0.0)
    {
        Ship->P.Y  = 0.0f;
        Ship->dP.Y = 0.0f;
    }
    if (Ship->P.X + Ship->Size.X >= BackBuffer->Width)
    {
        Ship->P.X  = BackBuffer->Width - Ship->Size.X - 1.0f;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y + Ship->Size.Y >= BackBuffer->Height)
    {
        Ship->P.Y  = BackBuffer->Height - Ship->Size.Y - 1.0f;
        Ship->dP.Y = 0.0f;
    }

    Render(BackBuffer, GameState);
}

// TODO(joe): This function will need to be performant.
extern "C" GET_SOUND_SAMPLES(GetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    // TODO(joe): This is pretty hacky but the real game won't be generating sounds
    // using sine anyway. Remove this when we can actually play sound files.
    static float tSine = 0.0f;

    float WavePeriod = (float)SoundBuffer->SamplesPerSec / GameState->ToneHz;

    int16 *Sample = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue  = sinf(tSine);
        uint16 ToneValue = (uint16)(SoundBuffer->ToneVolume * SineValue);

        *Sample++ = ToneValue; // Left
        *Sample++ = ToneValue; // Right

        tSine += 2 * PI32 * 1.0f / WavePeriod;
    }
}
