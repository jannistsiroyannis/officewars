#ifndef TURNRESOLUTION_H
#define TURNRESOLUTION_H

#include "game.h"

void tickGame(struct GameState* game);

void stepGameHistory(struct GameState* game, unsigned targetStep);

void stepGameHistoryLatest(struct GameState* game);

#endif
