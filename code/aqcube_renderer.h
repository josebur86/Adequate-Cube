#pragma once

#include "aqcube_arena.h"

struct loaded_bitmap
{
    bool IsValid;

    u32 Width;
    u32 Height;
    u32 Pitch;
    u8 *Pixels;
};

enum render_target_type
{
    RenderTarget_Unknown = 0,
    RenderTarget_Software,
    RenderTarget_OpenGL,
};
struct render_target
{
    render_target_type Type;
};

// NOTE(joe): Available Rendering Operations
#define RENDERER_CLEAR(name) void (name)(render_target *Target, vector3 C)
typedef RENDERER_CLEAR(renderer_clear);
#define RENDERER_DRAW_BITMAP(name) void (name)(render_target *Target, vector2 P, loaded_bitmap Bitmap)
typedef RENDERER_DRAW_BITMAP(renderer_draw_bitmap);

struct renderer
{
    render_target *Target;

    renderer_clear *Clear;
    renderer_draw_bitmap *DrawBitmap;
};

enum render_group_entry_type
{
    RenderGroupEntryType_Unknown = 0,
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


static void RenderGroupToTarget(renderer Renderer, render_group *RenderGroup)
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
                Renderer.Clear(Renderer.Target, ClearEntry->C);

                EntryOffset += sizeof(render_group_entry_clear);
            } break;
            case RenderGroupEntryType_DrawBitmap:
            {
                render_group_entry_draw_bitmap *DrawBitmapEntry = (render_group_entry_draw_bitmap *)Entry;
                vector2 P = DrawBitmapEntry->P * RenderGroup->PixelsPerMeter;
                Renderer.DrawBitmap(Renderer.Target, P, DrawBitmapEntry->Bitmap);

                EntryOffset += sizeof(render_group_entry_draw_bitmap);
            } break;
            case RenderGroupEntryType_Unknown:
            default:
            {
                Assert(!"Unknown Render Group Entry");
            }
        }
    }
}
