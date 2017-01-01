#pragma once

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

#include "aqcube_platform.h"

