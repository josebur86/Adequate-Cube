#include "aqcube.h"

#include <assert.h>
#include <math.h>

//
// Rendering
//

static void ClearBuffer(game_back_buffer *BackBuffer, u8 R, u8 G, u8 B)
{
    s8 *Row = (s8 *)BackBuffer->Memory;
    for (int YIndex = 0; YIndex < BackBuffer->Height; ++YIndex)
    {
        s32 *Pixel = (s32 *)Row;
        for (int XIndex = 0; XIndex < BackBuffer->Width; ++XIndex)
        {
            *Pixel++ = (R << 16) | (G << 8) | (B << 0);
        }

        Row += BackBuffer->Pitch;
    }
}

static void DrawRectangle(game_back_buffer *BackBuffer, int X, int Y, int Width, int Height, u8 R, u8 G, u8 B)
{
    for (int YIndex = 0; YIndex < Height; ++YIndex)
    {
        u32 *Pixel = (u32 *)BackBuffer->Memory + ((Y + YIndex) * BackBuffer->Width) + X;
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

static void DrawBitmap(game_back_buffer *BackBuffer, s32 X, s32 Y, loaded_bitmap Bitmap)
{
    u8 *SourceRow = Bitmap.Pixels;
    u8 *DestRow = (u8 *)((u32 *)BackBuffer->Memory + (Y * BackBuffer->Width) + X);

    for (s32 RowIndex = 0; RowIndex < (s32)Bitmap.Height; ++RowIndex)
    {
        u32 *SourcePixel = (u32 *)SourceRow;
        u32 *DestPixel = (u32 *)DestRow;

        for (s32 ColIndex = 0; ColIndex < (s32)Bitmap.Width; ++ColIndex)
        {
            if (X + ColIndex < BackBuffer->Width && X + ColIndex >= 0 && 
                Y + RowIndex < BackBuffer->Height && Y + RowIndex >= 0)
            {
                r32 SA = ((r32)((*SourcePixel >> 24) & 0xFF) / 255.0f);
                r32 DA = 1.0f - SA;

                u8 SR = ((*SourcePixel >> 0)  & 0xFF);
                u8 SG = ((*SourcePixel >> 8)  & 0xFF);
                u8 SB = ((*SourcePixel >> 16) & 0xFF);

                u8 DR = ((*DestPixel >> 0)  & 0xFF);
                u8 DG = ((*DestPixel >> 8)  & 0xFF);
                u8 DB = ((*DestPixel >> 16) & 0xFF);

                u32 C = (u8(DA * DR + SA * SR) << 0) | 
                        (u8(DA * DG + SA * SG) << 8) |
                        (u8(DA * DB + SA * SB) << 16);

                *DestPixel = C;
            }

            ++DestPixel;
            ++SourcePixel;
        }

        DestRow += BackBuffer->Pitch;
        SourceRow += Bitmap.Pitch;
    }
}

static font_glyph * GetFontGlyphFor(game_state *GameState, char C)
{
    font_glyph *Glyph = GameState->FontGlyphs + (C - 32);
    if (!Glyph->IsLoaded)
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
        if (*Text != ' ')
        {
            font_glyph *Glyph = GetFontGlyphFor(GameState, *Text);
            Assert(Glyph->IsLoaded);

            u32 Y = AtY + Glyph->Baseline + Glyph->Top;
            u32 X = AtX + Glyph->ToLeftEdge;
            DrawBitmap(BackBuffer, AtX, Y, Glyph->Glyph);
            AtX += Glyph->AdvanceWidth;
            if (*(Text + 1) != '\0')
            {
                AtX += GetFontKernAdvanceFor(GameState, *Text, *(Text+1));
            }
        }
        else
        {
            // TODO(joe): Should we still produce a glyph just without a bitmap so that we can get
            // the the glyph width information?
            AtX += 30;
        }

        ++Text;
    }

    AtX = 10;
    AtY += 80; // TODO(joe): Proper line advance.
}

static void Render(game_back_buffer *BackBuffer, game_state *GameState)
{
    AtX = 10;
    AtY = 10;
   
    ClearBuffer(BackBuffer, 0, 43, 54);

    entity Ship = GameState->Ship;
    DrawBitmap(BackBuffer, (s32)Ship.P.X, (s32)Ship.P.Y, GameState->ShipBitmap);

    DEBUGDrawTextLine(BackBuffer, GameState, "AT AV AW AY Av Aw Ay");
    DEBUGDrawTextLine(BackBuffer, GameState, "Fa Fe Fo Kv Kw Ky LO");
    DEBUGDrawTextLine(BackBuffer, GameState, "LV LY PA Pa Pe Po TA");
    DEBUGDrawTextLine(BackBuffer, GameState, "Ta Te Ti To Tr Ts Tu Ty");
    DEBUGDrawTextLine(BackBuffer, GameState, "UA VA Va Ve Vo Vr Vu Vy");
    DEBUGDrawTextLine(BackBuffer, GameState, "WA WO Wa We Wr Wv Wy");
}

extern "C" UPDATE_GAME_AND_RENDER(UpdateGameAndRender)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->Platform.DEBUGLoadBitmap = Memory->DEBUGLoadBitmap;
        GameState->Platform.DEBUGLoadFontGlyph = Memory->DEBUGLoadFontGlyph;
        GameState->Platform.DEBUGGetFontKernAdvanceFor = Memory->DEBUGGetFontKernAdvanceFor;

        GameState->Arena = InitializeArena((u64)((u8 *)Memory->PermanentStorage + sizeof(*GameState)), Gigabytes(1));

        GameState->Ship.P.X = (float)BackBuffer->Width / 8;
        GameState->Ship.P.Y = (float)BackBuffer->Height / 2;
        GameState->Ship.Size.X = 80.0f;
        GameState->Ship.Size.Y = 40.0f;

        GameState->ShipBitmap = GameState->Platform.DEBUGLoadBitmap(&GameState->Arena, "Ship-1.png");

        GameState->ToneHz = 256;

        Memory->IsInitialized = true;
    }

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

    float ShipSpeed = 2000.0f; // pixels / sec^2
    ddP = ddP * ShipSpeed; // TODO(joe): Overload *= for vectors
    ddP = -5.0f * Ship->dP + ddP;

    Ship->P = (0.5f * ddP * Square(Input->dt)) + (Ship->dP * Input->dt) + Ship->P;
    Ship->dP = (ddP * Input->dt) + Ship->dP;

    if (Ship->P.X < 0.0)
    {
        Ship->P.X = 0.0f;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y < 0.0)
    {
        Ship->P.Y = 0.0f;
        Ship->dP.Y = 0.0f;
    }
    if (Ship->P.X + Ship->Size.X >= BackBuffer->Width)
    {
        Ship->P.X = BackBuffer->Width - Ship->Size.X - 1.0f;
        Ship->dP.X = 0.0f;
    }
    if (Ship->P.Y + Ship->Size.Y >= BackBuffer->Height)
    {
        Ship->P.Y = BackBuffer->Height - Ship->Size.Y - 1.0f;
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
