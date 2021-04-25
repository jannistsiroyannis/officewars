#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <limits.h>

#include "math/vec.h"
#include "game.h"

static const unsigned MAXCONNECTIONS = 4;
static const unsigned MINCONNECTIONS = 3;
static const unsigned NODESPERPLAYER = 5;

static void disconnectNodes(struct GameState* state, unsigned a, unsigned b)
{
   state->adjacencyMatrix[a * state->nodeCount + b] = 0;
   state->adjacencyMatrix[b * state->nodeCount + a] = 0;
}

static void connectNodes(struct GameState* state, unsigned a, unsigned b)
{
   state->adjacencyMatrix[a * state->nodeCount + b] = 1;
   state->adjacencyMatrix[b * state->nodeCount + a] = 1;
}

struct GameState initatePreGame(const char* gameName)
{
   struct GameState state = {0};

   state.metaGameState = PREGAME;
   state.gameName = malloc(strlen(gameName)+1);
   strcpy(state.gameName, gameName);
   state.winningPlayer = UINT_MAX;

   return state;
}

static void findIsland(struct GameState* state, unsigned node, unsigned* visited)
{
   if (visited[node])
      return;
   
   visited[node] = 1;

   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   getConnectedNodes(state, node, connected, &connectedCount);

   for (unsigned connectedNode = 0; connectedNode < connectedCount; ++connectedNode)
   {
      findIsland(state, connected[connectedNode], visited);
   }
}

static int islandIsComplete(unsigned* visited, unsigned nodeCount)
{
   for (unsigned i = 0; i < nodeCount; ++i)
   {
      if (!visited[i])
         return 0;
   }
   return 1;
}

static Vec3 colorVec(char* color)
{
   // Color is allocated in server.c as: char color[8], and is expected on
   // form #AA00EE

   char comp[3] = {0};
   comp[0] = color[1];
   comp[1] = color[2];
   unsigned x = strtol(comp, NULL, 16);
   comp[0] = color[3];
   comp[1] = color[4];
   unsigned y = strtol(comp, NULL, 16);
   comp[0] = color[5];
   comp[1] = color[6];
   unsigned z = strtol(comp, NULL, 16);

   Vec3 c = {x,y,z};
   return c;
}

static void ensureAcceptableColor(struct GameState* game, char* color)
{
   // A players colors must be distinct from other players colors, and must
   // also not be too close to white or black (these colors are reserved for
   // the game itself and neutral worlds).

   Vec3 vChosenColor = colorVec(color);

   unsigned hadToChange = 1;
   while (hadToChange)
   {
      hadToChange = 0;

      Vec3 white = {256, 256, 256};
      Vec3 black = {0, 0, 0};
      Vec3 randomColor = {rand()%256, rand()%256, rand()%256};

      if (sqrt(V3SqrDist(white, vChosenColor)) < 80.0 || sqrt(V3SqrDist(black, vChosenColor)) < 80.0)
      {
         vChosenColor = randomColor;
         hadToChange = 1;
         continue;
      }
      for (unsigned i = 0; i < game->playerCount; ++i)
      {
         Vec3 vOtherPlayerColor = colorVec(game->playerColor[i]);
         if (sqrt(V3SqrDist(vOtherPlayerColor, vChosenColor)) < 80.0)
         {
            vChosenColor = randomColor;
            hadToChange = 1;
            break;
         }	 
      }
   }

   snprintf(color, 8, "#%02x%02x%02x",
            (unsigned) vChosenColor.x,
            (unsigned) vChosenColor.y,
            (unsigned) vChosenColor.z);
}

static void connectIslands(struct GameState* state)
{
   unsigned currentIsland[state->nodeCount];
   memset(currentIsland, 0, state->nodeCount * sizeof(currentIsland[0]));
   findIsland(state, 0, currentIsland);

   while (!islandIsComplete(currentIsland, state->nodeCount))
   {
      // Find the closest 2 pairs of nodes, each with one node _on_ the island, and one part _off_.
      // This shit is n^2, but thats ok, the graphs are small.
      float shortestDist[2] = {999998.0, 999999.0};
      unsigned nearestNodes[4];
      for (unsigned i = 0; i < state->nodeCount; ++i)
      {
         if (currentIsland[i])
         {
            for (unsigned j = 0; j < state->nodeCount; ++j)
            {
               if (!currentIsland[j])
               {
                  if (i == j)
                     continue;
            
                  float dist = V3SqrDist(V4toV3(state->nodeSpacePositions[i]), V4toV3(state->nodeSpacePositions[j]));
                  if (dist < shortestDist[0])
                  {
                     shortestDist[0] = dist;
                     nearestNodes[0] = i;
                     nearestNodes[1] = j;
                  }
                  else if (dist < shortestDist[1])
                  {
                     shortestDist[1] = dist;
                     nearestNodes[2] = i;
                     nearestNodes[3] = j;
                  }
               }
            }
         }
      }
      
      connectNodes(state, nearestNodes[0], nearestNodes[1]);
      connectNodes(state, nearestNodes[2], nearestNodes[3]);

      for (unsigned i = 0; i < 4; ++i)
      {
         unsigned node = nearestNodes[i];
         unsigned connected[MAXCONNECTIONS+2];
         unsigned connectedCount = 0;
         getConnectedNodes(state, node, connected, &connectedCount);
         while (connectedCount > MAXCONNECTIONS)
         {
            disconnectNodes(state, node, connected[rand()%connectedCount]);
            getConnectedNodes(state, node, connected, &connectedCount);
         }
      }
   
      memset(currentIsland, 0, state->nodeCount * sizeof(currentIsland[0]));
      findIsland(state, 0, currentIsland);
   }
}

static unsigned topologicalDistance(struct GameState* state, unsigned a, unsigned b)
{
   // Do BFS for shortest distance between a and b
   unsigned queue[state->nodeCount];
   unsigned visited[state->nodeCount];
   memset(visited, 0, sizeof(visited[0]) * state->nodeCount);
   unsigned head = 0, tail = 0, depth = 0, remainingAtDepth = 1;

   // For getConnected
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   
   queue[head++] = a;
   visited[a] = 1;
   while (head != tail)
   {
      // Get a node from queue
      unsigned node = queue[tail];
      if (++tail == state->nodeCount) tail = 0;

      // If we're at target, we're done
      if (node == b)
      {
         return depth;
      }
      
      // Put all not already visited connections in the queue
      getConnectedNodes(state, node, connected, &connectedCount);
      for (unsigned i = 0; i < connectedCount; ++i)
      {
         if (visited[connected[i]])
	    continue;
         visited[connected[i]] = 1;
	 
         queue[head] = connected[i];
         if (++head == state->nodeCount) head = 0;
      }

      if (--remainingAtDepth == 0)
      {
         ++depth;
         if (head > tail)
	    remainingAtDepth = head - tail;
         else
	    remainingAtDepth = head + state->nodeCount - tail;
      }
   }

   // Should never be reached, unless the graph is disjoint (which is not allowed)
   return -1;
}

static unsigned getPartitionSizeWithoutNode(struct GameState* state, unsigned node, unsigned nodeToRemove, unsigned* visited)
{
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   getConnectedNodes(state, node, connected, &connectedCount);
      
   visited[node] = 1;
   unsigned count = 1;
   for (unsigned i = 0; i < connectedCount; ++i)
   {
      unsigned candidate = connected[i];
      if (candidate != nodeToRemove &&
          !visited[candidate] &&
          state->controlledByInitial[node] == state->controlledByInitial[candidate])
      {
         count += getPartitionSizeWithoutNode(state, candidate, nodeToRemove, visited);
      }   
   }
   return count;
}

static unsigned removalWouldSplitPartition(struct GameState* state, unsigned nodeToRemove, unsigned partitionSize)
{
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   getConnectedNodes(state, nodeToRemove, connected, &connectedCount);

   // Find a start node other than nodeToRemove
   unsigned node = UINT_MAX;
   for (unsigned i = 0; i < connectedCount; ++i)
   {
      unsigned n = connected[i];
      if (state->controlledByInitial[nodeToRemove] == state->controlledByInitial[n])
      {
         node = n;
         break;
      }
   }
   if (node == UINT_MAX)
      return 1; // Partition of size 1.. would make no sense

   // Do DFS from 'node', expect to find partitionSize-1 nodes. Any less, and this removal would result in a split
   unsigned visited[state->nodeCount];
   memset(visited, 0, sizeof(visited[0]) * state->nodeCount);
   unsigned partitionSizeWhenRemoved = getPartitionSizeWithoutNode(state, node, nodeToRemove, visited);
   if (partitionSizeWhenRemoved == partitionSize-1)
      return 0;
   return 1;
}

static void partitionGraph(struct GameState* state)
{
   // The idea is assign each player (but one) one node each. and the final player
   // the rest. These are now the partitions (player territories).
   // From here we can let small partitions "steal" nodes along any edges,
   // as long as the partition being stolen from is larger.
   // Keep doing this, until all partitions are roughly equal in size.

   unsigned partitionCount = state->playerCount;
   unsigned partitionSize[partitionCount];
   memset(partitionSize, 0, sizeof(partitionSize[0]) * partitionCount);
   unsigned* belongsToPartition = state->controlledByInitial;
   memset(belongsToPartition, 0, sizeof(belongsToPartition[0]) * state->nodeCount);
   
   // Set initial partitions
   for (unsigned player = 1; player < state->playerCount; ++player)
   {
      belongsToPartition[player] = player;
      partitionSize[player] = 1;
   }
   partitionSize[0] = state->nodeCount - (state->playerCount-1);

   // Fight!
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   unsigned movement = 1;
   unsigned iterationsDone = 0;
   while(movement)
   {
      ++iterationsDone;
      if (iterationsDone >= 200) // Sometimes you just gotta give it up..
         break;
         
      movement = 0;
      for (unsigned node = 0; node < state->nodeCount; ++node)
      {
         getConnectedNodes(state, node, connected, &connectedCount);
         for (unsigned i = 0; i < connectedCount; ++i)
         {
            unsigned candidate = connected[i];
            
            unsigned candidatePartition = belongsToPartition[candidate];
            unsigned nodePartition = belongsToPartition[node];
            if (candidatePartition != nodePartition &&
                partitionSize[candidatePartition] > partitionSize[nodePartition] &&
                !removalWouldSplitPartition(state, candidate, partitionSize[candidatePartition]))
            {
               // Steal!
               belongsToPartition[candidate] = nodePartition;
               partitionSize[candidatePartition]--;
               partitionSize[nodePartition]++;
               movement = 1;
               //fprintf(stderr, "Stole %u from player %u, to player %u\n", candidate, candidatePartition, nodePartition);
            }
         }
      }
   }
}

static unsigned bordersToEnemy(struct GameState* state, unsigned node, unsigned player)
{
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   getConnectedNodes(state, node, connected, &connectedCount);
   for (unsigned i = 0; i < connectedCount; ++i)
   {
      unsigned candidate = connected[i];
      unsigned candidatePlayer = state->controlledByInitial[candidate];

      if (candidatePlayer != -1 && candidatePlayer != player)
         return 1;
   }
   return 0;
}

static void repulsePlayers(struct GameState* state)
{
   unsigned connected[MAXCONNECTIONS];
   unsigned connectedCount = 0;
   unsigned better[MAXCONNECTIONS];
   unsigned betterCount = 0;
   unsigned movement = 1;
   unsigned iterationsDone = 0;
   while(movement)
   {
      ++iterationsDone;
      if (iterationsDone >= 200) // Sometimes you just gotta give it up..
         break;
         
      movement = 0;
      for (unsigned node = 0; node < state->nodeCount; ++node)
      {
         unsigned player = state->controlledByInitial[node];
         if (player != -1)
         {
            betterCount = 0;
            getConnectedNodes(state, node, connected, &connectedCount);
            for (unsigned i = 0; i < connectedCount; ++i)
            {
               unsigned candidate = connected[i];
               if (!bordersToEnemy(state, candidate, player) &&
                   state->controlledByInitial[candidate] == -1)
               {
                  better[betterCount++] = candidate;
               }
            }

            if (betterCount)
            {
               unsigned selectedBetterNode = better[rand() % betterCount];
               state->controlledByInitial[node] = -1;
               state->controlledByInitial[selectedBetterNode] = player;
               movement = 1;
            }
         }
      }
   }
}

void startGame(struct GameState* state)
{
   if (state->metaGameState != PREGAME)
      return;
   
   state->nodeCount = NODESPERPLAYER * state->playerCount;
   state->metaGameState = INGAME;
   state->adjacencyMatrix = calloc(sizeof(*(state->adjacencyMatrix)),
                                   (state->nodeCount * state->nodeCount));
   state->controlledBy = calloc(sizeof(*(state->controlledBy)), state->nodeCount);
   state->controlledByInitial = calloc(sizeof(*(state->controlledByInitial)), state->nodeCount);
   state->nodeSpacePositions = calloc(sizeof(*(state->nodeSpacePositions)), state->nodeCount);
   
   // Random out the nodes
   for (unsigned node = 0; node < state->nodeCount; ++node)
   {
      state->nodeSpacePositions[node].x = ((float) (rand() % 1000) - 500.0) / 500.0;
      state->nodeSpacePositions[node].y = ((float) (rand() % 1000) - 500.0) / 500.0;
      state->nodeSpacePositions[node].z = 0.1 * ((float) (rand() % 1000) - 500.0) / 500.0; // Keep the galaxy flat-ish. If it's too "deep" it gets confusing.
      state->nodeSpacePositions[node].w = 1.0;

      state->controlledByInitial[node] = -1;
   }

   // Randomly connect  nodes
   for (unsigned node = 0; node < state->nodeCount; ++node)
   {
      unsigned connectionsWanted = (rand() % (MAXCONNECTIONS - MINCONNECTIONS + 1)) + MINCONNECTIONS;
      
      while (getConnectedCount(state, node) < connectionsWanted)
      {
         unsigned nearestNode = UINT_MAX;
         float nearestDistance = 9999999.0;
         for (unsigned candidate = 0; candidate < state->nodeCount; ++candidate)
         {
            float distance = sqrt(V3SqrDist(V4toV3(state->nodeSpacePositions[node]),
                                            V4toV3(state->nodeSpacePositions[candidate])));
	    
            if (!nodesConnect(state, node, candidate) &&
                distance < nearestDistance &&
                node != candidate &&
                getConnectedCount(state, candidate) < MAXCONNECTIONS)
            {
               nearestNode = candidate;
               nearestDistance = distance;
            }
         }

         if (nearestNode != UINT_MAX)
            connectNodes(state, node, nearestNode);
         else
            break; // It may not possible to connect this node any further, causing an infinite loop.
      }
   }

   // Walk the walk, islands are not allowed
   connectIslands(state);

   // Find pairs of nodes that are extremely far apart (in topology)
   // and connect them. This should balance the problem of "chain/highway galaxies"
   for (unsigned iterations = 0; iterations < 2; ++iterations)
   {
      unsigned farthestNode[2] = {UINT_MAX, UINT_MAX};
      unsigned farthestDistance = 0;
      for (unsigned node = 0; node < state->nodeCount; ++node)
      {
         if (getConnectedCount(state, node) >= MAXCONNECTIONS)
            continue;
         
         for (unsigned candidate = 0; candidate < state->nodeCount; ++candidate)
         {
            unsigned distance = topologicalDistance(state, node, candidate);
            if (!nodesConnect(state, node, candidate) &&
                distance > farthestDistance &&
                node != candidate &&
                getConnectedCount(state, candidate) < MAXCONNECTIONS)
            {
               farthestNode[0] = node;
               farthestNode[1] = candidate;
               farthestDistance = distance;
            }
         }
      }
      if (farthestNode[0] != UINT_MAX && farthestNode[1] != UINT_MAX)
         connectNodes(state, farthestNode[0], farthestNode[1]);
   }
   
   // Random out player positions
   {
      unsigned connected[MAXCONNECTIONS];
      unsigned connectedCount = 0;
      
      for (unsigned player = 0; player < state->playerCount; ++player)
      {
         unsigned node = rand() % state->nodeCount;
         while (state->controlledByInitial[node] != -1)
         {
            node = rand() % state->nodeCount;
         }
         state->controlledByInitial[node] = player;
      }
   }
   
   // Doesn't _really_ work. Too often, no stealing can be done,
   // because everywhere you try, the target partition would be divided,
   // meaning it stays huge.
   //partitionGraph(state); 

   repulsePlayers(state);
   
   // Do some passes of atract/repulse to make the graph easier on the human eye
   for (unsigned iterations = 0; iterations < 100; ++iterations)
   {
      for (unsigned node = 0; node < state->nodeCount; ++node)
      {
         for (unsigned otherNode = 0; otherNode < state->nodeCount; ++otherNode)
         {
            Vec3 v3Node = V4toV3(state->nodeSpacePositions[node]);
            Vec3 v3OtherNode = V4toV3(state->nodeSpacePositions[otherNode]);

            if (nodesConnect(state, node, otherNode))
            {
               // Atract
               if (V3Length(V3Subt(v3OtherNode, v3Node)) > 0.5)
               {
                  Vec3 towardsOther = V3ScalarMult(0.2, V3Normalized(V3Subt(v3OtherNode, v3Node)));
                  v3Node = V3Add(v3Node, towardsOther);
               }
               // (Repulse if TOO close)
               else if (V3Length(V3Subt(v3OtherNode, v3Node)) < 0.3)
               {
                  Vec3 towardsOther = V3ScalarMult(-0.3, V3Normalized(V3Subt(v3OtherNode, v3Node)));
                  v3Node = V3Add(v3Node, towardsOther);
               }
            }
            else
            {
               // Repulse
               if (V3Length(V3Subt(v3OtherNode, v3Node)) < 0.6)
               {
                  Vec3 towardsOther = V3ScalarMult(-0.3, V3Normalized(V3Subt(v3OtherNode, v3Node)));
                  v3Node = V3Add(v3Node, towardsOther);
               }
            }

            state->nodeSpacePositions[node] = V3toV4(v3Node);
            state->nodeSpacePositions[otherNode] = V3toV4(v3OtherNode);
         }
      }
   }
   
   // Start the first turn
   state->turnCount = 1;
   state->turn = calloc(state->turnCount, sizeof(state->turn[0]));
}

void addPlayer(struct GameState* game, const char* name, char* color, const char* playerSecret)
{
   if (game->metaGameState != PREGAME)
      return;
   
   game->playerCount++;
   game->playerName = realloc(game->playerName, game->playerCount * sizeof(game->playerName[0]));
   game->playerColor = realloc(game->playerColor, game->playerCount * sizeof(game->playerColor[0]));
   game->playerSecret = realloc(game->playerSecret, game->playerCount * sizeof(game->playerSecret[0]));
   game->playerName[game->playerCount-1] = malloc(64);
   game->playerColor[game->playerCount-1] = malloc(8);
   game->playerSecret[game->playerCount-1] = malloc(7);
   strcpy(game->playerName[game->playerCount-1], name);
   ensureAcceptableColor(game, color);
   strcpy(game->playerColor[game->playerCount-1], color);
   strcpy(game->playerSecret[game->playerCount-1], playerSecret);
}

void addOrder(struct GameState* game, unsigned type, unsigned from, unsigned to, const char* playerSecret)
{
   // Check that the game is in the expected state for new orders
   if (game->metaGameState != INGAME)
      return;
   
   // Check that the player actually owns the "from" system
   unsigned playerId = game->controlledBy[from];
   if ( strcmp(game->playerSecret[playerId], playerSecret) ||
        ( !nodesConnect(game, from, to) && game->controlledBy[to] != playerId ) )
   {
      return;
   }
       
   struct Turn* turn = &(game->turn[game->turnCount-1]);

   // Remove any other orders from that same node
   for (unsigned i = 0; i < turn->orderCount; ++i)
   {
      if (turn->fromNode[i] == from)
      {
         unsigned remaining = turn->orderCount - i - 1;
         memmove(&(turn->issuingPlayer[i]), &(turn->issuingPlayer[i+1]), remaining * sizeof(turn->issuingPlayer[0]));
         memmove(&(turn->fromNode[i]), &(turn->fromNode[i+1]), remaining * sizeof(turn->fromNode[0]));
         memmove(&(turn->toNode[i]), &(turn->toNode[i+1]), remaining * sizeof(turn->toNode[0]));
         memmove(&(turn->type[i]), &(turn->type[i+1]), remaining * sizeof(turn->type[0]));
         --(turn->orderCount);
         break;
      }
   }

   // resize orders arrays
   turn->issuingPlayer = realloc(turn->issuingPlayer, (turn->orderCount+1) * sizeof(turn->issuingPlayer[0]));
   turn->fromNode = realloc(turn->fromNode, (turn->orderCount+1) * sizeof(turn->fromNode[0]));
   turn->toNode = realloc(turn->toNode, (turn->orderCount+1) * sizeof(turn->toNode[0]));
   turn->type = realloc(turn->type, (turn->orderCount+1) * sizeof(turn->type[0]));

   // If the target is "your own", be helpful and convert to a supportorder.
   if (game->controlledBy[to] == playerId)
      type = 1;

   // Add the new orders at the end
   turn->issuingPlayer[turn->orderCount] = playerId;
   turn->fromNode[turn->orderCount] = from;
   turn->toNode[turn->orderCount] = to;
   turn->type[turn->orderCount] = type;
   turn->orderCount++;
}

void freeGameState(struct GameState* state)
{
   if (state->adjacencyMatrix)
      free(state->adjacencyMatrix);
   if (state->controlledBy)
      free(state->controlledBy);
   if (state->controlledByInitial)
      free(state->controlledByInitial);
   if (state->nodeSpacePositions)
      free(state->nodeSpacePositions);
   if (state->gameName)
      free(state->gameName);
   for (unsigned i = 0; i < state->playerCount; ++i)
   {
      free(state->playerName[i]);
      free(state->playerColor[i]);
      free(state->playerSecret[i]);
   }
   for (unsigned i = 0; i < state->turnCount; ++i)
   {
      free(state->turn[i].issuingPlayer);
      free(state->turn[i].fromNode);
      free(state->turn[i].toNode);
      free(state->turn[i].type);
   }
}

unsigned nodesConnect(struct GameState* state, unsigned a, unsigned b)
{
   /*
     Assuming 4 nodes, 0..3:

     the matrix might look something like:

     0  1  2  3
     ----------
     0 |0, 0, 1, 0
     1 |0, 0, 0, 1
     2 |1, 0, 0, 0
     3 |0, 1, 0, 0
   */

   return state->adjacencyMatrix[a * state->nodeCount + b];
}

/*
  Fills the 'out' array with all nodes connected to 'a'.
  'out' should be nodeCount in size.
  'outSize' tells the caller how many nodes are actually in the 'out' list
*/
void getConnectedNodes(struct GameState* state, unsigned a, unsigned* out, unsigned* outSize)
{
   unsigned* nodeRow = & (state->adjacencyMatrix[a * state->nodeCount]);
   
   *outSize = 0;
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      if (nodeRow[i])
      {
         out[(*outSize)++] = i;
      }
   }
}

unsigned getConnectedCount(struct GameState* state, unsigned a)
{
   unsigned* nodeRow = & (state->adjacencyMatrix[a * state->nodeCount]);
   unsigned count = 0;
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      if (nodeRow[i])
         ++count;
   }
   return count;
}

/*
  forPlayer carries a very special meaning here.
  If it is -1 (overflow), all information is serialized,
  If if is -2 (overflow), no secrets (pending orders/tokens) are serialized
  Otherwise, forPlayer should be a valid player id (number), meaning
  only serialize what that player should be allowed to know.

  Deserialization must be unaffected by this.
*/
void serialize(struct GameState* state, unsigned forPlayer, FILE* f)
{
   fprintf(f, "%s\n", state->id);
   fprintf(f, "%s\n", state->gameName);
   fprintf(f, "%d\n", state->metaGameState);
   fprintf(f, "%u\n", state->winningPlayer);
   fprintf(f, "%u\n", state->playerCount);
   for (unsigned i = 0; i < state->playerCount; ++i)
   {
      fprintf(f, "%s\n", state->playerName[i]);
      fprintf(f, "%s\n", state->playerColor[i]);
      if (forPlayer == -1 || forPlayer == i)
         fprintf(f, "%s\n", state->playerSecret[i]);
      else
         fprintf(f, "REDACT\n");
   }
   
   fprintf(f, "%u\n", state->nodeCount);
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      for (unsigned j = 0; j < state->nodeCount; ++j)
      {
         fprintf(f, "%u", state->adjacencyMatrix[i*(state->nodeCount)+j]);
      }
      fprintf(f, "\n");
   }
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      fprintf(f, "%.5f,%.5f,%.5f\n",
	      state->nodeSpacePositions[i].x,
	      state->nodeSpacePositions[i].y,
	      state->nodeSpacePositions[i].z
         ); // w not necessary, always 1.0
   }
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      fprintf(f, "%u\n", state->controlledByInitial[i]);
   }
   
   fprintf(f, "%u\n", state->turnCount);

   // Careful here, on the last (current) turn, return only the players own orders!!
   if (forPlayer == -1) // Serialize everything
   {
      for (unsigned i = 0; i < state->turnCount; ++i)
      {
         fprintf(f, "%u\n", state->turn[i].orderCount);
         for (unsigned j = 0; j < state->turn[i].orderCount; ++j)
         {
            fprintf(f, "%u\n", state->turn[i].issuingPlayer[j]);
            fprintf(f, "%u\n", state->turn[i].fromNode[j]);
            fprintf(f, "%u\n", state->turn[i].toNode[j]);
            fprintf(f, "%u\n", state->turn[i].type[j]);
         }
      }
   }
   else if (forPlayer != -2)// Only forPlayers pending orders
   {
      // All historic turns
      for (unsigned i = 0; i < state->turnCount-1; ++i)
      {
         fprintf(f, "%u\n", state->turn[i].orderCount);
         for (unsigned j = 0; j < state->turn[i].orderCount; ++j)
         {
            fprintf(f, "%u\n", state->turn[i].issuingPlayer[j]);
            fprintf(f, "%u\n", state->turn[i].fromNode[j]);
            fprintf(f, "%u\n", state->turn[i].toNode[j]);
            fprintf(f, "%u\n", state->turn[i].type[j]);
         }
      }

      // Pending orders, only for the requesting player
      unsigned orderCount = 0;
      for (unsigned j = 0; j < state->turn[state->turnCount-1].orderCount; ++j)
      {
         if (state->turn[state->turnCount-1].issuingPlayer[j] == forPlayer)
	    ++orderCount;
      }
      fprintf(f, "%u\n", orderCount);
      for (unsigned j = 0; j < state->turn[state->turnCount-1].orderCount; ++j)
      {
         if (state->turn[state->turnCount-1].issuingPlayer[j] == forPlayer)
         {
            fprintf(f, "%u\n", state->turn[state->turnCount-1].issuingPlayer[j]);
            fprintf(f, "%u\n", state->turn[state->turnCount-1].fromNode[j]);
            fprintf(f, "%u\n", state->turn[state->turnCount-1].toNode[j]);
            fprintf(f, "%u\n", state->turn[state->turnCount-1].type[j]);
         }
      }
   }
   else // history for everyone
   {
      // All historic turns
      if (state->turnCount > 0)
      {
         for (unsigned i = 0; i < state->turnCount-1; ++i)
         {
            fprintf(f, "%u\n", state->turn[i].orderCount);
            for (unsigned j = 0; j < state->turn[i].orderCount; ++j)
            {
               fprintf(f, "%u\n", state->turn[i].issuingPlayer[j]);
               fprintf(f, "%u\n", state->turn[i].fromNode[j]);
               fprintf(f, "%u\n", state->turn[i].toNode[j]);
               fprintf(f, "%u\n", state->turn[i].type[j]);
            }
         }
         fprintf(f, "0\n"); // no visible orders this round
      }
   }

}

struct GameState deserialize(FILE* f)
{
   struct GameState state;
      
   fscanf(f, "%6[^\n]\n", state.id);
   state.gameName = malloc(64);
   fscanf(f, "%63[^\n]\n", state.gameName);
   fscanf(f, "%d\n", &state.metaGameState);
   fscanf(f, "%u\n", &state.winningPlayer);
   fscanf(f, "%u\n", &state.playerCount);
   state.playerName = malloc(state.playerCount * sizeof(state.playerName[0]));
   state.playerColor = malloc(state.playerCount * sizeof(state.playerColor[0]));
   state.playerSecret = malloc(state.playerCount * sizeof(state.playerSecret[0]));
   for (unsigned i = 0; i < state.playerCount; ++i)
   {
      state.playerName[i] = malloc(64);
      fscanf(f, "%63[^\n]\n", state.playerName[i]);
      state.playerColor[i] = malloc(8);
      fscanf(f, "%7[^\n]\n", state.playerColor[i]);
      state.playerSecret[i] = malloc(7);
      fscanf(f, "%6[^\n]\n", state.playerSecret[i]);
   }

   fscanf(f, "%u\n", &state.nodeCount);
   
   state.adjacencyMatrix = calloc(sizeof(*(state.adjacencyMatrix)), (state.nodeCount * state.nodeCount));
   state.controlledBy = calloc(sizeof(*(state.controlledBy)), state.nodeCount);
   state.controlledByInitial = calloc(sizeof(*(state.controlledByInitial)), state.nodeCount);
   state.nodeSpacePositions = calloc(sizeof(*(state.nodeSpacePositions)), state.nodeCount);
   
   for (unsigned i = 0; i < state.nodeCount; ++i)
   {
      for (unsigned j = 0; j < state.nodeCount; ++j)
      {
         fscanf(f, "%1u", &state.adjacencyMatrix[i*(state.nodeCount)+j]);
      }
      fscanf(f, "\n");
   }

   for (unsigned i = 0; i < state.nodeCount; ++i)
   {
      fscanf(f, "%f,%f,%f\n",
             &state.nodeSpacePositions[i].x,
             &state.nodeSpacePositions[i].y,
             &state.nodeSpacePositions[i].z
         );
      state.nodeSpacePositions[i].w = 1.0;
   }

   for (unsigned i = 0; i < state.nodeCount; ++i)
   {
      fscanf(f, "%u\n", &state.controlledByInitial[i]);
   }
   
   fscanf(f, "%u\n", &state.turnCount);
   state.turn = calloc(state.turnCount, sizeof(state.turn[0]));
   for (unsigned i = 0; i < state.turnCount; ++i)
   {
      fscanf(f, "%u\n", &state.turn[i].orderCount);
      
      state.turn[i].issuingPlayer = malloc(sizeof(state.turn[i].issuingPlayer[0]) * state.turn[i].orderCount);
      state.turn[i].fromNode = malloc(sizeof(state.turn[i].fromNode[0]) * state.turn[i].orderCount);
      state.turn[i].toNode = malloc(sizeof(state.turn[i].toNode[0]) * state.turn[i].orderCount);
      state.turn[i].type = malloc(sizeof(state.turn[i].type[0]) * state.turn[i].orderCount);
      
      for (unsigned j = 0; j < state.turn[i].orderCount; ++j)
      {
         fscanf(f, "%u\n", &state.turn[i].issuingPlayer[j]);
         fscanf(f, "%u\n", &state.turn[i].fromNode[j]);
         fscanf(f, "%u\n", &state.turn[i].toNode[j]);
         fscanf(f, "%u\n", &state.turn[i].type[j]);
      }
      
   }
   
   return state;
}
