#include "aqcube_renderer.h"

struct software_render_target
{
    render_target_type Type;

    game_back_buffer *BackBuffer;
};

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

RENDERER_CLEAR(SoftwareRendererClear)
{
    Assert(Target->Type == RenderTarget_Software);
    software_render_target *SoftwareTarget = (software_render_target *)Target;

    u8 R = (u8)C.R;
    u8 G = (u8)C.G;
    u8 B = (u8)C.B;
    game_back_buffer *BackBuffer = SoftwareTarget->BackBuffer;
    
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

RENDERER_DRAW_BITMAP(SoftwareRendererDrawBitmap)
{
    Assert(Target->Type == RenderTarget_Software);
    software_render_target *SoftwareTarget = (software_render_target *)Target;

    s32 X = (s32)P.X;
    s32 Y = (s32)P.Y;
    game_back_buffer *BackBuffer = SoftwareTarget->BackBuffer;
    
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
