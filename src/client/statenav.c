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

   
   char htmlBuf[4096] = {0};
   FILE* htmlF = fmemopen((void*)htmlBuf, 4095, "w");
   if (!htmlF)
      return;

   // Open games list
   for (unsigned i = 0; i < gameCount; ++i)
   {
      struct GameState game = deserialize(f);

      switch (game.metaGameState)
      {
	 case PREGAME:
	 {
            fprintf(htmlF, " \
<div class=\"game-entry\"> \
  <b>Open to join: </b>%s<br/><b>%u</b> players have joined<br/>", game.gameName, game.playerCount);

            char* token = getCookie(game.id);
            if (!token)
            {
               fprintf(htmlF, "<button \
    type=\"button\" \
    onClick=\"_receiveButtonClick(allocate(intArrayFromString('registerfor %s'), ALLOC_NORMAL))\">\
      Join game \
  </button>", game.id);
            }
            else
            {
               free(token);
               fprintf(htmlF, "You have already joined this game.");
            }
            fprintf(htmlF, "</div>");
	    break;
	 }
	    
	 case INGAME:
	 {
	    fprintf(htmlF, " \
<div class=\"game-entry\"> \
  <b>In-game: </b>%s<br/><b>%u</b> players<br/> \
  <button \
    type=\"button\" \
    onClick=\"_receiveButtonClick(allocate(intArrayFromString('enter %s'), ALLOC_NORMAL))\">\
      Enter/observe \
  </button> \
</div>", game.gameName, game.playerCount, game.id);
	    break;
	 }

         case POSTGAME:
	 {
            char* winningName = "INDECIVSIE";
            if (game.winningPlayer != UINT_MAX)
               winningName = game.playerName[game.winningPlayer];
            
	    fprintf(htmlF, " \
<div class=\"game-entry\"> \
  <b>Finished: </b>%s<br/><b>%u</b> players<br/> Won by: %s<br/> \
  <button \
    type=\"button\" \
    onClick=\"_receiveButtonClick(allocate(intArrayFromString('enter %s'), ALLOC_NORMAL))\">\
      Review history \
  </button> \
</div>", game.gameName, game.playerCount, winningName, game.id);
	    break;
	 }
      }

   }

   fclose(f);
   fclose(htmlF);
   
   EM_ASM(
      {
	 var controlArea = document.getElementById("control");
	 controlArea.innerHTML = UTF8ToString($0);
      }, htmlBuf
      );

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
   // Deserialize game state
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
   }

   // Construct control panel
   {
      struct GameState* game = &clientState.state;
      char* playerSecret = getCookie(game->id);

      // Find our player ID
      unsigned playerId;
      for (unsigned player = 0; player < game->playerCount; ++player)
      {
         if (!strcmp(game->playerSecret[player], playerSecret))
            playerId = player;
      }

      // Find pre-existing surrenderorders, if any
      struct Turn* turn = & game->turn[game->turnCount-1];
      unsigned surrenderingTo = UINT_MAX; // = not surrendering
      for (unsigned order = 0; order < turn->orderCount; ++order)
      {
         if ( turn->issuingPlayer[order] == playerId && turn->type[order] == SURRENDERORDER )
         {
            surrenderingTo = turn->toNode[order];
         }
      }

      // Generate surrender-to html
      unsigned nodesPerPlayer[game->playerCount];
      memset(nodesPerPlayer, 0, sizeof(nodesPerPlayer[0]) * game->playerCount);
      for (unsigned node = 0; node < game->nodeCount; ++node)
      {
         if (game->controlledBy[node] != UINT_MAX)
         {
            nodesPerPlayer[game->controlledBy[node]]++;
         }
      }
      char surrenderOptionsBuf[512*game->playerCount];
      unsigned pos = 0;
      pos += sprintf(surrenderOptionsBuf + pos, "<select onchange=\"_receiveButtonClick(allocate(intArrayFromString('surrender '+this.value), ALLOC_NORMAL))\">");
      if (surrenderingTo == UINT_MAX)
         pos += sprintf(surrenderOptionsBuf + pos, "<option selected=\"selected\" value=\"-1\">Not surrendering</option>");
      else
         pos += sprintf(surrenderOptionsBuf + pos, "<option value=\"-1\">Not surrendering</option>");
      for (unsigned player = 0; player < game->playerCount; ++player)
      {
         // Surviving players that aren't me
         if (nodesPerPlayer[player] > 0 && strcmp(game->playerSecret[player], playerSecret))
         {
            if (surrenderingTo == player)
               pos += sprintf(surrenderOptionsBuf + pos, "<option value=\"%d\" selected=\"selected\">Surrender to %s</option>", player, game->playerName[player]);
            else
               pos += sprintf(surrenderOptionsBuf + pos, "<option value=\"%d\">Surrender to %s</option>", player, game->playerName[player]);
         }
      }
      pos += sprintf(surrenderOptionsBuf + pos, "/<select>");
      free(playerSecret);
   
      // Reset the control area
      EM_ASM(
         {
            var controlArea = document.getElementById("control");
            controlArea.innerHTML = "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('list'), ALLOC_NORMAL))\">Game selection</button>";
            controlArea.innerHTML += "&nbsp;<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyp'), ALLOC_NORMAL))\">&lt</button>";
            controlArea.innerHTML += "<input type=\"text\" id=\"turnInput\" size=\"4\" value=\"" + $0 + "\"/>";
            controlArea.innerHTML += "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyn'), ALLOC_NORMAL))\">&gt</button>";
            controlArea.innerHTML += "<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('historyl'), ALLOC_NORMAL))\">&gt&gt</button>";
            controlArea.innerHTML += "&nbsp;<button type=\"button\" onClick=\"window.open('rules.html');\">Read rules</button>&nbsp;";
            if ($1 == 0)
            {
               controlArea.innerHTML += "<input type=\"text\" placeholder=\"Enter secret to rejoin\" id=\"secretInput\" onkeypress=\"javascript: if(event.keyCode == 13){_receiveButtonClick(allocate(intArrayFromString('setsecret ' + event.target.value), ALLOC_NORMAL));}\"/>";
            }
            else
            {
               controlArea.innerHTML += UTF8ToString($2);
            }
         }, clientState.state.turnCount-1, playerIsAuthed(), surrenderOptionsBuf);
   }

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
   else if (!strncmp(value, "surrender ", strlen("surrender ")))
   {
      int surrenderTo = strtol(value + strlen("surrender "), NULL, 10);
      sendOrder(SURRENDERORDER, UINT_MAX, surrenderTo);
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
	    controlPosition = "side menu";
	    resize();
	 });
   }
   else
   {
      EM_ASM(
	 {
	    // Both globally defined in index.html
	    controlPosition = "main menu";
	    resize();
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
            var controlArea = document.getElementById("control");
            var n = "0123456789ABCDEF";
            var colorPreset = "#";// + (Math.random()*0xFFFFFF).toString(16);
            for (var i = 0; i < 6; i++)
            {
               colorPreset += n[Math.floor(Math.random() * 16)];
            }
	 
            controlArea.innerHTML = "\
<div class=\"info\">Enter a name for your galactic empire and choose a color for your banner. Do NOT use a name that can identify you as a person.</div>\
<div class=\"config\">\
<input type=\"text\" id=\"playerName\" placeholder=\"Solar republic\" name=\"name\"/><label for=\"playerName\">Name of your empire</label>\
<input type=\"color\" id=\"playerColor\" name=\"color\" value=\""+colorPreset+"\"/><label for=\"playerColor\">Color</label>\
</div>\
<div class=\"footer\">\
<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('registerconfirm "+ UTF8ToString($0) +" ' + document.getElementById('playerColor').value + ' ' + document.getElementById('playerName').value ), ALLOC_NORMAL))\">Join</button>\
<button type=\"button\" onClick=\"_receiveButtonClick(allocate(intArrayFromString('list'), ALLOC_NORMAL))\">Cancel</button>\
</div>";
         }, parameter);
   }
   else if (target == SECRET_DISPLAY)
   {
      EM_ASM(
         {
            var controlArea = document.getElementById("control");
            var parts = UTF8ToString($0).split("\n");
            var gameId = parts[0];
            var secret = parts[1];
            controlArea.innerHTML = " \
<div class=\"info\">\
Your access token for this game is: <span class=\"secret\">" + secret + "</span> \
Please save it, or write it down! When you click the button below, this token and the ID of the game you joined (but nothing else) will be stored in a persistent first party cookie (for 60 days) in your browser. The game cannot function without this cookie. If you do not wish such a cookie to be set, please close this window without clicking the button below. You will need to re-enter this token to get back into the game if your cookie is lost or if you wish to play from another device.\
</div>\
<div class=\"footer\">\
<button type=\"button\" onClick=\"setCookieSecret('"+gameId+"', '"+secret+"');_receiveButtonClick(allocate(intArrayFromString('list'), ALLOC_NORMAL))\">I'm done, write my token to a cookie!</button> \
</div>";
         }, parameter);
   }
}
