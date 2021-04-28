#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "turnresolution.h"
#include "game.h"

static unsigned countSupporters(struct GameState* game, struct Turn* turn, unsigned node, unsigned* pinned)
{
   // How many connected fleets are ordered to support the fleet at node ?
   unsigned supporters = 0;
   for (unsigned order = 0; order < turn->orderCount; ++order)
   {
      if (turn->toNode[order] == node &&
	  turn->type[order] == SUPPORTORDER &&
	  nodesConnect(game, node, turn->fromNode[order]) &&
	  !pinned[turn->fromNode[order]])
      {
         ++supporters;
      }
   }
   return supporters;
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

static int resolveTurn(struct GameState* game, unsigned turnIndex)
{
   struct Turn* turn = &game->turn[turnIndex];
   int gameStateWasChanged = 0;

   // Pinning
   unsigned pinned[game->nodeCount];
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

   // calculate all defensive strengths
   unsigned defensiveStrength[game->nodeCount];
   memset(defensiveStrength, 0, sizeof(defensiveStrength[0]) * game->nodeCount);
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {  
      unsigned initialDefenceStrength = 1;
      if (game->controlledBy[node] == -1)
         initialDefenceStrength = 0;
      
      defensiveStrength[node] = initialDefenceStrength + countSupporters(game, turn, node, pinned);  
   }

   // Battles
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      // Find the strongest attacking force
      unsigned strongestAttackingPlayer = -1;
      unsigned strongestAttackingCandidate = -1;
      unsigned candidateStrength = 0;
      for (unsigned order = 0; order < turn->orderCount; ++order)
      {
         if (turn->toNode[order] == node &&
             turn->type[order] == ATTACKORDER &&
             nodesConnect(game, node, turn->fromNode[order]))
         {
            // Judge strength of attack
            unsigned strength = 1 + countSupporters(game, turn, turn->fromNode[order], pinned);
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
      if (strongestAttackingCandidate != -1 && candidateStrength > defensiveStrength[node])
      {
         game->controlledBy[node] = strongestAttackingPlayer;
         gameStateWasChanged = 1;
      }
   }

   // Surrenders
   unsigned allSurrendersDone;
   do
   {
      allSurrendersDone = 1;
      for (unsigned order = 0; order < turn->orderCount; ++order)
      {
         if (turn->type[order] == SURRENDERORDER)
         {
            for (unsigned node = 0; node < game->nodeCount; ++node)
            {
               if (game->controlledBy[node] == turn->issuingPlayer[order])
               {
                  game->controlledBy[node] = turn->toNode[order]; // toNode doubles as "to player" if the order type is SURRENDERORDER
                  allSurrendersDone = 0;
               }
            }
         }
      }
   } while (!allSurrendersDone);

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
