#ifndef CLIENT_H

#include "../common/game.h"
#include "../common/math/frame.h"
#include "../common/math/vec.h"

#define MAX_REQUEST_BODY_LEN 1024

struct ClientGame
{
   struct GameState state; // the actual game state, replicated from server
   Vec3* nodeScreenPositions; // 2d screen pixel positions of nodes (calculated per frame)
   unsigned* edgeBuffer; // a static buffer for passing edge-lists into JS-land without malloc
   float* supportBuffer; // a static buffer, where support strengths for the current turn are kept (for rendering)
   unsigned* orderCountBuffer; // a static buffer, where order counts for the current turn are kept (for rendering)
   
   // view
   Vec3 viewFocus;
   unsigned nodeFocus;
   float viewEulerX;
   float viewEulerY;
   float viewEulerXVel;
   float viewEulerYVel;
   float zoomFactor;

   int viewingTurn;

   int viewIsActive;

   // AJAX stuff
   char requestBodyBuffer[MAX_REQUEST_BODY_LEN];
   unsigned requestInFlight;
   void (*requestCallback) (const char* data, unsigned size); // Replace with queue if needed.
};

void makeAjaxRequest(const char* url, const char* method, const char* body,
		     void (*callback) (const char* data, unsigned size));

char* getCookie(const char* key); // EM_JS(char*, getCookie, (const char* key)

void setCookie(const char* key, const char* value); // EM_JS(void, setCookie, (const char* key, const char* value)

void sendOrder(enum OrderType type, unsigned from, unsigned to);

#endif
