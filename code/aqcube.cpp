#include "aqcube.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static font_glyph * GetFontGlyphFor(game_state *GameState, char C)
{
    font_glyph *Glyph = GameState->FontGlyphs + (C - 32);
    if (!Glyph->IsLoaded && GameState->Platform.DEBUGLoadFontGlyph)
    {
        *Glyph = GameState->Platform.DEBUGLoadFontGlyph(&GameState->Arena, C);
        Assert(Glyph->Glyph.IsValid);
        Glyph->IsLoaded = true;
    }

    return Glyph;
}
static s32 GetFontKernAdvanceFor(game_state *GameState, char A, char B)
{
    // TODO(joe): I would rather access the kerning table directly than have
    // to ask the platform layer for it, but this is debug code so...
    s32 Result = GameState->Platform.DEBUGGetFontKernAdvanceFor(A , B);
    return Result;
}

static u32 AtX = 10;
static u32 AtY = 10;

// TODO(joe): Multiple Fonts?.
static void DEBUGDrawTextLine(game_back_buffer *BackBuffer, game_state *GameState, char *Text)
{
    while(Text && *Text != '\0')
    {
        font_glyph *Glyph = GetFontGlyphFor(GameState, *Text);
        Assert(Glyph->IsLoaded);

        u32 Y = AtY + Glyph->Baseline + Glyph->Top;
        u32 X = AtX + Glyph->ToLeftEdge;
        //DrawBitmap(BackBuffer, X + (Glyph->Glyph.Width / 2), Y + (Glyph->Glyph.Height / 2), Glyph->Glyph);
        AtX += Glyph->AdvanceWidth;
#if 1
        if ((Text + 1) && *(Text + 1) != '\0')
        {
            AtX += GetFontKernAdvanceFor(GameState, *Text, *(Text+1));
        }
#endif

        ++Text;
    }

    AtX = 10;
    AtY += 80; // TODO(joe): Proper line advance.
}

static void Render(renderer Renderer, game_state *GameState, r32 LastFrameTime)
{
    AtX = 10;
    AtY = 10;
   
    render_group RenderGroup = BeginRenderGroup(&GameState->TransArena, Megabytes(16), GameState->PixelsPerMeter);
    PushClear(&RenderGroup, V3(0, 43, 54));

    entity Ship = GameState->Ship;
    PushBitmap(&RenderGroup, Ship.P, GameState->ShipBitmap);

    RenderGroupToTarget(Renderer, &RenderGroup);

#if 0
    char Buffer[256];
    sprintf_s(Buffer, "Last Frame Time: %.2fms", LastFrameTime);
    DEBUGDrawTextLine(BackBuffer, GameState, Buffer);
#endif
}

extern "C" UPDATE_GAME_AND_RENDER(UpdateGameAndRender)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->Platform.DEBUGLoadBitmap = Memory->DEBUGLoadBitmap;
        GameState->Platform.DEBUGLoadFontGlyph = Memory->DEBUGLoadFontGlyph;
        GameState->Platform.DEBUGGetFontKernAdvanceFor = Memory->DEBUGGetFontKernAdvanceFor;

        GameState->Renderer.Target = Memory->RenderTarget;
        GameState->Renderer.Clear = Memory->RendererClear;
        GameState->Renderer.DrawBitmap = Memory->RendererDrawBitmap;

        GameState->Arena = InitializeArena((u64)((u8 *)Memory->PermanentStorage + sizeof(*GameState)), Gigabytes(1));
        GameState->TransArena = InitializeArena((u64)Memory->TransientStorage, Megabytes(256));

        GameState->PixelsPerMeter = 6.54f;
        r32 MetersPerPixel = 1.0f / GameState->PixelsPerMeter;
        GameState->World.Bounds = Rect(V2(0.0f, 0.0f), 
                                       V2((float)BackBuffer->Width * MetersPerPixel, 
                                          (float)BackBuffer->Height * MetersPerPixel));

        GameState->Ship.P.X = Width(GameState->World.Bounds) / 8.0f;
        GameState->Ship.P.Y = Height(GameState->World.Bounds) / 2.0f;
        GameState->Ship.Size.X = 15.24f;
        GameState->Ship.Size.Y = 4.87f;

        if (GameState->Platform.DEBUGLoadBitmap)
        {
            GameState->ShipBitmap = GameState->Platform.DEBUGLoadBitmap(&GameState->Arena, "Ship-1.png");
        }
        else
        {
            Assert(!"Platform cannot load bitmaps!");
        }

        GameState->ToneHz = 256;
        
        Memory->IsInitialized = true;
    }
    ClearArena(&GameState->TransArena);

    world *World = &GameState->World;
    entity *Ship = &GameState->Ship;

    vector2 ddP = { 0.0f, 0.0f };

    for (u32 ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        game_controller *Controller = Input->Controllers + ControllerIndex;
        if (Controller->IsConnected)
        {
            if (Controller->IsAnalog)
            {
                ddP += V2(Controller->XAxis, Controller->YAxis);
            }
            else
            {
                if (Controller->Down.IsDown)
                {
                    ddP.Y = 1.0f;
                }
                if (Controller->Up.IsDown)
                {
                    ddP.Y = -1.0f;
                }
                if (Controller->Right.IsDown)
                {
                    ddP.X = 1.0f;
                }
                if (Controller->Left.IsDown)
                {
                    ddP.X = -1.0f;
                }
                if ((Controller->Down.IsDown || Controller->Up.IsDown)
                    && (Controller->Right.IsDown || Controller->Left.IsDown))
                {
                    ddP = ddP * 0.707f;
                }
            }
        }
    }

    float ShipAccel = 600.0f; // meters / sec^2
    ddP *= ShipAccel; 
    ddP = -7.0f * Ship->dP + ddP;

    Ship->P = (0.5f * ddP * Square(Input->dt)) + (Ship->dP * Input->dt) + Ship->P;
    Ship->dP = (ddP * Input->dt) + Ship->dP;

    if (Ship->P.X < World->Bounds.Left)
    {
        Ship->P.X = 0.0f;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y < World->Bounds.Top)
    {
        Ship->P.Y = 0.0f;
        Ship->dP.Y = 0.0f;
    }
    if (Ship->P.X >= World->Bounds.Right)
    {
        Ship->P.X = World->Bounds.Right;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y >= World->Bounds.Bottom)
    {
        Ship->P.Y = World->Bounds.Bottom;
        Ship->dP.Y = 0.0f;
    }
    
    rect ScreenRect = Rect(V2(0, 0), V2(BackBuffer->Width, BackBuffer->Height));
    GameState->Time += 100.0f * Input->dt;
    GameState->TestCoord.Origin = Center(ScreenRect);
    GameState->TestCoord.XAxis = 100.0f * V2(Cos(GameState->Time), Sin(GameState->Time));
    GameState->TestCoord.YAxis = 2*V2(-GameState->TestCoord.XAxis.Y, GameState->TestCoord.XAxis.X);
    GameState->TestCoord.Color= V4(1, 1, 0, 1);

    Render(GameState->Renderer, GameState, LastFrameTime);
}

// TODO(joe): This function will need to be performant.
extern "C" GET_SOUND_SAMPLES(GetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    // TODO(joe): This is pretty hacky but the real game won't be generating sounds
    // using sine anyway. Remove this when we can actually play sound files.
    static float tSine = 0.0f;

    float WavePeriod = (float)SoundBuffer->SamplesPerSec / GameState->ToneHz;

    s16 *Sample = SoundBuffer->Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        u16 ToneValue = (u16)(SoundBuffer->ToneVolume * SineValue);

        *Sample++ = ToneValue; // Left
        *Sample++ = ToneValue; // Right

        tSine += 2 * PI32 * 1.0f / WavePeriod;
    }
}
