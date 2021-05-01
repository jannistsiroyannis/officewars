#ifndef LAZY_CLIENT_H

#include "../common/game.h"
#include "../common/math/frame.h"
#include "../common/math/vec.h"

struct GameList
{
    unsigned gameCount;
    struct GameState *games;
};

#endif // LAZY_CLIENT_H