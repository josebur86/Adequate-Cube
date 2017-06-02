#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4456)
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"
#pragma warning(pop)

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

//
// Font
//
struct font_data
{
    bool IsLoaded;
    stbtt_fontinfo FontInfo;

    r32 Scale;
    s32 Ascent;  // NOTE(joe): Coordinate above the baseline the font extends.
    s32 Descent; // NOTE(joe): Coorindate below the baseline the font exenteds (Typically negative). 
    s32 LineGap; // NOTE(joe): Spacing between one row's descent and the next row's ascent.
};
static font_data GlobalDebugFont;


static loaded_bitmap DEBUGLoadBitmap(arena *Arena, char *FileName)
{
    loaded_bitmap Result = {};

    s32 X, Y, N;
    u8 *Pixels = stbi_load(FileName, &X, &Y, &N, 0);
    if (Pixels)
    {
        Result.IsValid = true;
        Result.Width = X;
        Result.Height = Y;
        Result.Pitch = X*N;
        s32 BufferSize = X*Y*N;

        Result.Pixels = (u8 *)PushSize(Arena, BufferSize); 

        u8 *SourceRow = Pixels;
        u8 *DestRow = Result.Pixels;
        for (s32 RowIndex = 0; RowIndex < (s32)Y; ++RowIndex)
        {
            u32 *SourcePixel = (u32 *)SourceRow;
            u32 *DestPixel = (u32 *)DestRow;
            for (s32 ColIndex = 0; ColIndex < (s32)X; ++ColIndex)
            {
                u8 R = (u8)(*SourcePixel >> 0  & 0xFF);
                u8 G = (u8)(*SourcePixel >> 8  & 0xFF);
                u8 B = (u8)(*SourcePixel >> 16 & 0xFF);
                u8 A = (u8)(*SourcePixel >> 24 & 0xFF);

                u32 Pixel = (A << 24) | (R << 16) | (G << 8) | (B << 0);
                *DestPixel = Pixel;

                ++SourcePixel;
                ++DestPixel;
            }

            SourceRow += Result.Pitch;
            DestRow += Result.Pitch;
        }

        stbi_image_free(Pixels);
    }

    return Result;
}

static font_glyph DEBUGLoadFontGlyph(arena *Arena, char C)
{
    Assert(GlobalDebugFont.IsLoaded);

    font_glyph Result = {};

    Result.IsLoaded = true;
    Result.Glyph.IsValid = true;

    if (C != ' ')
    {
        s32 FontWidth;
        s32 FontHeight;
        u8 *Bitmap = stbtt_GetCodepointBitmap(&GlobalDebugFont.FontInfo, 
                0, 
                GlobalDebugFont.Scale,
                C, 
                &FontWidth, 
                &FontHeight, 0, 0);

        Result.Glyph.Width = FontWidth;
        Result.Glyph.Height = FontHeight;
        Result.Glyph.Pitch = FontWidth*4;
    
        u32 BufferSize = (u32)FontWidth*FontHeight*4;
        Result.Glyph.Pixels = (u8 *)PushSize(Arena, BufferSize);

        Assert(Bitmap);
        u8 *Source = Bitmap;
        u32 *Pixel = (u32 *)Result.Glyph.Pixels;
        for (u32 Index = 0; Index < BufferSize; ++Index)
        {
            u8 R = *Source;
            u8 G = *Source;
            u8 B = *Source;
            u8 A = *Source;
            *Pixel = (A << 24) | (R << 16) | (G << 8) | (B << 0);

            ++Source;
            ++Pixel;
        }

        stbtt_FreeBitmap(Bitmap, 0);
    }


    s32 X0;
    s32 X1;
    s32 Y0;
    s32 Y1;
    stbtt_GetCodepointBitmapBox(&GlobalDebugFont.FontInfo,
                                C,
                                GlobalDebugFont.Scale, GlobalDebugFont.Scale,
                                &X0, &Y0, &X1, &Y1);

    s32 AdvanceWidth;
    s32 LeftSideBearing;
    stbtt_GetCodepointHMetrics(&GlobalDebugFont.FontInfo, C, &AdvanceWidth, &LeftSideBearing);

    Result.Baseline = (s32)(GlobalDebugFont.Scale * GlobalDebugFont.Ascent);
    Result.Top = Y0;
    Result.Bottom = Y1;
    Result.AdvanceWidth = (s32)(GlobalDebugFont.Scale * AdvanceWidth);
    Result.ToLeftEdge = (s32)(GlobalDebugFont.Scale * LeftSideBearing);
    
    return Result;
};

static s32 DEBUGGetFontKernAdvanceFor(char A, char B)
{
    s32 Result = (s32)(GlobalDebugFont.Scale * stbtt_GetCodepointKernAdvance(&GlobalDebugFont.FontInfo, A, B));
    return Result;
}
