#include <stdio.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "../common/game.h"
#include "../common/turnresolution.h"

static int lockFd;

static void unlock()
{
   if (flock(lockFd, LOCK_UN))
   {
      printf("File unlock error.");
      exit(-1);
   }
   close(lockFd);
}

static void ensureOnlyInstance()
{
   lockFd = open("/tmp/officewars.lock", O_RDWR | O_CREAT, S_IRWXU);
   if (lockFd == -1)
   {
      printf("File open error.");
      exit(-1);
   }
   if (flock(lockFd, LOCK_EX))
   {
      printf("File lock error.");
      exit(-1);
   }
   atexit(unlock);
}

// Generate a 6 char key, to use as an access key or game id
static void generateKey(char key[7])
{
   const char* availableChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
   const int availableLen = strlen(availableChars);
   for (int i = 0; i < 6; ++i)
   {
      key[i] = availableChars[rand() % availableLen];
   }
   key[6] = '\0';
}

static void exitWithError(int code)
{
   fprintf(stderr, "Controlled error out with code %d\n", code);
   printf("Status: %d got some problems..\n\n", code);
   exit(-1);
}

static void createNewGameFile(const char* gameName)
{
   if (strlen(gameName) > 63 || strlen(gameName) < 1)
      exitWithError(400);
   FILE* gameFile = NULL;
   char id[7];
   while (!gameFile)
   {
      generateKey(id);  
      gameFile = fopen(id, "wx");
      if (!gameFile && errno != EEXIST) exitWithError(500);
   }

   struct GameState game = initatePreGame(gameName);
   strcpy(game.id, id);

   serialize(&game, -1, gameFile);

   fclose(gameFile);
}

static int validateId(const char* id)
{
   if (strlen(id) != 6)
      return 0;
   for (int i = 0; i < 6; ++i)
   {
      if (id[i] < 65 || id[i] > 90) // A through Z
         return 0;
   }
   return 1;
}

static struct GameState loadGame(const char id[7])
{
   if (!validateId(id))
      exitWithError(400);
   FILE* gameFile = fopen(id, "r");
   if (!gameFile)
   {
      exitWithError(500);
   }
   struct GameState game = deserialize(gameFile);
   fclose(gameFile);
   return game;
}

static void saveAndCloseGame(struct GameState* game, const char id[7])
{
   FILE* newGameFile = fopen(id, "w");
   serialize(game, -1, newGameFile);
   fclose(newGameFile);
   freeGameState(game);
}

static void tickAllGames()
{
   DIR *d;
   struct dirent *dir;
   d = opendir(".");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if (strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)) // ignore . and ..
         {
            struct GameState game = loadGame(dir->d_name);
            tickGame(&game);
            saveAndCloseGame(&game, dir->d_name);
         }
      }
      closedir(d);
   }
}

static void addRandomAI()
{
   DIR *d;
   struct dirent *dir;
   d = opendir(".");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if (strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)) // ignore . and ..
         {
            struct GameState game = loadGame(dir->d_name);

            char playerSecret[7] = {0};
            generateKey(playerSecret);
            char name[32] = {0};
            char color[8] = {0};
            snprintf(name, 31, "random%d", rand());
            snprintf(color, 8, "#%02x%02x%02x", rand()%256, rand()%256, rand()%256);
            addPlayer(&game, name, color, playerSecret);

            saveAndCloseGame(&game, dir->d_name);
         }
      }
      closedir(d);
   }
}

static void moveRandomAI()
{
   DIR *d;
   struct dirent *dir;
   d = opendir(".");
   if (d)
   {
      while ((dir = readdir(d)) != NULL)
      {
         if (strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)) // ignore . and ..
         {
            struct GameState game = loadGame(dir->d_name);
            stepGameHistoryLatest(&game);

            for (unsigned node = 0; node < game.nodeCount; ++node)
            {
               unsigned playerId = game.controlledBy[node];
               if (playerId != 4294967295)
               {
                  const char* name = game.playerName[playerId];
                  if (strncmp(name, "random", strlen("random")))
                     continue;
		  
                  char* secret = game.playerSecret[playerId];

                  unsigned connected[game.nodeCount];
                  unsigned connectedCount = 0;
                  getConnectedNodes(&game, node, connected, &connectedCount);
                  unsigned randomTarget = connected[rand() % connectedCount];

                  unsigned type = 0;

                  //printf("Doing order from %u to %u by %u with secret %s\n", node, randomTarget, playerId, secret);
		  
                  addOrder(&game, type, node, randomTarget, secret);
               }
            }
	    
            saveAndCloseGame(&game, dir->d_name);
         }
      }
      closedir(d);
   }
}

int main (int argc, char** argv)
{
   //system("pwd 1>&2 ; whoami 1>&2");
   ensureOnlyInstance();
   
   // Create games directory if not already done, and cd to it
   struct stat st = {0};
   if (stat("./games", &st) == -1) {
      if (mkdir("./games", 0755) != 0) exitWithError(500);
   }
   if (chdir("./games") != 0) exitWithError(500);

   if (argc == 2 && !strcmp(argv[1], "tick"))
   {
      tickAllGames();
      return 0;
   }

   if (argc == 3 && !strcmp(argv[1], "tick"))
   {
      struct GameState game = loadGame(argv[2]);
      tickGame(&game);
      saveAndCloseGame(&game, argv[2]);
      return 0;
   }

   srand(time(NULL));

   // For dev/debug, add "random action AI".
   if (argc == 3 && !strcmp(argv[1], "randomadd"))
   {
      int count = atoi(argv[2]);
      for (int i = 0; i < count; ++i)
         addRandomAI();
      return 0;
   }
   if (argc == 2 && !strcmp(argv[1], "randommove"))
   {
      moveRandomAI();
      return 0;
   }

   // Read the request
   const char* method = getenv("REQUEST_METHOD");
   
   const char* path = getenv("PATH_INFO");
   int contentLength = 0;
   if (getenv("CONTENT_LENGTH"))
      contentLength = strtol(getenv("CONTENT_LENGTH"), NULL, 10);
   
   if (contentLength > 1024)
   {
      exitWithError(400);
   }
   char data[contentLength+1];
   data[contentLength] = '\0';
   fread(data, 1, contentLength, stdin);
   
   // Interpret and respond
   if (!strcmp(path, "/games"))
   {
      if (!strcmp(method, "POST"))
      {
         createNewGameFile(data);
      }

      // Respond regardless of the method
      printf("Content-Type: text/plain\n\n");	    

      // First, the number of games
      DIR *d;
      struct dirent *dir;
      unsigned gameCount = 0;
      d = opendir(".");
      if (d)
      {
         while ((dir = readdir(d)) != NULL)
         {
            if (strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)) // ignore . and ..
               ++gameCount;
         }
         closedir(d);
      }
      printf("%u\n", gameCount); // number of games in list

      // Then, for each game
      d = opendir(".");
      if (d)
      {
         while ((dir = readdir(d)) != NULL)
         {
            if (strncmp(dir->d_name, ".", 1) && strncmp(dir->d_name, "..", 2)) // ignore . and ..
            {
               struct GameState game = loadGame(dir->d_name);
               serialize(&game, -2, stdout); // no secrets
               freeGameState(&game);
            }
         }
         closedir(d);
      }
   }
   else if (!strncmp(path, "/state/", strlen("/state/")))
   {
      const char* id = path + strlen("/state/");
      const char* playerSecret = data;
      struct GameState game = loadGame(id);
      unsigned serializationFor = -2; // share no secrets
      if (validateId(playerSecret)) // If there's an authed player, let them see their own moves
      {
         for (unsigned i = 0; i < game.playerCount; ++i)
         {
            if (!strcmp(playerSecret, game.playerSecret[i]))
            {
               serializationFor = i;
            }
         }
      }
      
      printf("Content-Type: text/plain\n\n");
      serialize(&game, serializationFor, stdout);
      freeGameState(&game);
   }
   else if (!strncmp(path, "/register", strlen("/register")))
   {
      // Read input, like so: KWFEUF #0000ff Their Name
      FILE* f = fmemopen((void*)data, contentLength, "r");
      if (!f)
         return 0;
      char gameId[7] = {0};
      fscanf(f, "%6s ", gameId);
      char color[8] = {0};
      fscanf(f, "%7s ", color);
      char name[64] = {0};
      fscanf(f, "%63[^\n]", name);
      fclose(f);

      // Add player to game
      char playerSecret[7] = {0};
      generateKey(playerSecret);
      struct GameState game = loadGame(gameId);
      if (strlen(name) < 2 || strlen(name) > 63)
         exitWithError(400);
      addPlayer(&game, name, color, playerSecret);
      saveAndCloseGame(&game, gameId);
      printf("Content-Type: text/plain\n\n");
      printf("%s\n%s\n", gameId, playerSecret);
   }
   else if (!strncmp(path, "/start", strlen("/start")))
   {
      struct GameState game = loadGame(data);
      startGame(&game);
      printf("Content-Type: text/plain\n\n");
      printf("Start OK.\n");
      saveAndCloseGame(&game, data);
   }
   else if (!strcmp(path, "/orders"))
   {
      if (!strcmp(method, "POST"))
      {
         FILE* f = fmemopen((void*)data, contentLength, "r");
         if (!f)
	    return 0;
         unsigned type;
         unsigned from;
         unsigned to;
         char gameId[7] = {0};
         char playerSecret[7] = {0};

         fscanf(f, "%u\n%u\n%u\n%6[^\n]\n%6[^\n]\n", &type, &from, &to, gameId, playerSecret);
	 
         if (!validateId(playerSecret))
         {
            fclose(f);
            exitWithError(400);
         }
         fclose(f);

         // Open data file and add order
         struct GameState game = loadGame(gameId);

         stepGameHistoryLatest(&game);

         addOrder(&game, type, from, to, playerSecret);

         unsigned playerId = game.controlledBy[from];
         if (strcmp(game.playerSecret[playerId], playerSecret))
            playerId = -2;

         printf("Content-Type: text/plain\n\n");
         serialize(&game, playerId, stdout);

         saveAndCloseGame(&game, gameId);
      }
   }
   else
   {
      exitWithError(400);
   }
   
   return 0;
}
