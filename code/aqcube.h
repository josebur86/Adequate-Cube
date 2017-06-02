#pragma once

/*
 * TODO(joe):
 *  - Scrolling background
 *  - Up/down ship sprites
 *
 *  - Power-Ups
 *      * Thing that follows your movements but can also shoot (familiar).
 *  - Enemies
 *      * Enter from both sides of the screen.
 *      * Can be on the ground
 */

/*
 * TODO(joe):
 *  - I haven't decided how I want to break up the different layers yet.  I
 *    don't really like the C style of prefixing every function name with the
 *    layer that it belongs to (e.g. PlaformReadFile). I kind of prefer moving
 *    the layer name into a namespace instead (e.g. Plaform::ReadFile). It does
 *    require that the caller type two extra characters though. I think I like it
 *    for functions but not necessarily for types defined in the platform. I
 *    wouldn't like having to type Game::game_state, for example. Although
 *    game::state could be interesting...
 */

#include "aqcube_math.h"
#include "aqcube_platform.h"

struct platform
{
    debug_load_bitmap *DEBUGLoadBitmap;
    debug_load_font_glyph *DEBUGLoadFontGlyph;
    debug_get_font_kern_advance_for *DEBUGGetFontKernAdvanceFor;
};

//
// Entity
//
struct entity
{
    vector2 Size;

    vector2 P;
    vector2 dP; // Meters Per Second
};

struct world
{
    rect Bounds;
};

struct coordinate_system
{
    vector2 Origin;
    vector2 XAxis;
    vector2 YAxis;
    vector4 Color;
};

struct game_state
{
    arena Arena;

    world World;
    entity Ship;
    loaded_bitmap ShipBitmap;
    r32 PixelsPerMeter;

    font_glyph FontGlyphs[('~' - '!') + 1];

    coordinate_system TestCoord;
    r32 Time;

    int ToneHz;

    platform Platform;
};

// TODO(joe): Include the transition count?
struct controller_button_state
{
    bool IsDown;
};

struct game_controller
{
    bool IsConnected;
    bool IsAnalog;

    controller_button_state Up;
    controller_button_state Down;
    controller_button_state Left;
    controller_button_state Right;

    r32 XAxis;
    r32 YAxis;
};

struct game_input
{
    float dt;

    game_controller Controllers[2];
};

