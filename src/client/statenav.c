#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/fetch.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "statenav.h"
#include "../common/turnresolution.h"

extern struct ClientGame clientState; // client.c

static void receiveGames(const char* data, unsigned size)
{
   // Why, right? Well, reading the buffer directly means we have to deal with
   // incrementing the offset ourselves (we can't read it all in one go, we need
   // to know the count first). Letting the FILE* do the work makes it easier.
   FILE* f = fmemopen((void*)data, size, "r");
   if (!f)
      return;
   
   unsigned gameCount;
   fscanf(f, "%u\n", &gameCount);

   EM_ASM({
      mainMenu();
	});
   
   // Open games list
   for (unsigned i = 0; i < gameCount; ++i)
   {
      struct GameState game = deserialize(f);
      char* token = getCookie(game.id);
      int isInGame = token != NULL;
      free(token);
      EM_ASM({
         let gameId = UTF8ToString($2);
         let isInGame = $4;
         addGame({
            name: UTF8ToString($0),
            count: $1,
            id: gameId,
            state: $3,
            inGame: isInGame,
            join: isInGame ? null : (() => _receiveButtonClick(
               allocate(intArrayFromString('registerfor ' + gameId), ALLOC_NORMAL)
            )),
            view: () => _receiveButtonClick(allocate(intArrayFromString('enter ' + gameId), ALLOC_NORMAL))
         });
      }, game.gameName, game.playerCount, game.id, game.metaGameState, isInGame);
   }

   fclose(f);
}

static void receiveStartConfirm(const char* data, unsigned size)
{
   gotoState(GAME_SELECTION, 0);
}

static void receiveRegisterConfirm(const char* data, unsigned size)
{
   gotoState(SECRET_DISPLAY, data);
}

static void focusHomeWorld()
{
   char* playerSecret = getCookie(clientState.state.id);
   if (playerSecret)
   {
      for (unsigned i = 0; i < clientState.state.playerCount; ++i)
      {
	 if (!strcmp(playerSecret, clientState.state.playerSecret[i]))
	 {
	    // i is 'our' player id!
	    for (unsigned node = 0; node < clientState.state.nodeCount; ++node)
	    {
	       if (clientState.state.controlledBy[node] == i)
	       {
		  clientState.nodeFocus = node;
                  break;
	       }
	    }
	 }
      }
      free(playerSecret);
   }
}

static int playerIsAuthed()
{
   char* playerSecret = getCookie(clientState.state.id);
   if (playerSecret)
   {
      for (unsigned i = 0; i < clientState.state.playerCount; ++i)
      {
	 if (!strcmp(playerSecret, clientState.state.playerSecret[i]))
	 {
	    free(playerSecret);
	    return 1;
	 }
      }
      free(playerSecret);
   }
   return 0;
}

void receiveState(const char* data, unsigned size)
{
   FILE* f = fmemopen((void*)data, size, "r");
   if (!f)
      return;

   if (clientState.state.nodeCount)
   {
      freeGameState(&clientState.state);
      free(clientState.nodeScreenPositions);
      free(clientState.edgeBuffer);
   }
   
   clientState.state = deserialize(f);
   stepGameHistoryLatest(&clientState.state);
	 
   clientState.nodeScreenPositions = malloc(sizeof(*(clientState.nodeScreenPositions)) *
					    clientState.state.nodeCount);

   clientState.edgeBuffer = malloc(sizeof(*(clientState.edgeBuffer)) *
				   clientState.state.nodeCount);
   
   fclose(f);

   // clear the control area
   EM_ASM(
      {
	 var controlArea = document.getElementById("control");
	 controlArea.innerHTML = "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('list'), ALLOC_NORMAL))\">Game selection</button>";
	 controlArea.innerHTML += "&nbsp;<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyp'), ALLOC_NORMAL))\">&lt</button>";
	 controlArea.innerHTML += "<input type=\"text\" id=\"turnInput\" size=\"4\" value=\"" + $0 + "\"/>";
	 controlArea.innerHTML += "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyn'), ALLOC_NORMAL))\">&gt</button>";
	 controlArea.innerHTML += "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyl'), ALLOC_NORMAL))\">&gt&gt</button>";
	 controlArea.innerHTML += "<button type=\"button\" onClick=\"window.open('rules.html');\">Read rules</button>";
	 if ($1 == 0)
	 {
	    controlArea.innerHTML += "<input type=\"text\" placeholder=\"Enter secret to rejoin\" id=\"secretInput\" onkeypress=\"javascript: if(event.keyCode == 13){_receiveButtonClick(allocate(intArrayFromString('setsecret ' + event.target.value), ALLOC_NORMAL));}\"/>";
	 }
      }, clientState.state.turnCount-1, playerIsAuthed());

   clientState.viewingTurn = clientState.state.turnCount-1;

   // If you are a player of this game, start out focusing your own home world
   // If you're receiving state, but were already looking at the game, do not change focus!
   if (!clientState.viewIsActive)
   {
      focusHomeWorld();
   }

   clientState.viewIsActive = 1;
}


EMSCRIPTEN_KEEPALIVE
void receiveButtonClick(char* value)
{
   printf("GOT CLICK COMMAND: \"%s\"\n", value);
   
   if (!strcmp(value, "list"))
   {
      gotoState(GAME_SELECTION, 0);
   }
   else if (!strncmp(value, "enter ", strlen("enter ")))
   {
      char* gameId = value + strlen("enter ");
      gotoState(IN_GAME, gameId);
   }
   else if (!strncmp(value, "registerfor ", strlen("registerfor ")))
   {
      char* gameId = value + strlen("registerfor ");
      gotoState(REGISTER_FOR, gameId);
   }
   else if (!strncmp(value, "registerconfirm ", strlen("registerconfirm ")))
   { 
      char* registerString = value + strlen("registerconfirm "); // like so: KWFEUF #0000ff My Name
      makeAjaxRequest("/cgi-bin/server/register", "POST", registerString, receiveRegisterConfirm);
   }
   else if (!strncmp(value, "historyn", strlen("historyn")))
   {
      clientState.viewingTurn = EM_ASM_INT(
         {
            var element = document.getElementById("turnInput");
            var value = parseInt(element.value) + 1;
            if (value <= $0)
               element.value = value;
            else
               element.value = $0;
            return element.value;
         }, clientState.state.turnCount-1);

      stepGameHistory(&clientState.state, clientState.viewingTurn);
   }
   else if (!strncmp(value, "historyp", strlen("historyp")))
   {
      clientState.viewingTurn = EM_ASM_INT(
         {
            var element = document.getElementById("turnInput");
            var value = parseInt(element.value) - 1;
            if (value > -1)
               element.value = value;
            else
               element.value = 0;
            return element.value;
         });

      stepGameHistory(&clientState.state, clientState.viewingTurn);
   }
   else if (!strncmp(value, "historyl", strlen("historyl")))
   {
      clientState.viewingTurn = EM_ASM_INT(
         {
            var element = document.getElementById("turnInput");
            element.value = $0;
            return element.value;
         }, clientState.state.turnCount-1);

      stepGameHistory(&clientState.state, clientState.viewingTurn);
   }
   else if (!strncmp(value, "setsecret ", strlen("setsecret ")))
   {
      const char* secret = value + strlen("setsecret ");
      setCookie(clientState.state.id, secret);
      clientState.viewIsActive = 0;
      gotoState(IN_GAME, clientState.state.id);
   }

   free(value);
}

void gotoState(enum StateTarget target, const char* parameter)
{
   // Set control div position
   if (target == IN_GAME)
   {
      EM_ASM(
	 {
	    // Both globally defined in index.html
       gameMenu();
	 });
   }
   else
   {
      EM_ASM(
	 {
       mainMenu();
	 });
   }

   
   if (target == GAME_SELECTION)
   {
      makeAjaxRequest("/cgi-bin/server/games", "GET", NULL, receiveGames);
   }
   else if (target == IN_GAME)
   {
      char path[128] = {0};
      snprintf(path, 127, "/cgi-bin/server/state/%s", parameter);
      char* playerSecret = getCookie(parameter);
      clientState.viewIsActive = 0;
      makeAjaxRequest(path, "POST", playerSecret, receiveState);
      if (playerSecret)
	 free(playerSecret);
   }
   else if (target == GAME_CREATION)
   {
      EM_ASM(
         {
            var controlArea = document.getElementById("control");
	 
            controlArea.innerHTML = " \
<input type=\"text\" id=\"gameNameInput\"></input> \
<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('createdone'), ALLOC_NORMAL))\">Create new game</button>";
         });
   }
   else if (target == START_CONFIRM)
   {
      EM_ASM(
         {
            var controlArea = document.getElementById("control");
	 
            controlArea.innerHTML = "\
Please confirm game start. Once the game has started, new players will no longer be able to join.<br/> \
<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('startconfirm "+ UTF8ToString($0) +"'), ALLOC_NORMAL))\">Confirm</button>\
<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('list'), ALLOC_NORMAL))\">Cancel</button>";
         }, parameter);
   }
   else if (target == REGISTER_FOR)
   {
      EM_ASM(
         {
            registerFor({
               register: (color, name) => {
                  _receiveButtonClick(
                     allocate(
                        intArrayFromString([
                           "registerconfirm",
                           UTF8ToString($0),
                           color,
                           name
                        ].join(" ")),
                        ALLOC_NORMAL
                     )
                  );
               },
               cancel: () => _receiveButtonClick(
                  allocate(
                     intArrayFromString('list'),
                     ALLOC_NORMAL
                  )
               )
            });
         }, parameter);
   }
   else if (target == SECRET_DISPLAY)
   {
      EM_ASM(
         {
            let parts = UTF8ToString($0).split("\n");
            let gameId = parts[0];
            let secret = parts[1];
            document.cookie = gameId + "=" + secret + "; Max-Age=8640000; SameSite=Strict";
            displaySecret({
               secret: secret,
               gameId: gameId,
               action: () => _receiveButtonClick(
                  allocate(
                     intArrayFromString('list'),
                     ALLOC_NORMAL
                  )
               ),
            });
         }, parameter);
   }
}
