#pragma once

/*
 * TODO(joe):
 *  - Scrolling background
 *  - Up/down sprites
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

//
// Entity
//
struct entity
{
    vector2 Size;

    vector2 P;
    vector2 dP; // Unit Per Second
};

struct game_state
{
    entity Ship;

    int ToneHz;
};

