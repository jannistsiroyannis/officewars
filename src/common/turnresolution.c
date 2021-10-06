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

   // How many total orders from that same node?
   unsigned splitOrderCount = 0;
   for (unsigned order = 0; order < turn->orderCount; ++order)
   {
      if (turn->fromNode[order] == node)
         ++splitOrderCount;
   }

   if (splitOrderCount)
      return strength / splitOrderCount;
   return strength;
}

static fixed_real calculateStrength(struct GameState* game, struct Turn* turn, unsigned node, unsigned* pinned)
{
   if (game->controlledBy[node] == -1)
      return (1 << FIXED_SHIFT) / 2; // Neutral nodes have a set strength, and cannot receive support

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
      {
         // How many total orders from that same node? Because splitting a nodes power only affects
         // it's offence and support, _not_ it's defence.
         unsigned splitOrderCount = 0;
         for (unsigned order = 0; order < turn->orderCount; ++order)
         {
            if (turn->fromNode[order] == node)
               ++splitOrderCount;
         }

         // This is _super_ tricky! Saying "candidateStrength > splitOrderCount * nodeStrength[node]" would
         // make a lot more sense to a human reader, BUT we're dealing with finite precision real numbers now,
         // which means ((A / n) * n) might in fact _not_ be equal to A (due to truncation errors). So,
         // in order to not let what should be a draw, turn into somebody randomly winning, it's better
         // that both the attackers strength and the defenders are divided by the same number, rather
         // than trying to multiply the defensive strength back up to what it originally was.
         fixed_real candidateStrengthScaled = candidateStrength;
         if (splitOrderCount)
            candidateStrengthScaled = candidateStrength/splitOrderCount;
         
         if (strongestAttackingCandidate != -1 && candidateStrengthScaled > nodeStrength[node])
         {
            game->controlledBy[node] = strongestAttackingPlayer;
            gameStateWasChanged = 1;
         }
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

   unsigned movedPlayers[game->playerCount];
   unsigned hasNodes[game->playerCount];
   memset(movedPlayers, 0, game->playerCount * sizeof(movedPlayers[0]));
   memset(hasNodes, 0, game->playerCount * sizeof(hasNodes[0]));
   struct Turn* currentTurn = &game->turn[game->turnCount-1];
   for (unsigned order = 0; order < currentTurn->orderCount; ++order)
   {
      unsigned playerId = currentTurn->issuingPlayer[order];
      movedPlayers[playerId] = 1;
   }
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      unsigned playerId = game->controlledBy[node];
      if (playerId != UINT_MAX)
         hasNodes[playerId] = 1;
   }
   unsigned everyoneMoved = 1;
   for (unsigned i = 0; i < game->playerCount; ++i)
   {
      if (!movedPlayers[i] && hasNodes[i])
      {
         everyoneMoved = 0;
         fprintf(stderr, "Did not move: %s\n", game->playerName[i]);
      }
   }
   
   if (!everyoneMoved)
      return;

   fprintf(stderr, "All players checked in, now ticking to turn: %u!\n", game->turnCount);
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

   for (unsigned i = 0; i < steps; ++i)
   {
      resolveTurn(game, i);
   }

   // Test the game state for a winner
   unsigned winningPlayer = UINT_MAX;
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      if (game->controlledBy[node] != UINT_MAX && game->controlledBy[node] != winningPlayer)
      {
         if (winningPlayer != UINT_MAX) // More than 1 player remaining.
         {
            winningPlayer = UINT_MAX;
            break;
         }
         else 
            winningPlayer = game->controlledBy[node];
      }
   }
   if (winningPlayer != UINT_MAX)
   {
      // "There can be only one!"
      game->metaGameState = POSTGAME;
      game->winningPlayer = winningPlayer;
   }
}

void stepGameHistoryLatest(struct GameState* game)
{
   unsigned targetStep = game->turnCount > 0 ? game->turnCount-1 : 0;
   stepGameHistory(game, targetStep);
}

void calculateDisplayStrengths(struct GameState* game, unsigned turnIndex, float* strength, unsigned* orderCount)
{
   struct Turn* turn = &game->turn[turnIndex];

   unsigned pinned[game->nodeCount];
   findPinnedWorlds(game, turn, pinned);

   // What's the (offensive) strength of each node
   for (unsigned node = 0; node < game->nodeCount; ++node)
   {
      fixed_real fixedStrength = calculateStrength(game, turn, node, pinned);
      float floatStrength = (fixedStrength >> FIXED_SHIFT) +
         ( (float)(fixedStrength & FIXED_MASK) / (float)FIXED_MASK) ;
      strength[node] = floatStrength;

      // How many total orders from that same node?
      unsigned splitOrderCount = 0;
      for (unsigned order = 0; order < turn->orderCount; ++order)
      {
         if (turn->fromNode[order] == node)
            ++splitOrderCount;
      }
      orderCount[node] = splitOrderCount;
   }
}
