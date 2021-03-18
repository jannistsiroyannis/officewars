#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "math/vec.h"
#include "game.h"

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

   return state;
}

static void findIsland(struct GameState* state, unsigned node, unsigned* visited)
{
   if (visited[node])
   return;
   
   visited[node] = 1;

   unsigned connected[state->nodeCount];
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
   
      // Merge the islands
      findIsland(state, nearestNodes[1], currentIsland);
      findIsland(state, nearestNodes[3], currentIsland);
   }
}

void startGame(struct GameState* state)
{
   state->nodeCount = 7 * state->playerCount;
   state->metaGameState = INGAME;
   state->adjacencyMatrix = calloc(sizeof(*(state->adjacencyMatrix)),
				   (state->nodeCount * state->nodeCount));
   state->controlledBy = calloc(sizeof(*(state->controlledBy)), state->nodeCount);
   state->controlledByInitial = calloc(sizeof(*(state->controlledByInitial)), state->nodeCount);
   state->nodeSpacePositions = calloc(sizeof(*(state->nodeSpacePositions)), state->nodeCount);
   state->occupied = calloc(sizeof(*(state->occupied)), state->nodeCount);
   state->occupiedInitial = calloc(sizeof(*(state->occupiedInitial)), state->nodeCount);
   state->homeWorld = calloc(sizeof(*(state->homeWorld)), state->nodeCount);

   // Random out player home worlds with clusters
   {
      unsigned node = 0;
      for (unsigned player = 0; player < state->playerCount; ++player)
      {
	 state->nodeSpacePositions[node].x = ((float) (rand() % 1000) - 500.0) / 500.0;
	 state->nodeSpacePositions[node].y = ((float) (rand() % 1000) - 500.0) / 500.0;
	 state->nodeSpacePositions[node].z = 0.1 * ((float) (rand() % 1000) - 500.0) / 500.0; // Keep the galaxy flat-ish. If it's too "deep" it gets confusing.
	 state->nodeSpacePositions[node].w = 1.0;

	 state->controlledByInitial[node] = player;
	 state->homeWorld[node] = player;
	 state->occupiedInitial[node] = 1;

	 unsigned homeNode = node;
	 Vec3 homePos = V4toV3(state->nodeSpacePositions[node]);

	 ++node;

	 // Add reasonable controlled clusters around home worlds
	 for (unsigned i = 0; i < 3; ++i)
	 {
	    Vec3 r = {rand(), rand(), rand()};
	    r = V3Normalized(r);
	    state->nodeSpacePositions[node] = V3toV4(V3Add(homePos, V3ScalarMult(0.001, r)));
	    state->controlledByInitial[node] = player;
	    state->occupiedInitial[node] = 1;
	    state->homeWorld[node] = -1;
	    connectNodes(state, node, homeNode);
	    ++node;
	 }
      }
   }
   
   // Random out the remaining nodes
   for (unsigned node = state->playerCount * 4; node < state->nodeCount; ++node)
   {
      state->nodeSpacePositions[node].x = ((float) (rand() % 1000) - 500.0) / 500.0;
      state->nodeSpacePositions[node].y = ((float) (rand() % 1000) - 500.0) / 500.0;
      state->nodeSpacePositions[node].z = 0.1 * ((float) (rand() % 1000) - 500.0) / 500.0; // Keep the galaxy flat-ish. If it's too "deep" it gets confusing.
      state->nodeSpacePositions[node].w = 1.0;

      state->controlledByInitial[node] = -1;
      state->homeWorld[node] = -1;
   }

   // Connect the nodes
   unsigned temp[state->nodeCount];
   for (unsigned node = 0; node < state->nodeCount; ++node)
   {
      unsigned connectedCount = 0;
      unsigned connectionsWanted = (rand() % 5) + 4;
      getConnectedNodes(state, node, temp, &connectedCount);
      if (connectedCount >= connectionsWanted)
	 continue;
      unsigned connectionsNeeded = connectionsWanted - connectedCount;
      
      while (connectedCount < connectionsNeeded)
      {
	 unsigned nearestNode = 0;
	 float nearestDistance = 9999999.0;
	 for (unsigned candidate = 0; candidate < state->nodeCount; ++candidate)
	 {
	    float distance = sqrt(V3SqrDist(V4toV3(state->nodeSpacePositions[node]),
					     V4toV3(state->nodeSpacePositions[candidate])));
	    
	    if (!nodesConnect(state, node, candidate) &&
		distance < nearestDistance)
	    {
	       nearestNode = candidate;
	       nearestDistance = distance;
	    }
	 }
	 connectNodes(state, node, nearestNode);
	 ++connectedCount;
      }
   }

   // Walk the walk, islands are not allowed
   connectIslands(state);

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

void addPlayer(struct GameState* game, const char* name, const char* color, const char* playerSecret)
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
   strcpy(game->playerColor[game->playerCount-1], color);
   strcpy(game->playerSecret[game->playerCount-1], playerSecret);
}

void addOrder(struct GameState* game, unsigned type, unsigned from, unsigned to, const char* playerSecret)
{
   // Check that the player actually owns the "from" system
   unsigned playerId = game->controlledBy[from];
   if ( strcmp(game->playerSecret[playerId], playerSecret) ||
	( !nodesConnect(game, from, to) && game->controlledBy[to] != playerId ) )
   {
      return;
   }

   // Check that there's actually a unit there
   if (!game->occupied[from])
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
   if (state->occupied)
      free(state->occupied);
   if (state->occupiedInitial)
      free(state->occupiedInitial);
   if (state->homeWorld)
      free(state->homeWorld);
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
      if ( *(nodeRow + i) )
	 out[(*outSize)++] = i;
   }
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
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      fprintf(f, "%u\n", state->occupiedInitial[i]);
   }
   for (unsigned i = 0; i < state->nodeCount; ++i)
   {
      fprintf(f, "%u\n", state->homeWorld[i]);
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
      else
	 fprintf(f, "0\n"); // no turns now
   }

}

struct GameState deserialize(FILE* f)
{
   struct GameState state;
      
   fscanf(f, "%6[^\n]\n", state.id);
   state.gameName = malloc(64);
   fscanf(f, "%63[^\n]\n", state.gameName);
   fscanf(f, "%d\n", &state.metaGameState);
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
   state.occupied = calloc(sizeof(*(state.occupied)), state.nodeCount);
   state.occupiedInitial = calloc(sizeof(*(state.occupiedInitial)), state.nodeCount);
   state.homeWorld = calloc(sizeof(*(state.homeWorld)), state.nodeCount);

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
   for (unsigned i = 0; i < state.nodeCount; ++i)
   {
      fscanf(f, "%u\n", &state.occupiedInitial[i]);
   }
   for (unsigned i = 0; i < state.nodeCount; ++i)
   {
      fscanf(f, "%u\n", &state.homeWorld[i]);
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
