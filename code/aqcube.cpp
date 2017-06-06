#include "aqcube.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

//
// Rendering
//
struct render_target
{
    game_back_buffer *BackBuffer;
};
static void ClearBuffer(render_target RenderTarget, u8 R, u8 G, u8 B)
{
    game_back_buffer *BackBuffer = RenderTarget.BackBuffer;

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

static void DrawRectangle(render_target RenderTarget, int X, int Y, int Width, int Height, u8 R, u8 G, u8 B)
{
    game_back_buffer *BackBuffer = RenderTarget.BackBuffer;

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

static void DrawBitmap(render_target RenderTarget, s32 X, s32 Y, loaded_bitmap Bitmap)
{
    game_back_buffer *BackBuffer = RenderTarget.BackBuffer;

    X -= Bitmap.Width / 2;
    Y -= Bitmap.Height / 2;

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

enum render_group_entry_type
{
    Unknown = 0,
    RenderGroupEntryType_Clear,
    RenderGroupEntryType_DrawBitmap,
};
struct render_group_entry
{
    render_group_entry_type Type;
};
struct render_group_entry_clear
{
    render_group_entry_type Type;
    vector3 C;
};
struct render_group_entry_draw_bitmap
{
    render_group_entry_type Type;
    vector2 P;
    loaded_bitmap Bitmap;
};

struct render_group
{
    arena EntryArena;
    //matrix44 Transform;
    r32 PixelsPerMeter;

};
static render_group BeginRenderGroup(arena *Arena, u64 Size, r32 PixelsPerMeter)
{
    render_group Group = {};
    Group.EntryArena = SubArena(Arena, Size);
    Group.PixelsPerMeter = PixelsPerMeter;

    return Group;
}

static void PushClear(render_group *RenderGroup, vector3 C)
{
    render_group_entry_clear *Entry = PushStruct(&RenderGroup->EntryArena, render_group_entry_clear);
    Entry->Type = RenderGroupEntryType_Clear;
    Entry->C = C;
}
static void PushBitmap(render_group *RenderGroup, vector2 P, loaded_bitmap Bitmap)
{
    render_group_entry_draw_bitmap *Entry = PushStruct(&RenderGroup->EntryArena, render_group_entry_draw_bitmap);
    Entry->Type = RenderGroupEntryType_DrawBitmap;
    Entry->P = P;
    Entry->Bitmap = Bitmap;
}


static void RenderGroupToTarget(render_target RenderTarget, render_group *RenderGroup)
{
    u64 EntryBaseAddress = RenderGroup->EntryArena.BaseAddress;
    u64 EntryOffset = 0;
    while (EntryOffset < RenderGroup->EntryArena.Size)
    {
        render_group_entry *Entry = (render_group_entry *)(EntryBaseAddress + EntryOffset);
        switch(Entry->Type)
        {
            case RenderGroupEntryType_Clear:
            {
                render_group_entry_clear *ClearEntry = (render_group_entry_clear *)Entry;
                ClearBuffer(RenderTarget, (u8)ClearEntry->C.R, (u8)ClearEntry->C.G, (u8)ClearEntry->C.B);

                EntryOffset += sizeof(render_group_entry_clear);
            } break;
            case RenderGroupEntryType_DrawBitmap:
            {
                render_group_entry_draw_bitmap *DrawBitmapEntry = (render_group_entry_draw_bitmap *)Entry;
                s32 X = (s32)(DrawBitmapEntry->P.X * RenderGroup->PixelsPerMeter);
                s32 Y = (s32)(DrawBitmapEntry->P.Y * RenderGroup->PixelsPerMeter);
                DrawBitmap(RenderTarget, X, Y, DrawBitmapEntry->Bitmap);

                EntryOffset += sizeof(render_group_entry_draw_bitmap);
            } break;
            default:
            {
                Assert(!"Render Group Entry Not Supported");
            }
        }
    }
}

static void Render(render_target RenderTarget, game_state *GameState, r32 LastFrameTime)
{
    AtX = 10;
    AtY = 10;
   
    render_group RenderGroup = BeginRenderGroup(&GameState->TransArena, Megabytes(16), GameState->PixelsPerMeter);
    PushClear(&RenderGroup, V3(0, 43, 54));

    entity Ship = GameState->Ship;
    PushBitmap(&RenderGroup, Ship.P, GameState->ShipBitmap);

    RenderGroupToTarget(RenderTarget, &RenderGroup);

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

    render_target RenderTarget = {};
    RenderTarget.BackBuffer= BackBuffer;
    Render(RenderTarget, GameState, LastFrameTime);
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
