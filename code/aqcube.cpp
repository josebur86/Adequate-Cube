#include "aqcube.h"

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

