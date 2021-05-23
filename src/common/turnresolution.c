#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "turnresolution.h"
#include "game.h"

// This needs an explanation. Because the turn resolution _must_ be guaranteed to
// play out _exactly_ the same way every time on every platfrom for any given
// set of inputs, floating point math won't work. If the WASM runtime is for
// example compiled with --ffast-math on one machine, but with IEEE 754 floats on
// another, the results could differ, which could (in theory) lead to two players
// incorrectly "seeing" different outcomes of a battle, which breaks the game.
// For this reason we need fixed point arithmetic when counting node strengths.
#define FIXED_SHIFT 16
#define FIXED_MASK 0x0000FFFF
typedef uint32_t fixed_real;

static fixed_real calculateStrengthInternal(struct GameState* game, struct Turn* turn, unsigned node, unsigned* pinned, unsigned* visited)
{
   fixed_real strength = 1 << FIXED_SHIFT;
   for (unsigned order = 0; order < turn->orderCount; ++order)
   {
      if (turn->toNode[order] == node &&
	  turn->type[order] == SUPPORTORDER &&
	  nodesConnect(game, node, turn->fromNode[order]) &&
	  !pinned[turn->fromNode[order]] &&
          !visited[turn->fromNode[order]])
      {
         // 'turn->fromNode[order]' supports 'node' and is not pinned
         visited[turn->fromNode[order]] = 1;
         strength += calculateStrengthInternal(game, turn, turn->fromNode[order], pinned, visited) / 2;
      }
   }
   return strength;
}

static fixed_real calculateStrength(struct GameState* game, struct Turn* turn, unsigned node, unsigned* pinned)
{
   if (game->controlledBy[node] == -1)
      return 0; // Neutral nodes have no strength

   unsigned visited[game->nodeCount];
   memset(visited, 0, sizeof(visited[0]) * game->nodeCount);
   visited[node] = 1; // Can't support yourself
   return calculateStrengthInternal(game, turn, node, pinned, visited);
}

static int existsControlledPathToInt(struct GameState* game, unsigned from, unsigned to, unsigned playerId, unsigned* visited)
{
   if (from == to)
      return 1;
   
   unsigned connectedCount = 0;
   unsigned connected[game->nodeCount];
   getConnectedNodes(game, from, connected, &connectedCount);

   visited[from] = 1;
   for (unsigned i = 0; i < connectedCount; ++i)
   {
      unsigned node = connected[i];
      if (!visited[node] && game->controlledBy[node] == playerId)
      {
         if (existsControlledPathToInt(game, node, to, playerId, visited))
	    return 1;
      }
   }

   return 0;
}

static int existsControlledPathTo(struct GameState* game, unsigned from, unsigned to, unsigned playerId)
{
   unsigned visited[game->nodeCount];
   memset(visited, 0, sizeof(visited[0]) * game->nodeCount);
   return existsControlledPathToInt(game, from, to, playerId, visited);
}

static void findPinnedWorlds(struct GameState* game, struct Turn* turn, unsigned* pinned)
{
   memset(pinned, 0, sizeof(pinned[0]) * game->nodeCount);
   for (unsigned order = 0; order < turn->orderCount; ++order)
   {
      if (turn->type[order] == ATTACKORDER &&
	  nodesConnect(game, turn->fromNode[order], turn->toNode[order]))
      {
         // turn->toNode[order] is under attack, and must now be considered pinned
         pinned[turn->toNode[order]] = 1;
      }
   }
}

static int resolveTurn(struct GameState* game, unsigned turnIndex)
{
   struct Turn* turn = &game->turn[turnIndex];
   int gameStateWasChanged = 0;

   // Pinning
   unsigned pinned[game->nodeCount];
   findPinnedWorlds(game, turn, pinned);
   
   // calculate all node strengths
   fixed_real nodeStrength[game->nodeCount];
   memset(nodeStrength, 0, sizeof(nodeStrength[0]) * game->nodeCount);
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      nodeStrength[node] = calculateStrength(game, turn, node, pinned);
   }

   // Battles
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      // Find the strongest attacking force
      unsigned strongestAttackingPlayer = -1;
      unsigned strongestAttackingCandidate = -1;
      fixed_real candidateStrength = 0;
      for (unsigned order = 0; order < turn->orderCount; ++order)
      {
         if (turn->toNode[order] == node &&
             turn->type[order] == ATTACKORDER &&
             nodesConnect(game, node, turn->fromNode[order]))
         {
            // Judge strength of attack
            fixed_real strength = nodeStrength[turn->fromNode[order]];
            if (strength > candidateStrength) // a new strongest attacker
            {
               strongestAttackingPlayer = turn->issuingPlayer[order];
               strongestAttackingCandidate = turn->fromNode[order];
               candidateStrength = strength;
            } else if (strength == candidateStrength && // a tied attacker (another player). None must win!
                       turn->issuingPlayer[order] != strongestAttackingPlayer)
            {
               strongestAttackingPlayer = -1;
               strongestAttackingCandidate = -1;
               candidateStrength = strength;
            }
         }
      }

      // If one attacker is stronger than all other attackers and stronger than the defence, the attacker wins
      if (strongestAttackingCandidate != -1 && candidateStrength > nodeStrength[node])
      {
         game->controlledBy[node] = strongestAttackingPlayer;
         gameStateWasChanged = 1;
      }
   }

   // Surrenders
   for (unsigned order = 0; order < turn->orderCount; ++order)
   {
      if (turn->type[order] == SURRENDERORDER)
      {
         for (unsigned node = 0; node < game->nodeCount; ++node)
         {
            if (game->controlledBy[node] == turn->issuingPlayer[order])
            {
               game->controlledBy[node] = UINT_MAX; 
            }
         }
      }
   }
   
   return gameStateWasChanged;
}

void tickGame(struct GameState* game)
{
   if (game->metaGameState != INGAME)
      return;
   
   fprintf(stderr, "TICK GOING turn now: %u!\n", game->turnCount);
   // Add a new turn, implicitly finalizing the current one
   game->turnCount++;
   game->turn = realloc(game->turn, game->turnCount * sizeof(game->turn[0]));
   game->turn[game->turnCount-1].orderCount = 0;
   // probably useless, but for completeness
   game->turn[game->turnCount-1].issuingPlayer = malloc(0);
   game->turn[game->turnCount-1].fromNode = malloc(0);
   game->turn[game->turnCount-1].toNode = malloc(0);
   game->turn[game->turnCount-1].type = malloc(0);
}

void stepGameHistory(struct GameState* game, unsigned targetStep)
{
   // Copy the initial state as the current one
   memcpy(game->controlledBy, game->controlledByInitial, sizeof(game->controlledByInitial[0]) * game->nodeCount);
   
   unsigned steps = targetStep > game->turnCount-1 ? game->turnCount-1 : targetStep;

   int gameStateWasChanged;
   for (unsigned i = 0; i < steps; ++i)
   {
      gameStateWasChanged = resolveTurn(game, i);
   }

   // Test the game state for a winner
   if (!gameStateWasChanged)
   {
      unsigned nodesPerPlayer[game->playerCount];
      memset(nodesPerPlayer, 0, sizeof(nodesPerPlayer[0]) * game->playerCount);
      for (unsigned node = 0; node < game->nodeCount; ++node)
      {
         if (game->controlledBy[node] != UINT_MAX)
         {
            nodesPerPlayer[game->controlledBy[node]]++;
         }
      }

      unsigned remainingPlayers = 0;
      unsigned highestNodeCount = 0;
      unsigned winningPlayer = UINT_MAX;
      for (unsigned player = 0; player < game->playerCount; ++player)
      {
         if (nodesPerPlayer[player])
            ++remainingPlayers;

         if (nodesPerPlayer[player] == highestNodeCount)
         {
            winningPlayer = UINT_MAX;
         }
         else if (nodesPerPlayer[player] > highestNodeCount)
         {
            highestNodeCount = nodesPerPlayer[player];
            winningPlayer = player;
         }
      }

      if (remainingPlayers < 3)
      {
         // There are now 1 or 2 players remaining, and the game is "locked" (no node changed owner)
         // This means the game is over.
         game->metaGameState = POSTGAME;
         game->winningPlayer = winningPlayer;
      }
   }
}

void stepGameHistoryLatest(struct GameState* game)
{
   unsigned targetStep = game->turnCount > 0 ? game->turnCount-1 : 0;
   stepGameHistory(game, targetStep);
}

void calculateDisplayStrengths(struct GameState* game, unsigned turnIndex, float* strength)
{
   struct Turn* turn = &game->turn[turnIndex];

   unsigned pinned[game->nodeCount];
   findPinnedWorlds(game, turn, pinned);

   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      fixed_real fixedStrength = calculateStrength(game, turn, node, pinned);
      float floatStrength = (fixedStrength >> FIXED_SHIFT) +
         ( (float)(fixedStrength & FIXED_MASK) / (float)FIXED_MASK) ;
      strength[node] = floatStrength;
   }
}
