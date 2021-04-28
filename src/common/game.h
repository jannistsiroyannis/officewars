#ifndef GAME_H
#define GAME_H

#include <stdio.h>

#include "../common/math/vec.h"

enum MetaGameState
{
   PREGAME,
   INGAME,
   POSTGAME,
};

enum OrderType
{
   ATTACKORDER = 0,
   SUPPORTORDER = 1,
   SURRENDERORDER = 2,
};

struct Turn
{
   unsigned orderCount;
   
   unsigned* issuingPlayer;
   unsigned* fromNode;
   unsigned* toNode;
   unsigned* type;
};

struct GameState
{
   // The number of playable "spaces/systems" in the game
   unsigned nodeCount;

   // Edges of the graph
   unsigned* adjacencyMatrix;

   // ID of player controlling each node
   unsigned* controlledBy; // not serialized, derived from turn data
   unsigned* controlledByInitial;

   // 3d positions of the nodes in the graph
   Vec4* nodeSpacePositions;

   // The ID of the game session
   char id[7];

   enum MetaGameState metaGameState;

   char* gameName;

   // Player info
   unsigned playerCount;

   char** playerName;
   
   char** playerColor;
   
   char** playerSecret;

   unsigned winningPlayer;

   // Turns
   unsigned turnCount;

   struct Turn* turn;
};

// Functions for managing the game state

struct GameState initatePreGame(const char* gameName);

void startGame(struct GameState* state);

void addPlayer(struct GameState* game, const char* name, char* color, const char* playerSecret);

void addOrder(struct GameState* game, enum OrderType type, unsigned from, unsigned to, const char* playerSecret);

void freeGameState(struct GameState* state);

// Functions for node adjacency

unsigned nodesConnect(struct GameState* state, unsigned a, unsigned b);

void getConnectedNodes(struct GameState* state, unsigned a, unsigned* out, unsigned* outSize);

unsigned getConnectedCount(struct GameState* state, unsigned a);

void serialize(struct GameState* state, unsigned forPlayer, FILE* f);

struct GameState deserialize(FILE* f);

#endif
