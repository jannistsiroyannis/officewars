#include "lazy-client.h"
#include "../common/turnresolution.h"
#include <stdlib.h>
#include <string.h>

struct GameList *loadGameList(char *data, unsigned size) {
    struct GameList *gameList = malloc(sizeof(struct GameList));
    gameList->gameCount = 0;

    FILE* f = fmemopen((void*)data, size, "r");
    if (!f) return gameList;

    fscanf(f, "%u\n", &(gameList->gameCount));
    gameList->games = malloc(sizeof(struct GameState) * gameList->gameCount);
    for (unsigned i = 0; i < gameList->gameCount; ++i)
    {
        struct GameState game = deserialize(f);
        gameList->games[i] = game;
    }
    return gameList;
}

struct GameState *loadGame(char *data, unsigned size) {
    struct GameState *game = malloc(sizeof(struct GameState));
    game->playerCount = 0;
    game->nodeCount = 0;
    game->gameName = "";
    game->id[0] = '\0';

    FILE* f = fmemopen((void*)data, size, "r");
    if (!f) return game;

    *game = deserialize(f);
    return game;
}

unsigned getGameCount(struct GameList *gameList) {
    return gameList->gameCount;
}

struct GameState* getGame(struct GameList *gameList, unsigned index) {
    return &(gameList->games[index]);
}

char* getGameName(struct GameState *game) {
    return game->gameName;
}

char* getGameId(struct GameState *game) {
    return game->id;
}


unsigned getPlayerCount(struct GameState *game) {
    return game->playerCount;
}

char* getPlayerName(struct GameState *game, unsigned index) {
    return game->playerName[index];
}

char* getPlayerColor(struct GameState *game, unsigned index) {
    return game->playerColor[index];
}


unsigned getNodeCount(struct GameState *game) {
    return game->nodeCount;
}

unsigned getNodeConnected(struct GameState *game, unsigned a, unsigned b) {
    return game->adjacencyMatrix[game->nodeCount * a + b];
}

unsigned getNodeControlledBy(struct GameState *game, unsigned index) {
    return game->controlledBy[index];
}

float getNodeX(struct GameState *game, unsigned index) {
    return game->nodeSpacePositions[index].x;
}

float getNodeY(struct GameState *game, unsigned index) {
    return game->nodeSpacePositions[index].y;
}

float getNodeZ(struct GameState *game, unsigned index) {
    return game->nodeSpacePositions[index].z;
}


unsigned getTurnCount(struct GameState *game) {
    return game->turnCount;
}
